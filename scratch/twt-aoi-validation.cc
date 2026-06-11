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
 * Trace sink for UdpServer RxWithAddresses: extract the generation timestamp.
 * @param packet the received packet (SeqTsHeader still attached)
 * @param from the sender address
 * @param to the receiver address
 */
void
RxTrace(Ptr<const Packet> packet, const Address& from, const Address& to)
{
    SeqTsHeader seqTs;
    packet->Copy()->RemoveHeader(seqTs);
    g_aoi.Update(seqTs.GetTs());
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
    double simTime = 60;   // seconds
    double distance = 5;   // meters
    uint32_t payload = 100; // bytes
    double appStart = 2;   // seconds

    CommandLine cmd(__FILE__);
    cmd.AddValue("wakeInterval", "TWT wake interval", wakeInterval);
    cmd.AddValue("wakeDuration", "TWT SP duration", wakeDuration);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.AddValue("distance", "AP-STA distance in meters", distance);
    cmd.AddValue("payload", "Status update payload in bytes", payload);
    cmd.Parse(argc, argv);

    NodeContainer apNode(1);
    NodeContainer staNode(1);

    // PHY and channel
    YansWifiChannelHelper channel = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy;
    phy.SetChannel(channel.Create());

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
    std::cout << "STA PHY awake ratio: "
              << (total.IsZero() ? 0.0 : awake.GetSeconds() / total.GetSeconds())
              << " (TWT duty cycle " << wakeDuration.GetSeconds() / wakeInterval.GetSeconds()
              << ")\n";
    for (const auto& [state, time] : g_phyTime)
    {
        std::cout << "  PHY " << state << ": " << time.GetSeconds() << " s\n";
    }

    Simulator::Destroy();
    return 0;
}
