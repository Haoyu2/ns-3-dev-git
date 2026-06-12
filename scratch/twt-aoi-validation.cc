/*
 * Copyright (c) 2026
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

/**
 * @file
 *
 * Single-station validation of the TWT AoI renewal-reward model.
 *
 * One 802.11ax STA sends one status update per TWT wake interval to the AP
 * (generate-at-will, period-aligned with the TWT schedule). The STA uses
 * TwtPowerSaveManager to doze between service periods. The script measures
 * - the time-average Age of Information at the AP (exact event-driven
 *   integration of the age sawtooth), and
 * - the PHY duty cycle from WifiPhyStateHelper traces,
 * and compares the measured AoI against the analytical prediction
 * E[A] = meanDelay + T * (1/pSucc - 1/2) from TwtAoiModel (contrib/aoi-twt).
 *
 * Expected outcome (reliable channel, pSucc ~= 1): measured mean AoI within
 * a few percent of meanDelay + T/2.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/network-module.h"
#include "ns3/seq-ts-header.h"
#include "ns3/wifi-module.h"

#include <iomanip>
#include <iostream>
#include <limits>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("TwtAoiValidation");

/// Exact integrator of the AoI sawtooth process at the receiver.
struct AoiTracker
{
    Time lastEvent;     ///< time of the last age-resetting reception
    Time ageAtLast;     ///< age right after the last reception
    double areaSec2{0}; ///< accumulated integral of the age process [s^2]
    Time firstEvent;    ///< time of the first reception
    uint64_t rxCount{0}; ///< number of receptions
    Time delaySum;      ///< sum of per-update delays
    Time freshThreshold{Time::Max()}; ///< updates younger than this are "fresh"
    uint64_t freshCount{0};           ///< number of fresh receptions
    Time freshDelaySum;               ///< sum of fresh per-update delays
    Time lastFreshReset;              ///< time of the last fresh reception
    double gapSumSec{0};              ///< sum of inter-fresh-reset gaps [s]
    double gapSqSumSec2{0};           ///< sum of squared gaps [s^2]
    uint64_t gapCount{0};             ///< number of gaps

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
        }
        Time newAge = now - genTime;
        delaySum += newAge;
        if (newAge < freshThreshold)
        {
            freshDelaySum += newAge;
            if (freshCount > 0)
            {
                double gap = (now - lastFreshReset).GetSeconds();
                gapSumSec += gap;
                gapSqSumSec2 += gap * gap;
                ++gapCount;
            }
            lastFreshReset = now;
            ++freshCount;
        }
        // in-order delivery: the new sample is always fresher
        ageAtLast = newAge;
        lastEvent = now;
        ++rxCount;
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

AoiTracker g_aoi;                      ///< the AoI tracker
std::map<std::string, Time> g_phyTime; ///< accumulated time per PHY state (STA)

/**
 * Error model corrupting only frames larger than a size threshold, so that
 * data frames experience a known loss probability while management frames
 * (association, ADDBA, etc.) are unaffected.
 *
 * The corruption verdict is drawn once per reception burst (receptions
 * separated by less than a coherence gap share the verdict), modeling
 * block fading whose coherence time covers a whole TWT service period:
 * all in-SP transmission attempts fail together, so the per-SP success
 * probability is exactly 1 - rate.
 */
class DataFrameErrorModel : public ErrorModel
{
  public:
    /**
     * @brief Get the type ID.
     * @return the object TypeId
     */
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("DataFrameErrorModel")
                                .SetParent<ErrorModel>()
                                .AddConstructor<DataFrameErrorModel>();
        return tid;
    }

    DataFrameErrorModel()
        : m_rng(CreateObject<UniformRandomVariable>())
    {
    }

    /**
     * Configure the model.
     * @param rate the corruption probability for matching frames
     * @param sizeThreshold corrupt only frames larger than this many bytes
     * @param coherenceTime block fading window (one verdict per window)
     */
    void Configure(double rate, uint32_t sizeThreshold, Time coherenceTime)
    {
        m_rate = rate;
        m_sizeThreshold = sizeThreshold;
        m_coherenceTime = coherenceTime;
    }

  private:
    bool DoCorrupt(Ptr<Packet> p) override
    {
        if (p->GetSize() <= m_sizeThreshold)
        {
            return false;
        }
        ++m_seen;
        // block fading: one verdict per coherence-time window, so all
        // attempts within one SP fail or succeed together
        auto window = Simulator::Now().GetNanoSeconds() / m_coherenceTime.GetNanoSeconds();
        if (window != m_lastWindow)
        {
            m_lastWindow = window;
            m_verdict = m_rng->GetValue() < m_rate;
            if (m_verdict)
            {
                ++m_corruptedBursts;
            }
            ++m_bursts;
        }
        if (m_verdict)
        {
            ++m_corrupted;
        }
        return m_verdict;
    }

    void DoReset() override
    {
    }

    double m_rate{0};                 ///< corruption probability per window
    uint32_t m_sizeThreshold{150};    ///< minimum frame size to corrupt
    Ptr<UniformRandomVariable> m_rng; ///< random source
    Time m_coherenceTime{MilliSeconds(100)}; ///< block fading window
    int64_t m_lastWindow{-1};         ///< index of the last fading window seen
    bool m_verdict{false};            ///< current window verdict

  public:
    uint64_t m_seen{0};            ///< data-sized frames seen
    uint64_t m_corrupted{0};       ///< data-sized frames corrupted
    uint64_t m_bursts{0};          ///< reception bursts (SPs with data)
    uint64_t m_corruptedBursts{0}; ///< bursts drawn as corrupted
};

/**
 * Trace sink for UdpServer RxWithAddresses: extract the generation timestamp.
 * @param packet the received packet (SeqTsHeader still attached)
 * @param from the sender address
 * @param to the receiver address
 */
bool g_dumpAoi = false; ///< print every reception for sawtooth inspection

void
RxTrace(Ptr<const Packet> packet, const Address& from, const Address& to)
{
    SeqTsHeader seqTs;
    packet->Copy()->RemoveHeader(seqTs);
    if (g_dumpAoi)
    {
        std::cout << "AOI t=" << Simulator::Now().GetMilliSeconds()
                  << "ms seq=" << seqTs.GetSeq()
                  << " age=" << (Simulator::Now() - seqTs.GetTs()).GetMilliSeconds() << "ms\n";
    }
    g_aoi.Update(seqTs.GetTs());
}

std::map<std::string, uint64_t> g_macDrops; ///< STA MAC drops by reason
std::map<std::string, uint64_t> g_apPhyDrops; ///< AP PHY rx drops by reason
uint64_t g_apMacRx = 0;                       ///< frames forwarded up by AP MAC
uint64_t g_apRxOk = 0;                        ///< PSDUs successfully received by AP PHY
uint64_t g_apRxError = 0;                     ///< PSDUs failed at AP PHY

/**
 * Trace sink for the AP PhyRxDrop trace.
 * @param packet the dropped packet
 * @param reason the drop reason
 */
void
ApPhyDropTrace(Ptr<const Packet> packet, WifiPhyRxfailureReason reason)
{
    std::ostringstream oss;
    oss << reason;
    ++g_apPhyDrops[oss.str()];
}

/**
 * Trace sink for the AP MacRx trace.
 * @param packet the packet forwarded up
 */
void
ApMacRxTrace(Ptr<const Packet> packet)
{
    ++g_apMacRx;
}

/**
 * Trace sink for the AP PHY RxOk trace.
 * @param packet the received packet
 * @param snr the SNR
 * @param mode the WifiMode
 * @param preamble the preamble
 */
void
ApRxOkTrace(Ptr<const Packet> packet, double snr, WifiMode mode, WifiPreamble preamble)
{
    ++g_apRxOk;
}

/**
 * Trace sink for the AP PHY RxError trace.
 * @param packet the failed packet
 * @param snr the SNR
 */
void
ApRxErrorTrace(Ptr<const Packet> packet, double snr)
{
    ++g_apRxError;
}

/**
 * Trace sink for the STA WifiMac DroppedMpdu trace.
 * @param reason the drop reason
 * @param mpdu the dropped MPDU
 */
void
MacDropTrace(WifiMacDropReason reason, Ptr<const WifiMpdu> mpdu)
{
    static const std::map<uint8_t, std::string> reasons{
        {0, "FAILED_ENQUEUE"},
        {1, "EXPIRED_LIFETIME"},
        {2, "REACHED_RETRY_LIMIT"},
        {3, "QOS_OLD_PACKET"}};
    ++g_macDrops[reasons.at(reason)];
}

/**
 * Trace sink for the STA WifiPhyStateHelper State trace.
 * @param start the time the state began
 * @param duration the time spent in the state
 * @param state the PHY state
 */
void
PhyStateTrace(Time start, Time duration, WifiPhyState state)
{
    std::ostringstream oss;
    oss << state;
    g_phyTime[oss.str()] += duration;
}

int
main(int argc, char* argv[])
{
    Time wakeInterval = MilliSeconds(100);
    Time wakeDuration = MilliSeconds(8);
    double simTime = 60;    // seconds
    double distance = 5;    // meters
    uint32_t payload = 100; // bytes
    double appStart = 2;    // seconds
    double errorRate = 0;   // per-attempt loss probability at the AP
    uint32_t retryLimit = 0; // frame retry limit (0 = keep default)

    CommandLine cmd(__FILE__);
    cmd.AddValue("wakeInterval", "TWT wake interval", wakeInterval);
    cmd.AddValue("wakeDuration", "TWT SP duration", wakeDuration);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("distance", "AP-STA distance in meters", distance);
    cmd.AddValue("payload", "Status update payload in bytes", payload);
    cmd.AddValue("errorRate",
                 "Probability that the AP fails to receive a frame (activated after "
                 "association; with retryLimit=1 the per-SP success probability is "
                 "exactly 1-errorRate)",
                 errorRate);
    cmd.AddValue("retryLimit", "STA frame retry limit (0 = keep default)", retryLimit);
    cmd.AddValue("dumpAoi", "Print every status update reception", g_dumpAoi);
    cmd.Parse(argc, argv);

    NodeContainer apNode(1);
    NodeContainer staNode(1);

    // PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());
    // 2.4 GHz: avoids the VHT-triggered mandatory Block Ack agreement, whose
    // in-order delivery (head-of-line blocking behind sequence gaps) is
    // undesirable for status updates and outside the AoI model
    phy.Set("ChannelSettings", StringValue("{0, 20, BAND_2_4GHZ, 0}"));

    WifiHelper wifi;
    wifi.SetStandard(WIFI_STANDARD_80211ax);
    wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                                 "DataMode",
                                 StringValue("HeMcs3"),
                                 "ControlMode",
                                 StringValue("HeMcs0"));

    Ssid ssid = Ssid("twt-aoi");

    WifiMacHelper staMac;
    // A TWT STA dozes through beacons; disable beacon-loss link monitoring
    // (proper TWT-aware association maintenance is a later milestone)
    staMac.SetType("ns3::StaWifiMac",
                   "Ssid",
                   SsidValue(ssid),
                   "MaxMissedBeacons",
                   UintegerValue(std::numeric_limits<uint32_t>::max()));
    staMac.SetPowerSaveManager("ns3::TwtPowerSaveManager",
                               "PowerSaveMode",
                               StringValue("0 1"),
                               "WakeInterval",
                               TimeValue(wakeInterval),
                               "WakeDuration",
                               TimeValue(wakeDuration),
                               "FirstSpOffset",
                               TimeValue(MilliSeconds(50)));
    NetDeviceContainer staDevice = wifi.Install(phy, staMac, staNode);

    WifiMacHelper apMac;
    apMac.SetType("ns3::ApWifiMac", "Ssid", SsidValue(ssid));
    NetDeviceContainer apDevice = wifi.Install(phy, apMac, apNode);

    if (retryLimit > 0)
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/FrameRetryLimit",
                    UintegerValue(retryLimit));
    }

    // Controlled per-attempt loss: corrupt only data-sized receptions at the
    // AP with a known probability, leaving management frames (ADDBA, PM
    // signaling) unaffected. Activated after association. Aggregation is
    // disabled so that every data MPDU is a separate reception.
    Ptr<DataFrameErrorModel> errorModel;
    if (errorRate > 0)
    {
        Config::Set("/NodeList/*/DeviceList/*/$ns3::WifiNetDevice/Mac/BE_MaxAmpduSize",
                    UintegerValue(0));
        // the model sees the MSDU (no MAC header): LLC+IP+UDP add ~36 bytes;
        // fading coherence = the wake interval, so each SP is one fading block
        errorModel = CreateObject<DataFrameErrorModel>();
        errorModel->Configure(errorRate, payload + 20, wakeInterval);
        auto apPhy = DynamicCast<WifiNetDevice>(apDevice.Get(0))->GetPhy();
        Simulator::Schedule(Seconds(1.5),
                            &WifiPhy::SetPostReceptionErrorModel,
                            apPhy,
                            Ptr<ErrorModel>(errorModel));
    }

    // Mobility
    MobilityHelper mobility;
    Ptr<ListPositionAllocator> positions = CreateObject<ListPositionAllocator>();
    positions->Add(Vector(0, 0, 0));
    positions->Add(Vector(distance, 0, 0));
    mobility.SetPositionAllocator(positions);
    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(apNode);
    mobility.Install(staNode);

    // Internet stack
    InternetStackHelper internet;
    internet.Install(apNode);
    internet.Install(staNode);
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    Ipv4InterfaceContainer apIf = ipv4.Assign(apDevice);
    ipv4.Assign(staDevice);

    // Pre-populate ARP caches: the AP cannot deliver a downlink ARP reply to
    // a dozing STA until the AP-side TWT mechanism (later milestone) serves
    // buffered frames during SPs
    NeighborCacheHelper neighborCache;
    neighborCache.PopulateNeighborCache();

    // Status updates: one per wake interval, STA -> AP
    uint16_t port = 4000;
    UdpServerHelper server(port);
    ApplicationContainer serverApp = server.Install(apNode.Get(0));
    serverApp.Start(Seconds(0));

    UdpClientHelper client(apIf.GetAddress(0), port);
    client.SetAttribute("MaxPackets", UintegerValue(0)); // unlimited
    client.SetAttribute("Interval", TimeValue(wakeInterval));
    client.SetAttribute("PacketSize", UintegerValue(payload));
    ApplicationContainer clientApp = client.Install(staNode.Get(0));
    clientApp.Start(Seconds(appStart));

    serverApp.Get(0)->TraceConnectWithoutContext("RxWithAddresses", MakeCallback(&RxTrace));

    Config::ConnectWithoutContext(
        "/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Phy/State/State",
        MakeCallback(&PhyStateTrace));
    Config::ConnectWithoutContext("/NodeList/1/DeviceList/*/$ns3::WifiNetDevice/Mac/DroppedMpdu",
                                  MakeCallback(&MacDropTrace));
    Config::ConnectWithoutContext("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/PhyRxDrop",
                                  MakeCallback(&ApPhyDropTrace));
    Config::ConnectWithoutContext("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Mac/MacRx",
                                  MakeCallback(&ApMacRxTrace));
    Config::ConnectWithoutContext("/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxOk",
                                  MakeCallback(&ApRxOkTrace));
    Config::ConnectWithoutContext(
        "/NodeList/0/DeviceList/*/$ns3::WifiNetDevice/Phy/State/RxError",
        MakeCallback(&ApRxErrorTrace));

    g_aoi.freshThreshold = wakeInterval;

    Simulator::Stop(Seconds(simTime));
    Simulator::Run();

    // Results
    auto serverPtr = DynamicCast<UdpServer>(serverApp.Get(0));
    uint64_t received = serverPtr->GetReceived();
    auto sent = static_cast<uint64_t>((simTime - appStart) / wakeInterval.GetSeconds());
    double pHat = sent > 0 ? static_cast<double>(received) / sent : 0;
    pHat = std::min(pHat, 1.0);

    Time meanDelay = g_aoi.rxCount > 0 ? g_aoi.delaySum / static_cast<int64_t>(g_aoi.rxCount)
                                       : Time::Max();
    Time measured = g_aoi.GetMeanAoi();
    // E[A] = meanDelay + T * (1/pSucc - 1/2); with one update per SP, the
    // SP-level success probability is estimated by the delivery ratio
    Time predicted = meanDelay + Seconds(wakeInterval.GetSeconds() * (1.0 / pHat - 0.5));

    Time awake;
    Time total;
    for (const auto& [state, time] : g_phyTime)
    {
        total += time;
        if (state != "SLEEP")
        {
            awake += time;
        }
    }

    std::cout << std::fixed << std::setprecision(3);
    std::cout << "\n--- TWT AoI validation ---\n";
    std::cout << "Updates sent:        " << sent << "\n";
    std::cout << "Updates received:    " << received << " (delivery ratio " << pHat << ")\n";
    std::cout << "Mean delay:          " << meanDelay.GetMilliSeconds() << " ms\n";
    std::cout << "Measured mean AoI:   " << measured.GetMilliSeconds() << " ms\n";
    std::cout << "Predicted mean AoI:  " << predicted.GetMilliSeconds() << " ms\n";
    std::cout << "Prediction error:    "
              << 100 * std::abs((measured - predicted).GetSeconds()) / predicted.GetSeconds()
              << " %\n";
    if (errorRate > 0)
    {
        // block fading per SP: the SP-level success probability is known,
        // and the AoI resets only on fresh (same-SP) deliveries
        double pKnown = 1 - errorRate;
        Time meanFreshDelay = g_aoi.freshCount > 0
                                  ? g_aoi.freshDelaySum / static_cast<int64_t>(g_aoi.freshCount)
                                  : Time::Max();
        Time predictedKnown =
            meanFreshDelay + Seconds(wakeInterval.GetSeconds() * (1.0 / pKnown - 0.5));
        std::cout << "Predicted (p=1-e):   " << predictedKnown.GetMilliSeconds() << " ms ("
                  << 100 * std::abs((measured - predictedKnown).GetSeconds()) /
                         predictedKnown.GetSeconds()
                  << " % error, fresh deliveries " << g_aoi.freshCount << ", mean fresh delay "
                  << meanFreshDelay.GetMilliSeconds() << " ms)\n";
        if (g_aoi.gapCount > 0)
        {
            // renewal-reward check: E[A] = meanFreshDelay + E[G^2]/(2 E[G])
            // with G the inter-reset gap; theory says E[G] = T/p and
            // E[G^2]/(2 E[G]) = T (1/p - 1/2)
            double meanGap = g_aoi.gapSumSec / g_aoi.gapCount;
            double halfSecondMoment = g_aoi.gapSqSumSec2 / g_aoi.gapCount / (2 * meanGap);
            std::cout << "Renewal check:       mean gap " << 1e3 * meanGap << " ms (theory "
                      << wakeInterval.GetSeconds() * 1e3 / pKnown << "), E[G^2]/2E[G] "
                      << 1e3 * halfSecondMoment << " ms (theory "
                      << 1e3 * wakeInterval.GetSeconds() * (1 / pKnown - 0.5) << ")\n";
        }
    }
    std::cout << "STA PHY awake ratio: "
              << (total.IsZero() ? 0.0 : awake.GetSeconds() / total.GetSeconds())
              << " (TWT duty cycle " << wakeDuration.GetSeconds() / wakeInterval.GetSeconds()
              << ")\n";
    for (const auto& [state, time] : g_phyTime)
    {
        std::cout << "  PHY " << state << ": " << time.GetSeconds() << " s\n";
    }
    if (errorModel)
    {
        std::cout << "Error model: " << errorModel->m_seen << " data frames seen, "
                  << errorModel->m_corrupted << " corrupted; " << errorModel->m_bursts
                  << " bursts, " << errorModel->m_corruptedBursts << " corrupted ("
                  << (errorModel->m_bursts > 0 ? static_cast<double>(errorModel->m_corruptedBursts) /
                                                     errorModel->m_bursts
                                               : 0)
                  << ")\n";
    }
    for (const auto& [reason, count] : g_macDrops)
    {
        std::cout << "STA MAC drop " << reason << ": " << count << "\n";
    }
    std::cout << "AP MacRx (frames forwarded up): " << g_apMacRx << "\n";
    std::cout << "AP PHY RxOk: " << g_apRxOk << ", RxError: " << g_apRxError << "\n";
    for (const auto& [reason, count] : g_apPhyDrops)
    {
        std::cout << "AP PHY drop " << reason << ": " << count << "\n";
    }

    Simulator::Destroy();
    return 0;
}
