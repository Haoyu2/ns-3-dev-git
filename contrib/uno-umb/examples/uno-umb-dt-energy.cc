/*
 * Example simulator for QoS-safe RAN cell sleep experiments.
 *
 * This program compares simple cell-sleep controllers and writes CSV files
 * that can be used directly for early paper plots.
 */

#include "ns3/applications-module.h"
#include "ns3/cell-sleep-controller.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/lte-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"

#include <algorithm>
#include <cstdint>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>
#include <vector>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("UnoUmbDtEnergy");

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

    const CellSleepPolicyMode policy = ParseCellSleepPolicy(policyName);

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
    eventOut << CellSleepController::GetEventCsvHeader() << "\n";

    CellSleepControllerConfig controllerConfig;
    controllerConfig.policy = policy;
    controllerConfig.policyName = policyName;
    controllerConfig.lteHelper = lteHelper;
    controllerConfig.enbNodes = enbNodes;
    controllerConfig.ueNodes = ueNodes;
    controllerConfig.enbDevs = enbLteDevs;
    controllerConfig.ueDevs = ueLteDevs;
    controllerConfig.servingEnb = servingEnb;
    controllerConfig.simTime = simTime;
    controllerConfig.controlStart = controlStart;
    controllerConfig.controlInterval = controlInterval;
    controllerConfig.ueRateMbps = ueRateMbps;
    controllerConfig.activeTxPowerDbm = activeTxPowerDbm;
    controllerConfig.sleepTxPowerDbm = sleepTxPowerDbm;
    controllerConfig.cellCapacityMbps = cellCapacityMbps;
    controllerConfig.sleepUeThreshold = sleepUeThreshold;
    controllerConfig.aggressiveSleepUeThreshold = aggressiveSleepUeThreshold;
    controllerConfig.minActiveEnbs = minActiveEnbs;
    controllerConfig.minReliableRsrpDbm = minReliableRsrpDbm;
    controllerConfig.requiredCoverageMarginDb = requiredCoverageMarginDb;
    controllerConfig.utilizationLimit = utilizationLimit;
    controllerConfig.uncertaintyScale = uncertaintyScale;
    controllerConfig.activePowerW = activePowerW;
    controllerConfig.sleepPowerW = sleepPowerW;

    CellSleepController controller(controllerConfig, &eventOut);
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
