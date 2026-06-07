/*
 * Prototype simulator for QoS-safe RAN cell sleep experiments.
 *
 * This scratch program is intentionally self-contained. It compares simple
 * cell-sleep controllers and writes CSV files that can be used directly for
 * early paper plots.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <limits>
#include <map>
#include <sstream>
#include <string>
#include <utility>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UnoUmbDtEnergy");

enum class PolicyMode
{
    AllOn,
    Threshold,
    Aggressive,
    Twin
};

struct TwinEstimate
{
    bool safe{false};
    double maxUtilization{0.0};
    double minCoverageMarginDb{0.0};
    double utilizationUncertainty{0.0};
    double coverageUncertaintyDb{0.0};
};

static PolicyMode
ParsePolicy(const std::string& policy)
{
    if (policy == "all-on")
    {
        return PolicyMode::AllOn;
    }
    if (policy == "threshold")
    {
        return PolicyMode::Threshold;
    }
    if (policy == "aggressive")
    {
        return PolicyMode::Aggressive;
    }
    if (policy == "twin")
    {
        return PolicyMode::Twin;
    }
    NS_ABORT_MSG("Unknown policy '" << policy
                                   << "'. Use all-on, threshold, aggressive, or twin.");
}

static double
DistanceMeters(Ptr<Node> a, Ptr<Node> b)
{
    return CalculateDistance(a->GetObject<MobilityModel>()->GetPosition(),
                             b->GetObject<MobilityModel>()->GetPosition());
}

static double
EstimateRsrpDbm(double txPowerDbm, double distanceMeters)
{
    const double distanceKm = std::max(distanceMeters / 1000.0, 0.01);
    const double pathLossDb = 128.1 + 37.6 * std::log10(distanceKm);
    return txPowerDbm - pathLossDb;
}

static std::vector<uint32_t>
BuildHomeCells(uint32_t numberOfUes, uint32_t numberOfEnbs, uint32_t lowLoadUes)
{
    std::vector<uint32_t> home(numberOfUes, 0);
    if (numberOfEnbs == 3 && numberOfUes > lowLoadUes)
    {
        const uint32_t leftCount = (numberOfUes - lowLoadUes) / 2;
        for (uint32_t u = 0; u < numberOfUes; ++u)
        {
            if (u < leftCount)
            {
                home[u] = 0;
            }
            else if (u < leftCount + lowLoadUes)
            {
                home[u] = 1;
            }
            else
            {
                home[u] = 2;
            }
        }
        return home;
    }

    for (uint32_t u = 0; u < numberOfUes; ++u)
    {
        home[u] = u % numberOfEnbs;
    }
    return home;
}

class SleepController
{
  public:
    SleepController(PolicyMode policy,
                    std::string policyName,
                    Ptr<LteHelper> lteHelper,
                    NodeContainer enbNodes,
                    NodeContainer ueNodes,
                    NetDeviceContainer enbDevs,
                    NetDeviceContainer ueDevs,
                    std::vector<uint32_t> servingEnb,
                    Time simTime,
                    Time controlStart,
                    Time controlInterval,
                    double ueRateMbps,
                    double activeTxPowerDbm,
                    double sleepTxPowerDbm,
                    double cellCapacityMbps,
                    double sleepUeThreshold,
                    double aggressiveSleepUeThreshold,
                    uint32_t minActiveEnbs,
                    double minReliableRsrpDbm,
                    double requiredCoverageMarginDb,
                    double utilizationLimit,
                    double uncertaintyScale,
                    double activePowerW,
                    double sleepPowerW,
                    std::ofstream* eventCsv)
        : m_policy(policy),
          m_policyName(std::move(policyName)),
          m_lteHelper(lteHelper),
          m_enbNodes(enbNodes),
          m_ueNodes(ueNodes),
          m_enbDevs(enbDevs),
          m_ueDevs(ueDevs),
          m_servingEnb(std::move(servingEnb)),
          m_simTime(simTime),
          m_controlStart(controlStart),
          m_controlInterval(controlInterval),
          m_ueRateMbps(ueRateMbps),
          m_activeTxPowerDbm(activeTxPowerDbm),
          m_sleepTxPowerDbm(sleepTxPowerDbm),
          m_cellCapacityMbps(cellCapacityMbps),
          m_sleepUeThreshold(sleepUeThreshold),
          m_aggressiveSleepUeThreshold(aggressiveSleepUeThreshold),
          m_minActiveEnbs(minActiveEnbs),
          m_minReliableRsrpDbm(minReliableRsrpDbm),
          m_requiredCoverageMarginDb(requiredCoverageMarginDb),
          m_utilizationLimit(utilizationLimit),
          m_uncertaintyScale(uncertaintyScale),
          m_activePowerW(activePowerW),
          m_sleepPowerW(sleepPowerW),
          m_eventCsv(eventCsv),
          m_active(enbNodes.GetN(), true)
    {
    }

    void Start()
    {
        Simulator::Schedule(m_controlStart, &SleepController::RunPolicy, this);
    }

    void FinalizeEnergy()
    {
        AccumulateEnergy(m_simTime.GetSeconds());
    }

    double GetEnergyJ() const
    {
        return m_energyJ;
    }

    double GetActiveCellSeconds() const
    {
        return m_activeCellSeconds;
    }

    uint32_t GetUnsafeSleepActions() const
    {
        return m_unsafeSleepActions;
    }

    uint32_t GetHandoverRequests() const
    {
        return m_handoverRequests;
    }

  private:
    void RunPolicy()
    {
        const double now = Simulator::Now().GetSeconds();
        AccumulateEnergy(now);

        std::vector<double> loadMbps = ComputeLoadMbps();
        std::vector<bool> desiredActive(m_active.size(), true);

        if (m_policy != PolicyMode::AllOn)
        {
            ApplySleepSelection(loadMbps, desiredActive);
        }

        for (uint32_t cell = 0; cell < desiredActive.size(); ++cell)
        {
            if (desiredActive[cell] && !m_active[cell])
            {
                SetEnbTxPower(cell, m_activeTxPowerDbm);
                m_active[cell] = true;
                WriteDecision(cell, "wake", "policy-selected-active", loadMbps[cell], TwinEstimate());
            }
            else if (!desiredActive[cell] && m_active[cell])
            {
                const TwinEstimate estimate = EstimateSleepRisk(cell, desiredActive, loadMbps);
                if (!estimate.safe && m_policy != PolicyMode::Twin)
                {
                    ++m_unsafeSleepActions;
                }
                OffloadCell(cell, desiredActive);
                Simulator::Schedule(MilliSeconds(80),
                                    &SleepController::SetEnbTxPower,
                                    this,
                                    cell,
                                    m_sleepTxPowerDbm);
                m_active[cell] = false;
                WriteDecision(cell, "sleep", "policy-selected-sleep", loadMbps[cell], estimate);
            }
            else
            {
                WriteDecision(cell,
                              desiredActive[cell] ? "keep-active" : "keep-sleep",
                              "no-state-change",
                              loadMbps[cell],
                              EstimateSleepRisk(cell, desiredActive, loadMbps));
            }
        }

        if (Simulator::Now() + m_controlInterval < m_simTime)
        {
            Simulator::Schedule(m_controlInterval, &SleepController::RunPolicy, this);
        }
    }

    void ApplySleepSelection(const std::vector<double>& loadMbps, std::vector<bool>& desiredActive)
    {
        std::vector<uint32_t> order(loadMbps.size());
        for (uint32_t i = 0; i < order.size(); ++i)
        {
            order[i] = i;
        }
        std::sort(order.begin(), order.end(), [&loadMbps](uint32_t a, uint32_t b) {
            return loadMbps[a] < loadMbps[b];
        });

        uint32_t activeCount = desiredActive.size();
        const double threshold = (m_policy == PolicyMode::Aggressive) ? m_aggressiveSleepUeThreshold
                                                                      : m_sleepUeThreshold;

        for (uint32_t cell : order)
        {
            const double servedUes = loadMbps[cell] / m_ueRateMbps;
            if (servedUes > threshold || activeCount <= m_minActiveEnbs)
            {
                continue;
            }

            if (m_policy == PolicyMode::Twin)
            {
                TwinEstimate estimate = EstimateSleepRisk(cell, desiredActive, loadMbps);
                if (!estimate.safe)
                {
                    continue;
                }
            }

            desiredActive[cell] = false;
            --activeCount;
        }
    }

    std::vector<double> ComputeLoadMbps() const
    {
        std::vector<double> load(m_enbNodes.GetN(), 0.0);
        for (uint32_t u = 0; u < m_servingEnb.size(); ++u)
        {
            load[m_servingEnb[u]] += m_ueRateMbps;
        }
        return load;
    }

    TwinEstimate EstimateSleepRisk(uint32_t candidateCell,
                                   const std::vector<bool>& desiredActive,
                                   const std::vector<double>& currentLoadMbps) const
    {
        TwinEstimate estimate;
        std::vector<bool> activeAfter = desiredActive;
        activeAfter[candidateCell] = false;

        std::vector<double> loadAfter = currentLoadMbps;
        loadAfter[candidateCell] = 0.0;
        double farthestOffloadMeters = 0.0;
        estimate.minCoverageMarginDb = std::numeric_limits<double>::max();

        for (uint32_t u = 0; u < m_servingEnb.size(); ++u)
        {
            if (m_servingEnb[u] != candidateCell)
            {
                continue;
            }

            const uint32_t target = FindNearestActiveEnb(u, activeAfter, candidateCell);
            if (target == candidateCell)
            {
                estimate.safe = false;
                estimate.minCoverageMarginDb = -std::numeric_limits<double>::infinity();
                return estimate;
            }

            loadAfter[target] += m_ueRateMbps;
            const double distance = DistanceMeters(m_ueNodes.Get(u), m_enbNodes.Get(target));
            farthestOffloadMeters = std::max(farthestOffloadMeters, distance);
            const double rsrpDbm = EstimateRsrpDbm(m_activeTxPowerDbm, distance);
            estimate.minCoverageMarginDb =
                std::min(estimate.minCoverageMarginDb, rsrpDbm - m_minReliableRsrpDbm);
        }

        if (estimate.minCoverageMarginDb == std::numeric_limits<double>::max())
        {
            estimate.minCoverageMarginDb = 99.0;
        }

        for (uint32_t cell = 0; cell < loadAfter.size(); ++cell)
        {
            if (activeAfter[cell])
            {
                estimate.maxUtilization =
                    std::max(estimate.maxUtilization, loadAfter[cell] / m_cellCapacityMbps);
            }
        }

        estimate.utilizationUncertainty =
            m_uncertaintyScale * (0.04 + 0.10 * estimate.maxUtilization +
                                  0.06 * (farthestOffloadMeters / 1000.0));
        estimate.coverageUncertaintyDb =
            m_uncertaintyScale * (1.0 + 2.0 * (farthestOffloadMeters / 1000.0));
        estimate.safe =
            (estimate.maxUtilization + estimate.utilizationUncertainty <= m_utilizationLimit) &&
            (estimate.minCoverageMarginDb - estimate.coverageUncertaintyDb >=
             m_requiredCoverageMarginDb);
        return estimate;
    }

    uint32_t FindNearestActiveEnb(uint32_t ueIndex,
                                  const std::vector<bool>& active,
                                  uint32_t avoidCell) const
    {
        uint32_t bestCell = avoidCell;
        double bestDistance = std::numeric_limits<double>::max();
        for (uint32_t cell = 0; cell < active.size(); ++cell)
        {
            if (!active[cell] || cell == avoidCell)
            {
                continue;
            }

            const double distance = DistanceMeters(m_ueNodes.Get(ueIndex), m_enbNodes.Get(cell));
            if (distance < bestDistance)
            {
                bestDistance = distance;
                bestCell = cell;
            }
        }
        return bestCell;
    }

    void OffloadCell(uint32_t sleepingCell, const std::vector<bool>& desiredActive)
    {
        for (uint32_t u = 0; u < m_servingEnb.size(); ++u)
        {
            if (m_servingEnb[u] != sleepingCell)
            {
                continue;
            }

            const uint32_t target = FindNearestActiveEnb(u, desiredActive, sleepingCell);
            if (target == sleepingCell)
            {
                continue;
            }

            m_lteHelper->HandoverRequest(Simulator::Now() + MilliSeconds(10),
                                         m_ueDevs.Get(u),
                                         m_enbDevs.Get(sleepingCell),
                                         m_enbDevs.Get(target));
            m_servingEnb[u] = target;
            ++m_handoverRequests;
        }
    }

    void SetEnbTxPower(uint32_t cell, double txPowerDbm)
    {
        Ptr<LteEnbNetDevice> enb = DynamicCast<LteEnbNetDevice>(m_enbDevs.Get(cell));
        enb->GetPhy()->SetTxPower(txPowerDbm);
    }

    void AccumulateEnergy(double nowSeconds)
    {
        if (nowSeconds <= m_lastEnergyUpdateSeconds)
        {
            return;
        }

        const double dt = nowSeconds - m_lastEnergyUpdateSeconds;
        for (bool active : m_active)
        {
            m_energyJ += dt * (active ? m_activePowerW : m_sleepPowerW);
            if (active)
            {
                m_activeCellSeconds += dt;
            }
        }
        m_lastEnergyUpdateSeconds = nowSeconds;
    }

    void WriteDecision(uint32_t cell,
                       const std::string& action,
                       const std::string& reason,
                       double loadMbps,
                       const TwinEstimate& estimate)
    {
        if (!m_eventCsv || !m_eventCsv->is_open())
        {
            return;
        }

        *m_eventCsv << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << ","
                    << m_policyName << "," << cell << "," << action << "," << reason << ","
                    << (m_active[cell] ? 1 : 0) << "," << loadMbps << ","
                    << estimate.maxUtilization << "," << estimate.minCoverageMarginDb << ","
                    << estimate.utilizationUncertainty << "," << estimate.coverageUncertaintyDb
                    << "," << (estimate.safe ? 1 : 0) << "\n";
    }

    PolicyMode m_policy;
    std::string m_policyName;
    Ptr<LteHelper> m_lteHelper;
    NodeContainer m_enbNodes;
    NodeContainer m_ueNodes;
    NetDeviceContainer m_enbDevs;
    NetDeviceContainer m_ueDevs;
    std::vector<uint32_t> m_servingEnb;
    Time m_simTime;
    Time m_controlStart;
    Time m_controlInterval;
    double m_ueRateMbps;
    double m_activeTxPowerDbm;
    double m_sleepTxPowerDbm;
    double m_cellCapacityMbps;
    double m_sleepUeThreshold;
    double m_aggressiveSleepUeThreshold;
    uint32_t m_minActiveEnbs;
    double m_minReliableRsrpDbm;
    double m_requiredCoverageMarginDb;
    double m_utilizationLimit;
    double m_uncertaintyScale;
    double m_activePowerW;
    double m_sleepPowerW;
    std::ofstream* m_eventCsv;
    std::vector<bool> m_active;
    double m_lastEnergyUpdateSeconds{0.0};
    double m_energyJ{0.0};
    double m_activeCellSeconds{0.0};
    uint32_t m_unsafeSleepActions{0};
    uint32_t m_handoverRequests{0};
};

int
main(int argc, char* argv[])
{
    uint32_t numberOfEnbs = 3;
    uint32_t numberOfUes = 16;
    uint32_t lowLoadUes = 2;
    uint32_t minActiveEnbs = 1;
    uint32_t packetSizeBytes = 1024;
    uint32_t seed = 1;
    uint32_t run = 1;
    double enbSpacingMeters = 500.0;
    double ueClusterRadiusMeters = 140.0;
    double ueRateMbps = 1.0;
    double cellCapacityMbps = 18.0;
    double sleepUeThreshold = 2.0;
    double aggressiveSleepUeThreshold = 7.0;
    double activeTxPowerDbm = 43.0;
    double sleepTxPowerDbm = -60.0;
    double minReliableRsrpDbm = -95.0;
    double requiredCoverageMarginDb = 3.0;
    double utilizationLimit = 0.78;
    double uncertaintyScale = 1.0;
    double activePowerW = 180.0;
    double sleepPowerW = 25.0;
    double minGoodputRatio = 0.85;
    double delaySlaMs = 80.0;
    Time simTime = Seconds(12.0);
    Time appStart = Seconds(0.6);
    Time controlStart = Seconds(2.0);
    Time controlInterval = Seconds(2.0);
    std::string policyName = "twin";
    std::string summaryCsv = "uno-umb-dt-energy-summary.csv";
    std::string eventCsv = "uno-umb-dt-energy-events.csv";

    CommandLine cmd(__FILE__);
    cmd.AddValue("policy", "Controller: all-on, threshold, aggressive, or twin", policyName);
    cmd.AddValue("numberOfEnbs", "Number of eNBs", numberOfEnbs);
    cmd.AddValue("numberOfUes", "Number of UEs", numberOfUes);
    cmd.AddValue("lowLoadUes", "UEs initially placed in the low-load center cell", lowLoadUes);
    cmd.AddValue("minActiveEnbs", "Minimum active eNBs allowed by sleep policies", minActiveEnbs);
    cmd.AddValue("simTime", "Total simulation time", simTime);
    cmd.AddValue("appStart", "Application start time", appStart);
    cmd.AddValue("controlStart", "First control decision time", controlStart);
    cmd.AddValue("controlInterval", "Control decision interval", controlInterval);
    cmd.AddValue("enbSpacingMeters", "Distance between neighboring eNBs", enbSpacingMeters);
    cmd.AddValue("ueClusterRadiusMeters", "Maximum UE offset from the home eNB", ueClusterRadiusMeters);
    cmd.AddValue("ueRateMbps", "Downlink UDP offered load per UE", ueRateMbps);
    cmd.AddValue("cellCapacityMbps", "Controller-side nominal cell capacity", cellCapacityMbps);
    cmd.AddValue("sleepUeThreshold", "UE-count threshold for threshold and twin policies", sleepUeThreshold);
    cmd.AddValue("aggressiveSleepUeThreshold",
                 "UE-count threshold for aggressive policy",
                 aggressiveSleepUeThreshold);
    cmd.AddValue("activeTxPowerDbm", "eNB Tx power while active", activeTxPowerDbm);
    cmd.AddValue("sleepTxPowerDbm", "eNB Tx power while sleeping", sleepTxPowerDbm);
    cmd.AddValue("minReliableRsrpDbm", "Twin-side minimum reliable RSRP proxy", minReliableRsrpDbm);
    cmd.AddValue("requiredCoverageMarginDb", "Twin-side coverage margin requirement", requiredCoverageMarginDb);
    cmd.AddValue("utilizationLimit", "Twin-side maximum load plus uncertainty", utilizationLimit);
    cmd.AddValue("uncertaintyScale", "Multiplier for twin uncertainty", uncertaintyScale);
    cmd.AddValue("activePowerW", "Analytical active eNB power draw", activePowerW);
    cmd.AddValue("sleepPowerW", "Analytical sleeping eNB power draw", sleepPowerW);
    cmd.AddValue("minGoodputRatio", "SLA goodput target relative to offered load", minGoodputRatio);
    cmd.AddValue("delaySlaMs", "Mean delay SLA in milliseconds", delaySlaMs);
    cmd.AddValue("packetSizeBytes", "UDP packet payload size", packetSizeBytes);
    cmd.AddValue("seed", "RNG seed", seed);
    cmd.AddValue("run", "RNG run", run);
    cmd.AddValue("summaryCsv", "CSV path for one-line run summary", summaryCsv);
    cmd.AddValue("eventCsv", "CSV path for controller decisions", eventCsv);
    cmd.Parse(argc, argv);

    NS_ABORT_MSG_IF(numberOfEnbs < 2, "At least two eNBs are required.");
    NS_ABORT_MSG_IF(numberOfUes == 0, "At least one UE is required.");
    NS_ABORT_MSG_IF(ueRateMbps <= 0.0, "ueRateMbps must be positive.");
    NS_ABORT_MSG_IF(controlStart <= appStart, "controlStart should be later than appStart.");

    const PolicyMode policy = ParsePolicy(policyName);

    RngSeedManager::SetSeed(seed);
    RngSeedManager::SetRun(run);
    Config::SetDefault("ns3::LteHelper::UseIdealRrc", BooleanValue(false));
    Config::SetDefault("ns3::LteEnbPhy::TxPower", DoubleValue(activeTxPowerDbm));
    Config::SetDefault("ns3::UdpClient::MaxPackets", UintegerValue(100000000));
    Config::SetDefault("ns3::UdpClient::PacketSize", UintegerValue(packetSizeBytes));

    Ptr<LteHelper> lteHelper = CreateObject<LteHelper>();
    Ptr<PointToPointEpcHelper> epcHelper = CreateObject<PointToPointEpcHelper>();
    lteHelper->SetEpcHelper(epcHelper);
    lteHelper->SetSchedulerType("ns3::RrFfMacScheduler");
    lteHelper->SetHandoverAlgorithmType("ns3::NoOpHandoverAlgorithm");
    lteHelper->SetEnbDeviceAttribute("DlBandwidth", UintegerValue(25));
    lteHelper->SetEnbDeviceAttribute("UlBandwidth", UintegerValue(25));

    Ptr<Node> pgw = epcHelper->GetPgwNode();

    NodeContainer remoteHostContainer;
    remoteHostContainer.Create(1);
    Ptr<Node> remoteHost = remoteHostContainer.Get(0);
    InternetStackHelper internet;
    internet.Install(remoteHostContainer);

    PointToPointHelper p2ph;
    p2ph.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Gb/s")));
    p2ph.SetDeviceAttribute("Mtu", UintegerValue(1500));
    p2ph.SetChannelAttribute("Delay", TimeValue(MilliSeconds(10)));
    NetDeviceContainer internetDevices = p2ph.Install(pgw, remoteHost);
    Ipv4AddressHelper ipv4h;
    ipv4h.SetBase("1.0.0.0", "255.0.0.0");
    Ipv4InterfaceContainer internetIpIfaces = ipv4h.Assign(internetDevices);

    Ipv4StaticRoutingHelper ipv4RoutingHelper;
    Ptr<Ipv4StaticRouting> remoteHostStaticRouting =
        ipv4RoutingHelper.GetStaticRouting(remoteHost->GetObject<Ipv4>());
    remoteHostStaticRouting->AddNetworkRouteTo(Ipv4Address("7.0.0.0"), Ipv4Mask("255.0.0.0"), 1);

    NodeContainer enbNodes;
    NodeContainer ueNodes;
    enbNodes.Create(numberOfEnbs);
    ueNodes.Create(numberOfUes);

    std::vector<uint32_t> homeCells = BuildHomeCells(numberOfUes, numberOfEnbs, lowLoadUes);

    Ptr<UniformRandomVariable> offsetRv = CreateObject<UniformRandomVariable>();
    offsetRv->SetAttribute("Min", DoubleValue(-ueClusterRadiusMeters));
    offsetRv->SetAttribute("Max", DoubleValue(ueClusterRadiusMeters));

    Ptr<ListPositionAllocator> positionAlloc = CreateObject<ListPositionAllocator>();
    for (uint32_t cell = 0; cell < numberOfEnbs; ++cell)
    {
        positionAlloc->Add(Vector(cell * enbSpacingMeters, 0.0, 25.0));
    }
    for (uint32_t u = 0; u < numberOfUes; ++u)
    {
        const double homeX = homeCells[u] * enbSpacingMeters;
        positionAlloc->Add(Vector(homeX + offsetRv->GetValue(), offsetRv->GetValue(), 1.5));
    }

    MobilityHelper mobility;
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.SetPositionAllocator(positionAlloc);
    mobility.Install(enbNodes);
    mobility.Install(ueNodes);

    NetDeviceContainer enbLteDevs = lteHelper->InstallEnbDevice(enbNodes);
    NetDeviceContainer ueLteDevs = lteHelper->InstallUeDevice(ueNodes);

    internet.Install(ueNodes);
    Ipv4InterfaceContainer ueIpIface = epcHelper->AssignUeIpv4Address(NetDeviceContainer(ueLteDevs));
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        Ptr<Ipv4StaticRouting> ueStaticRouting =
            ipv4RoutingHelper.GetStaticRouting(ueNodes.Get(u)->GetObject<Ipv4>());
        ueStaticRouting->SetDefaultRoute(epcHelper->GetUeDefaultGatewayAddress(), 1);
    }

    std::vector<uint32_t> servingEnb = homeCells;
    for (uint32_t u = 0; u < ueLteDevs.GetN(); ++u)
    {
        lteHelper->Attach(ueLteDevs.Get(u), enbLteDevs.Get(homeCells[u]));
    }

    const Time packetInterval = Seconds((packetSizeBytes * 8.0) / (ueRateMbps * 1000000.0));
    uint16_t dlPort = 12000;
    ApplicationContainer clientApps;
    ApplicationContainer serverApps;
    for (uint32_t u = 0; u < ueNodes.GetN(); ++u)
    {
        ++dlPort;
        PacketSinkHelper dlPacketSinkHelper("ns3::UdpSocketFactory",
                                            InetSocketAddress(Ipv4Address::GetAny(), dlPort));
        serverApps.Add(dlPacketSinkHelper.Install(ueNodes.Get(u)));

        UdpClientHelper dlClient(ueIpIface.GetAddress(u), dlPort);
        dlClient.SetAttribute("Interval", TimeValue(packetInterval));
        dlClient.SetAttribute("MaxPackets", UintegerValue(100000000));
        dlClient.SetAttribute("PacketSize", UintegerValue(packetSizeBytes));
        clientApps.Add(dlClient.Install(remoteHost));
    }

    serverApps.Start(appStart);
    clientApps.Start(appStart + MilliSeconds(100));
    clientApps.Stop(simTime - MilliSeconds(100));

    lteHelper->AddX2Interface(enbNodes);

    std::ofstream eventOut(eventCsv);
    eventOut << "time_s,policy,cell,action,reason,was_active,load_mbps,max_utilization,"
             << "min_coverage_margin_db,utilization_uncertainty,coverage_uncertainty_db,"
             << "twin_safe\n";

    SleepController controller(policy,
                               policyName,
                               lteHelper,
                               enbNodes,
                               ueNodes,
                               enbLteDevs,
                               ueLteDevs,
                               servingEnb,
                               simTime,
                               controlStart,
                               controlInterval,
                               ueRateMbps,
                               activeTxPowerDbm,
                               sleepTxPowerDbm,
                               cellCapacityMbps,
                               sleepUeThreshold,
                               aggressiveSleepUeThreshold,
                               minActiveEnbs,
                               minReliableRsrpDbm,
                               requiredCoverageMarginDb,
                               utilizationLimit,
                               uncertaintyScale,
                               activePowerW,
                               sleepPowerW,
                               &eventOut);
    controller.Start();

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Simulator::Stop(simTime);
    Simulator::Run();
    controller.FinalizeEnergy();
    monitor->CheckForLostPackets();

    uint64_t txPackets = 0;
    uint64_t rxPackets = 0;
    uint64_t txBytes = 0;
    uint64_t rxBytes = 0;
    Time delaySum = Seconds(0);
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();
    for (const auto& item : stats)
    {
        txPackets += item.second.txPackets;
        rxPackets += item.second.rxPackets;
        txBytes += item.second.txBytes;
        rxBytes += item.second.rxBytes;
        delaySum += item.second.delaySum;
    }

    const double measurementSeconds =
        std::max((simTime - (appStart + MilliSeconds(100))).GetSeconds(), 1.0);
    const double offeredLoadMbps = numberOfUes * ueRateMbps;
    const double throughputMbps = (rxBytes * 8.0) / (measurementSeconds * 1000000.0);
    const double lossRatio =
        (txPackets == 0) ? 0.0 : static_cast<double>(txPackets - rxPackets) / txPackets;
    const double meanDelayMs =
        (rxPackets == 0) ? 0.0 : delaySum.GetMilliSeconds() / static_cast<double>(rxPackets);
    const double allOnEnergyJ = simTime.GetSeconds() * numberOfEnbs * activePowerW;
    const double energySavingPct = 100.0 * (1.0 - controller.GetEnergyJ() / allOnEnergyJ);
    const bool slaViolation =
        (throughputMbps < minGoodputRatio * offeredLoadMbps) || (meanDelayMs > delaySlaMs);

    std::ofstream summaryOut(summaryCsv);
    summaryOut << "policy,seed,run,enbs,ues,sim_time_s,offered_load_mbps,throughput_mbps,"
               << "tx_packets,rx_packets,tx_bytes,rx_bytes,loss_ratio,mean_delay_ms,"
               << "energy_j,all_on_energy_j,energy_saving_pct,active_cell_seconds,"
               << "unsafe_sleep_actions,handover_requests,sla_violation\n";
    summaryOut << std::fixed << std::setprecision(6) << policyName << "," << seed << "," << run
               << "," << numberOfEnbs << "," << numberOfUes << "," << simTime.GetSeconds()
               << "," << offeredLoadMbps << "," << throughputMbps << "," << txPackets << ","
               << rxPackets << "," << txBytes << "," << rxBytes << "," << lossRatio << ","
               << meanDelayMs << "," << controller.GetEnergyJ() << "," << allOnEnergyJ << ","
               << energySavingPct << "," << controller.GetActiveCellSeconds() << ","
               << controller.GetUnsafeSleepActions() << "," << controller.GetHandoverRequests()
               << "," << (slaViolation ? 1 : 0) << "\n";

    std::cout << "policy=" << policyName << " throughputMbps=" << throughputMbps
              << " lossRatio=" << lossRatio << " meanDelayMs=" << meanDelayMs
              << " energySavingPct=" << energySavingPct
              << " unsafeSleepActions=" << controller.GetUnsafeSleepActions()
              << " handoverRequests=" << controller.GetHandoverRequests()
              << " slaViolation=" << (slaViolation ? 1 : 0) << std::endl;

    Simulator::Destroy();
    return 0;
}
