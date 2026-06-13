/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 *
 * Multi-station TWT AoI experiment: schedule algorithm vs baseline.
 *
 * A heterogeneous population of 802.11ax stations (different AoI weights,
 * duty-cycle budgets) sends periodic status updates to one AP. A TWT
 * scheduler from the aoi-twt contrib module computes the (period, offset,
 * duration) triple for each station; each station runs TwtPowerSaveManager
 * with SPs aligned on a common absolute time grid (AnchorTime). The script
 * reports per-station and weighted mean AoI (exact sawtooth integration),
 * compares them against the TwtAoiModel predictions, and reports per-station
 * duty cycles.
 *
 * Run with --scheduler=ns3::EqualIntervalTwtScheduler (baseline) or
 * --scheduler=ns3::HarmonicGreedyTwtScheduler (AoI-aware).
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/seq-ts-header.h"
#include "ns3/twt-schedule.h"
#include "ns3/twt-scheduler.h"
#include "ns3/wifi-module.h"

#include <cmath>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwtAoiMultiSta");

/**
 * Per-source block-fading error model: corrupts data-sized frames with a
 * per-source probability, drawing one verdict per service period (frames
 * from the same source separated by less than a gap threshold share the
 * verdict; the TWT queue gating guarantees inter-SP silence per source).
 * The source is recovered from the IPv4 header inside the MSDU.
 */
class SpFadingErrorModel : public ErrorModel
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("SpFadingErrorModel")
                                .SetParent<ErrorModel>()
                                .AddConstructor<SpFadingErrorModel>();
        return tid;
    }

    SpFadingErrorModel()
        : m_rng(CreateObject<UniformRandomVariable>())
    {
    }

    /**
     * Set the per-SP loss probability for a source.
     * @param src the source IPv4 address
     * @param rate the per-SP loss probability
     */
    void SetRate(Ipv4Address src, double rate)
    {
        m_state[src].rate = rate;
    }

    uint32_t m_sizeThreshold{120}; ///< corrupt only frames larger than this

  private:
    /// Per-source fading state
    struct SourceState
    {
        double rate{0};           ///< per-SP loss probability
        Time lastSeen{Seconds(-1)}; ///< last data reception from this source
        bool verdict{false};      ///< current SP verdict
    };

    bool DoCorrupt(Ptr<Packet> p) override
    {
        if (p->GetSize() <= m_sizeThreshold)
        {
            return false;
        }
        // recover the source address: MSDU = LLC/SNAP + IPv4 + ...
        auto copy = p->Copy();
        LlcSnapHeader llc;
        copy->RemoveHeader(llc);
        Ipv4Header ip;
        copy->RemoveHeader(ip);
        auto it = m_state.find(ip.GetSource());
        if (it == m_state.end())
        {
            return false;
        }
        auto& src = it->second;
        Time now = Simulator::Now();
        if (now - src.lastSeen > MilliSeconds(10))
        {
            // new service period for this source: draw a fresh verdict
            src.verdict = m_rng->GetValue() < src.rate;
        }
        src.lastSeen = now;
        return src.verdict;
    }

    void DoReset() override
    {
    }

    std::map<Ipv4Address, SourceState> m_state; ///< per-source state
    Ptr<UniformRandomVariable> m_rng;           ///< random source
};

/// Exact integrator of the AoI sawtooth process for one source.
struct AoiTracker
{
    Time lastEvent;      ///< time of the last reception
    Time ageAtLast;      ///< age right after the last reception
    double areaSec2{0};  ///< accumulated integral of the age process [s^2]
    Time firstEvent;     ///< time of the first reception
    uint64_t rxCount{0}; ///< number of receptions
    double peakSumSec{0}; ///< sum of per-cycle peak ages [s]
    uint64_t peakCount{0}; ///< number of peaks (age-resetting receptions)

    /**
     * Account for a status update received at the current simulation time.
     * @param genTime the generation (sampling) time carried by the update
     */
    void Update(Time genTime)
    {
        Time now = Simulator::Now();
        if (rxCount == 0)
        {
            firstEvent = now;
        }
        else
        {
            double elapsed = (now - lastEvent).GetSeconds();
            areaSec2 += elapsed * ageAtLast.GetSeconds() + elapsed * elapsed / 2;
            // age just before this reception = a per-cycle peak
            peakSumSec += ageAtLast.GetSeconds() + elapsed;
            ++peakCount;
        }
        ageAtLast = now - genTime;
        lastEvent = now;
        ++rxCount;
    }

    /**
     * @return the mean per-cycle peak AoI
     */
    Time GetMeanPeakAoi() const
    {
        return peakCount > 0 ? Seconds(peakSumSec / peakCount) : Time::Max();
    }

    /**
     * Close the integral and return the time-average AoI.
     * @return the time-average AoI between the first and last accounted instants
     */
    Time GetMeanAoi()
    {
        if (rxCount < 2)
        {
            return Time::Max();
        }
        Time now = Simulator::Now();
        double elapsed = (now - lastEvent).GetSeconds();
        double area = areaSec2 + elapsed * ageAtLast.GetSeconds() + elapsed * elapsed / 2;
        return Seconds(area / (now - firstEvent).GetSeconds());
    }
};

std::map<uint32_t, AoiTracker> g_aoi;      ///< per-station AoI trackers (by sta id)
std::map<Ipv4Address, uint32_t> g_addrToId; ///< source address -> station id
std::map<uint32_t, Time> g_awake;          ///< per-station non-sleep PHY time
std::map<uint32_t, Time> g_phyTotal;       ///< per-station total PHY state time

/**
 * Trace sink for UdpServer RxWithAddresses.
 * @param packet the received packet (SeqTsHeader still attached)
 * @param from the sender address
 * @param to the receiver address
 */
void
RxTrace(Ptr<const Packet> packet, const Address& from, const Address& to)
{
    SeqTsHeader seqTs;
    packet->Copy()->RemoveHeader(seqTs);
    auto src = InetSocketAddress::ConvertFrom(from).GetIpv4();
    if (auto it = g_addrToId.find(src); it != g_addrToId.end())
    {
        g_aoi[it->second].Update(seqTs.GetTs());
    }
}

/**
 * Trace sink for STA PHY state changes (context carries the node id).
 * @param context the trace context path
 * @param start the time the state began
 * @param duration the time spent in the state
 * @param state the PHY state
 */
void
PhyStateTrace(std::string context, Time start, Time duration, WifiPhyState state)
{
    // context: /NodeList/<id>/DeviceList/...
    uint32_t nodeId = std::stoul(context.substr(10));
    g_phyTotal[nodeId] += duration;
    if (state != WifiPhyState::SLEEP)
    {
        g_awake[nodeId] += duration;
    }
}

int
main(int argc, char* argv[])
{
    uint32_t nStations = 10;
    double simTime = 300;   // seconds
    uint32_t payload = 100; // bytes
    double appStart = 3;    // seconds
    std::string schedulerType = "ns3::HarmonicGreedyTwtScheduler";
    bool announced = false;
    double budgetScale = 1.0;
    double weightSkew = 0; // 0 = mild default weights 1..3
    bool fading = false;
    bool staticSetup = true;
    bool dyadic = false;
    double uniformBudget = 0;
    bool mixedMcs = false;

    CommandLine cmd(__FILE__);
    cmd.AddValue("nStations", "Number of stations", nStations);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("payload", "Status update payload in bytes", payload);
    cmd.AddValue("scheduler", "TypeId of the TWT scheduler to use", schedulerType);
    cmd.AddValue("announced", "Announce SP boundaries by PM bit toggling", announced);
    cmd.AddValue("budgetScale", "Scale factor on all duty cycle budgets", budgetScale);
    cmd.AddValue("uniformBudget",
                 "If > 0, give every station this exact duty-cycle budget "
                 "(overrides budgetScale); isolates AoI-weighting from budget "
                 "heterogeneity",
                 uniformBudget);
    cmd.AddValue("weightSkew",
                 "If > 0, alternate AoI weights between 1 and this value "
                 "(0 = mild default weights 1..3)",
                 weightSkew);
    cmd.AddValue("fading",
                 "Enable per-source per-SP block fading (heterogeneous loss "
                 "probabilities across stations)",
                 fading);
    cmd.AddValue("staticSetup",
                 "Use static association/BA setup (disable to reproduce the "
                 "in-band PM-mode-switch wedge)",
                 staticSetup);
    cmd.AddValue("dyadic",
                 "Use the dyadic-reservation scheduler variant covered by the "
                 "approximation analysis (DensityTarget 0.25)",
                 dyadic);
    cmd.AddValue("mixedMcs",
                 "Alternate stations between HeMcs0 and HeMcs7 (heterogeneous "
                 "SP airtimes); pair with a larger payload, e.g. 700 bytes",
                 mixedMcs);
    cmd.Parse(argc, argv);

    // --- Station population (heterogeneous weights and energy budgets) ---
    std::vector<TwtStationInfo> stations;
    std::vector<double> lossRates(nStations, 0.0);
    for (uint32_t i = 0; i < nStations; ++i)
    {
        TwtStationInfo sta;
        sta.id = i;
        sta.weight = weightSkew > 0 ? ((i % 2) ? weightSkew : 1.0) : 1.0 + (i % 3);
        sta.dutyCycleBudget =
            uniformBudget > 0 ? uniformBudget : budgetScale * (0.04 + 0.01 * (i % 4));
        // under block fading all in-SP attempts share the SP verdict, so the
        // per-SP success probability equals 1 - lossRate independent of the
        // number of attempts
        lossRates[i] = fading ? 0.15 + 0.15 * (i % 4) : 0.0;
        sta.attemptSuccessProb = 1.0 - lossRates[i];
        if (mixedMcs)
        {
            // heterogeneous airtimes: HeMcs0 vs HeMcs7 exchange estimates
            // for a ~700 byte payload (data + ack + contention)
            sta.attemptAirtime = (i % 2) ? MicroSeconds(300) : MicroSeconds(900);
            sta.wakeOverhead = MicroSeconds(500);
        }
        else
        {
            // effective airtime of one update exchange (data + ack + contention)
            sta.attemptAirtime = MilliSeconds(1);
            // per-wake overhead: PM announcements and radio settling
            sta.wakeOverhead = MilliSeconds(2);
        }
        stations.push_back(sta);
    }

    // --- Compute the TWT schedule ---
    ObjectFactory factory(schedulerType);
    // HarmonicGreedy and its EnergyGreedy subclass share these attributes.
    bool isGreedy = schedulerType.find("Greedy") != std::string::npos;
    if (fading && isGreedy)
    {
        // block fading: batching attempts within an SP cannot raise the SP
        // success probability
        factory.Set("MaxAttemptsPerSp", UintegerValue(1));
    }
    if (dyadic && isGreedy)
    {
        factory.Set("DyadicReservations", BooleanValue(true));
        factory.Set("DensityTarget", DoubleValue(0.25));
    }
    Ptr<TwtScheduler> scheduler = factory.Create<TwtScheduler>();
    auto computed = scheduler->Recompute(stations);
    NS_ABORT_MSG_IF(computed.size() != stations.size(), "Not all stations were scheduled");

    // index by station id (schedulers may return entries in packing order)
    std::vector<TwtScheduleEntry> schedule(nStations);
    for (const auto& entry : computed)
    {
        NS_ABORT_MSG_IF(entry.stationId >= nStations,
                        "Scheduler returned an unknown station id " << entry.stationId);
        schedule[entry.stationId] = entry;
    }

    std::cout << "Schedule (" << schedulerType << "):\n";
    for (const auto& entry : schedule)
    {
        std::cout << "  sta " << entry.stationId << ": T=" << std::fixed
                  << std::setprecision(2) << entry.wakeInterval.GetSeconds() * 1e3
                  << " ms, d=" << entry.wakeDuration.GetSeconds() * 1e3
                  << " ms, phi=" << entry.offset.GetSeconds() * 1e3 << " ms\n";
    }

    // --- Topology ---
    NodeContainer apNode(1);
    NodeContainer staNodes(nStations);

    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    phy.Set("ChannelSettings", StringValue("{0, 20, BAND_2_4GHZ, 0}"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HeMcs3"),
                                 "ControlMode",
                                 StringValue("HeMcs0"));

    Ssid ssid = Ssid("twt-aoi-multi");
    Time anchor = Seconds(2); // common SP grid epoch (after all associations)

    NetDeviceContainer staDevices;
    for (uint32_t i = 0; i < nStations; ++i)
    {
        const auto& entry = schedule[i];

        if (mixedMcs)
        {
            wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                         "DataMode",
                                         StringValue((i % 2) ? "HeMcs7" : "HeMcs0"),
                                         "ControlMode",
                                         StringValue("HeMcs0"));
        }

        WifiMacHelper staMac;
        staMac.SetType("ns3::StaWifiMac",
                       "Ssid",
                       SsidValue(ssid),
                       "MaxMissedBeacons",
                       UintegerValue(std::numeric_limits<uint32_t>::max()));
        staMac.SetPowerSaveManager("ns3::TwtPowerSaveManager",
                                   "PowerSaveMode",
                                   StringValue("0 1"),
                                   "WakeInterval",
                                   TimeValue(entry.wakeInterval),
                                   "WakeDuration",
                                   TimeValue(entry.wakeDuration),
                                   "FirstSpOffset",
                                   TimeValue(entry.offset),
                                   "AnchorTime",
                                   TimeValue(anchor),
                                   "Announced",
                                   BooleanValue(announced));
        staDevices.Add(wifi.Install(phy, staMac, staNodes.Get(i)));
    }

    if (mixedMcs)
    {
        // restore a common AP rate configuration
        wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                     "DataMode",
                                     StringValue("HeMcs3"),
                                     "ControlMode",
                                     StringValue("HeMcs0"));
    }
    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, apMac, apNode);

    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0, 0, 0));
    for (uint32_t i = 0; i < nStations; ++i)
    {
        double angle = 2 * M_PI * i / nStations;
        positions->Add(Vector(5 * std::cos(angle), 5 * std::sin(angle), 0));
    }
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNodes);

    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNodes);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apIf = ipv4.Assign(apDevice);
    Ipv4InterfaceContainer staIfs = ipv4.Assign(staDevices);

    NeighborCacheHelper neighborCache;
    neighborCache.PopulateNeighborCache();

    // Static association, PM mode alignment and Block Ack establishment:
    // bypasses the frame exchanges that would otherwise collide when many
    // stations associate simultaneously at startup. Disable (--staticSetup=0)
    // to reproduce the in-band PM-mode-switch wedge (see THEORY-NOTES f).
    auto apWifiDev = DynamicCast<WifiNetDevice>(apDevice.Get(0));
    if (staticSetup)
    {
        WifiStaticSetupHelper::SetStaticAssociation(apWifiDev, staDevices);
        WifiStaticSetupHelper::SetStaticBlockAck(apWifiDev, staDevices, {0});
    }

    if (fading)
    {
        Ptr<SpFadingErrorModel> fadingModel = CreateObject<SpFadingErrorModel>();
        fadingModel->m_sizeThreshold = payload + 20;
        for (uint32_t i = 0; i < nStations; ++i)
        {
            fadingModel->SetRate(staIfs.GetAddress(i), lossRates[i]);
        }
        Simulator::Schedule(Seconds(1.5),
                            &WifiPhy::SetPostReceptionErrorModel,
                            apWifiDev->GetPhy(),
                            Ptr<ErrorModel>(fadingModel));
    }

    // --- Status update traffic: one update per wake interval per station ---
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(0));
    serverApp.Get(0)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxTrace));

    for (uint32_t i = 0; i < nStations; ++i)
    {
        g_addrToId[staIfs.GetAddress(i)] = i;

        UdpClientHelper client(apIf.GetAddress(0), port);
        client.SetAttribute("MaxPackets", UintegerValue(0));
        client.SetAttribute("Interval", TimeValue(schedule[i].wakeInterval));
        client.SetAttribute("PacketSize", UintegerValue(payload));
        ApplicationContainer app = client.Install(staNodes.Get(i));
        // generate just before each SP (generate-at-will approximation):
        // start on the SP grid minus a small guard
        double phase = std::fmod((anchor + schedule[i].offset).GetSeconds(),
                                 schedule[i].wakeInterval.GetSeconds());
        double start = appStart - std::fmod(appStart - phase,
                                            schedule[i].wakeInterval.GetSeconds()) +
                       schedule[i].wakeInterval.GetSeconds() - 0.002;
        app.Start(Seconds(start));
    }

    Config::Connect("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State",
                    MakeCallback(&PhyStateTrace));

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // --- Results ---
    std::cout << "\n--- Results (" << schedulerType << ", " << simTime << " s) ---\n";
    std::cout << std::setw(4) << "sta" << std::setw(8) << "w" << std::setw(10) << "rx"
              << std::setw(14) << "AoI [ms]" << std::setw(14) << "pred [ms]" << std::setw(12)
              << "duty" << std::setw(12) << "budget" << "\n";

    double weightedAoi = 0;
    double weightedPred = 0;
    double weightedPeak = 0;
    double dutySum = 0;
    double totalWeight = 0;
    for (uint32_t i = 0; i < nStations; ++i)
    {
        Time measured = g_aoi[i].GetMeanAoi();
        // generate-at-will just before the SP: reset value is the small
        // generation guard plus the in-SP delivery delay, approximated by a
        // 4 ms constant; per-SP success probability = 1 - lossRate (block
        // fading: independent of the number of in-SP attempts)
        Time predicted = TwtAoiModel::MeanAoi(schedule[i].wakeInterval,
                                              1.0 - lossRates[i],
                                              MilliSeconds(4));
        uint32_t nodeId = staNodes.Get(i)->GetId();
        double duty = g_phyTotal[nodeId].IsZero()
                          ? 0
                          : g_awake[nodeId].GetSeconds() / g_phyTotal[nodeId].GetSeconds();

        weightedAoi += stations[i].weight * measured.GetSeconds() * 1e3;
        weightedPred += stations[i].weight * predicted.GetSeconds() * 1e3;
        weightedPeak += stations[i].weight * g_aoi[i].GetMeanPeakAoi().GetSeconds() * 1e3;
        dutySum += duty;
        totalWeight += stations[i].weight;

        std::cout << std::setw(4) << i << std::setw(8) << stations[i].weight << std::setw(10)
                  << g_aoi[i].rxCount << std::setw(14) << std::setprecision(2)
                  << measured.GetSeconds() * 1e3 << std::setw(14)
                  << predicted.GetSeconds() * 1e3 << std::setw(12) << std::setprecision(4)
                  << duty << std::setw(12) << stations[i].dutyCycleBudget << "\n";
    }
    std::cout << "Weighted mean AoI: " << std::setprecision(2) << weightedAoi / totalWeight
              << " ms (predicted " << weightedPred / totalWeight << " ms)\n";
    std::cout << "SUMMARY aoi_ms=" << weightedAoi / totalWeight
              << " peak_ms=" << weightedPeak / totalWeight
              << " duty=" << std::setprecision(5) << dutySum / nStations << "\n";

    Simulator::Destroy();
    return 0;
}
