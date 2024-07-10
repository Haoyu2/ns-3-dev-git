/*
 * Copyright (c) 2017-20 NITK Surathkal
 * Copyright (c) 2020 Tom Henderson (better alignment with experiment)
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Shravya K.S. <shravya.ks0@gmail.com>
 *          Apoorva Bhargava <apoorvabhargava13@gmail.com>
 *          Shikha Bakshi <shikhabakshi912@gmail.com>
 *          Mohit P. Tahiliani <tahiliani@nitk.edu.in>
 *          Tom Henderson <tomh@tomh.org>
 */

// The network topology used in this example is based on Fig. 17 described in
// Mohammad Alizadeh, Albert Greenberg, David A. Maltz, Jitendra Padhye,
// Parveen Patel, Balaji Prabhakar, Sudipta Sengupta, and Murari Sridharan.
// "Data Center TCP (DCTCP)." In ACM SIGCOMM Computer Communication Review,
// Vol. 40, No. 4, pp. 63-74. ACM, 2010.

// The topology is roughly as follows
//
//  S1         S3
//  |           |  (1 Gbps)
//  T1 ------- T2 -- R1
//  |           |  (1 Gbps)
//  S2         R2
//
// The link between switch T1 and T2 is 10 Gbps.  All other
// links are 1 Gbps.  In the SIGCOMM paper, there is a Scorpion switch
// between T1 and T2, but it doesn't contribute another bottleneck.
//
// S1 and S3 each have 10 senders sending to receiver R1 (20 total)
// S2 (20 senders) sends traffic to R2 (20 receivers)
//
// This sets up two bottlenecks: 1) T1 -> T2 interface (30 senders
// using the 10 Gbps link) and 2) T2 -> R1 (20 senders using 1 Gbps link)
//
// RED queues configured for ECN marking are used at the bottlenecks.
//
// Figure 17 published results are that each sender in S1 gets 46 Mbps
// and each in S3 gets 54 Mbps, while each S2 sender gets 475 Mbps, and
// that these are within 10% of their fair-share throughputs (Jain index
// of 0.99).
//
// This program runs the program by default for five seconds.  The first
// second is devoted to flow startup (all 40 TCP flows are stagger started
// during this period).  There is a three second convergence time where
// no measurement data is taken, and then there is a one second measurement
// interval to gather raw throughput for each flow.  These time intervals
// can be changed at the command line.
//
// The program outputs six files.  The first three:
// * dctcp-example-s1-r1-throughput.dat
// * dctcp-example-s2-r2-throughput.dat
// * dctcp-example-s3-r1-throughput.dat
// provide per-flow throughputs (in Mb/s) for each of the forty flows, summed
// over the measurement window.  The fourth file,
// * dctcp-example-fairness.dat
// provides average throughputs for the three flow paths, and computes
// Jain's fairness index for each flow group (i.e. across each group of
// 10, 20, and 10 flows).  It also sums the throughputs across each bottleneck.
// The fifth and sixth:
// * dctcp-example-t1-length.dat
// * dctcp-example-t2-length.dat
// report on the bottleneck queue length (in packets and microseconds
// of delay) at 10 ms intervals during the measurement window.
//
// By default, the throughput averages are 23 Mbps for S1 senders, 471 Mbps
// for S2 senders, and 74 Mbps for S3 senders, and the Jain index is greater
// than 0.99 for each group of flows.  The average queue delay is about 1ms
// for the T2->R2 bottleneck, and about 200us for the T1->T2 bottleneck.
//
// The RED parameters (min_th and max_th) are set to the same values as
// reported in the paper, but we observed that throughput distributions
// and queue delays are very sensitive to these parameters, as was also
// observed in the paper; it is likely that the paper's throughput results
// could be achieved by further tuning of the RED parameters.  However,
// the default results show that DCTCP is able to achieve high link
// utilization and low queueing delay and fairness across competing flows
// sharing the same path.

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include "utils.h"
#include <arpa/inet.h>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <netinet/in.h>
#include <unistd.h>

using namespace ns3;


int
main(int argc, char* argv[])
{
    CommandLine cmd(__FILE__);
    CLI(cmd, argc, argv);

    Init();

    NS3DefaultConfig();


    Time startTime = Seconds(0);
    Time stopTime = flowStartupWindow + convergenceTime + measurementWindow;

    NodeContainer S1;
    NodeContainer S2;
    NodeContainer S3;
    NodeContainer R2;
    Ptr<Node> T1 = CreateObject<Node>();
    Ptr<Node> T2 = CreateObject<Node>();
    Ptr<Node> R1 = CreateObject<Node>();
    S1.Create(10);
    S2.Create(20);
    S3.Create(10);
    R2.Create(20);



    PointToPointHelper pointToPointSR;
    pointToPointSR.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    pointToPointSR.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper pointToPointT;
    pointToPointT.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    pointToPointT.SetChannelAttribute("Delay", StringValue("10us"));

    // Create a total of 62 links.
    std::vector<NetDeviceContainer> S1T1;
    S1T1.reserve(10);
    std::vector<NetDeviceContainer> S2T1;
    S2T1.reserve(20);
    std::vector<NetDeviceContainer> S3T2;
    S3T2.reserve(10);
    std::vector<NetDeviceContainer> R2T2;
    R2T2.reserve(20);
    NetDeviceContainer T1T2 = pointToPointT.Install(T1, T2);
    NetDeviceContainer R1T2 = pointToPointSR.Install(R1, T2);

    for (std::size_t i = 0; i < 10; i++)
    {
        Ptr<Node> n = S1.Get(i);
        S1T1.push_back(pointToPointSR.Install(n, T1));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        Ptr<Node> n = S2.Get(i);
        S2T1.push_back(pointToPointSR.Install(n, T1));
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        Ptr<Node> n = S3.Get(i);
        S3T2.push_back(pointToPointSR.Install(n, T2));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        Ptr<Node> n = R2.Get(i);
        R2T2.push_back(pointToPointSR.Install(n, T2));
    }

    InternetStackHelper stack;
    stack.InstallAll();

    std::cout << "===" << MaxTH_SWITCH1 << "===" << MaxTH_SWITCH2<< std::endl;
    TrafficControlHelper tchRed10;
    // MinTh = 50, MaxTh = 150 recommended in ACM SIGCOMM 2010 DCTCP Paper
    // This yields a target (MinTh) queue depth of 60us at 10 Gb/s
    tchRed10.SetRootQueueDisc(BottleNeckQueueDisc,
                              "LinkBandwidth",
                              StringValue("10Gbps"),
                              "LinkDelay",
                              StringValue("10us"),
                              "MinTh",
                              DoubleValue(MaxTH_SWITCH1),
                              "MaxTh",
                              DoubleValue(MaxTH_SWITCH1));
    QueueDiscContainer queueDiscs1 = tchRed10.Install(T1T2);
    queueDiscs1.Get(0)->TraceConnectWithoutContext("Mark", MakeBoundCallback(MarkEvent, SWITCH1));
    queueDiscs1.Get(0)->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(DequeueEvent, SWITCH1));

//    queueDiscs1.Get(1)->TraceConnectWithoutContext("Mark", MakeBoundCallback(MarkEvent, T2));

    TrafficControlHelper tchRed1;
    // MinTh = 20, MaxTh = 60 recommended in ACM SIGCOMM 2010 DCTCP Paper
    // This yields a target queue depth of 250us at 1 Gb/s
    tchRed1.SetRootQueueDisc(BottleNeckQueueDisc,
                             "LinkBandwidth",
                             StringValue("1Gbps"),
                             "LinkDelay",
                             StringValue("10us"),
                             "MinTh",
                             DoubleValue(MaxTH_SWITCH2),
                             "MaxTh",
                             DoubleValue(MaxTH_SWITCH2));
    QueueDiscContainer queueDiscs2 = tchRed1.Install(R1T2.Get(1));
    queueDiscs2.Get(0)->TraceConnectWithoutContext("Mark", MakeBoundCallback(MarkEvent, SWITCH2));
    queueDiscs2.Get(0)->TraceConnectWithoutContext("Dequeue", MakeBoundCallback(DequeueEvent, SWITCH2));
    for (std::size_t i = 0; i < 10; i++)
    {
        tchRed1.Install(S1T1[i].Get(1));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        tchRed1.Install(S2T1[i].Get(1));
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        tchRed1.Install(S3T2[i].Get(1));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        tchRed1.Install(R2T2[i].Get(1));
    }

    Ipv4AddressHelper address;
    std::vector<Ipv4InterfaceContainer> ipS1T1;
    ipS1T1.reserve(10);
    std::vector<Ipv4InterfaceContainer> ipS2T1;
    ipS2T1.reserve(20);
    std::vector<Ipv4InterfaceContainer> ipS3T2;
    ipS3T2.reserve(10);
    std::vector<Ipv4InterfaceContainer> ipR2T2;
    ipR2T2.reserve(20);
    address.SetBase("172.16.1.0", "255.255.255.0");
    Ipv4InterfaceContainer ipT1T2 = address.Assign(T1T2);
    address.SetBase("192.168.0.0", "255.255.255.0");
    Ipv4InterfaceContainer ipR1T2 = address.Assign(R1T2);
    address.SetBase("10.1.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 10; i++)
    {
        ipS1T1.push_back(address.Assign(S1T1[i]));
        address.NewNetwork();
        uint32_t ip = ipS1T1.at(i).GetAddress(0, 0).Get();
        cm_ips[SEND1].emplace_back(IPV4ToStringNode(ip));
    }
    std::cout << std::endl;
    address.SetBase("10.2.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 20; i++)
    {
        ipS2T1.push_back(address.Assign(S2T1[i]));
        address.NewNetwork();
        uint32_t ip = ipS2T1.at(i).GetAddress(0, 0).Get();
        cm_ips[SEND2].emplace_back(IPV4ToStringNode(ip));

    }
    std::cout << std::endl;
    address.SetBase("10.3.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 10; i++)
    {
        ipS3T2.push_back(address.Assign(S3T2[i]));
        address.NewNetwork();
        uint32_t ip = ipS3T2.at(i).GetAddress(0, 0).Get();
        cm_ips[SEND3].emplace_back(IPV4ToStringNode(ip));

    }
    address.SetBase("10.4.1.0", "255.255.255.0");
    for (std::size_t i = 0; i < 20; i++)
    {
        ipR2T2.push_back(address.Assign(R2T2[i]));
        address.NewNetwork();
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    // Each sender in S2 sends to a receiver in R2
    std::vector<Ptr<PacketSink>> r2Sinks;
    r2Sinks.reserve(20);
    for (std::size_t i = 0; i < 20; i++)
    {
        uint16_t port = 50000 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(R2.Get(i));
        Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
        r2Sinks.push_back(packetSink);
        sinkApp.Start(startTime);
        sinkApp.Stop(stopTime);

        OnOffHelper clientHelper1("ns3::TcpSocketFactory", Address());
        clientHelper1.SetAttribute("OnTime",
                                   StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper1.SetAttribute("OffTime",
                                   StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper1.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
        clientHelper1.SetAttribute("PacketSize", UintegerValue(1000));

        ApplicationContainer clientApps1;
        AddressValue remoteAddress(InetSocketAddress(ipR2T2[i].GetAddress(0), port));
        clientHelper1.SetAttribute("Remote", remoteAddress);
        clientApps1.Add(clientHelper1.Install(S2.Get(i)));
        clientApps1.Start(i * flowStartupWindow / 20 + startTime + MilliSeconds(i * 5));
        clientApps1.Stop(stopTime);
    }

    // Each sender in S1 and S3 sends to R1
    std::vector<Ptr<PacketSink>> s1r1Sinks;
    std::vector<Ptr<PacketSink>> s3r1Sinks;
    s1r1Sinks.reserve(10);
    s3r1Sinks.reserve(10);
    for (std::size_t i = 0; i < 20; i++)
    {
        uint16_t port = 50000 + i;
        Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);
        ApplicationContainer sinkApp = sinkHelper.Install(R1);
        Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
        if (i < 10)
        {
            s1r1Sinks.push_back(packetSink);
        }
        else
        {
            s3r1Sinks.push_back(packetSink);
        }
        sinkApp.Start(startTime);
        sinkApp.Stop(stopTime);

        OnOffHelper clientHelper1("ns3::TcpSocketFactory", Address());
        clientHelper1.SetAttribute("OnTime",
                                   StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        clientHelper1.SetAttribute("OffTime",
                                   StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        clientHelper1.SetAttribute("DataRate", DataRateValue(DataRate("1Gbps")));
        clientHelper1.SetAttribute("PacketSize", UintegerValue(1000));

        ApplicationContainer clientApps1;
        AddressValue remoteAddress(InetSocketAddress(ipR1T2.GetAddress(0), port));
        clientHelper1.SetAttribute("Remote", remoteAddress);
        if (i < 10)
        {
            clientApps1.Add(clientHelper1.Install(S1.Get(i)));
            clientApps1.Start(i * flowStartupWindow / 10 + startTime + MilliSeconds(i * 5));
        }
        else
        {
            clientApps1.Add(clientHelper1.Install(S3.Get(i - 10)));
            clientApps1.Start((i - 10) * flowStartupWindow / 10 + startTime + MilliSeconds(i * 5));

        }
        clientApps1.Stop(stopTime);
    }


    for (std::size_t i = 0; i < 10; i++)
    {
        s1r1Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS1R1Sink, i));
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        r2Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS2R2Sink, i));
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        s3r1Sinks[i]->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceS3R1Sink, i));
    }
    Simulator::Schedule (flowStartupWindow + convergenceTime, &IncrementCurrentRecordingStep);

    Simulator::Schedule(flowStartupWindow + convergenceTime + measurementWindowTP, &PrintThroughput);
    Simulator::Schedule(flowStartupWindow + convergenceTime + measurementWindow,
                        &PrintFairness);
    Simulator::Schedule(progressInterval, &PrintProgress, progressInterval);
    Simulator::Schedule(flowStartupWindow + convergenceTime, &CheckT1QueueSize, queueDiscs1.Get(0));
    Simulator::Schedule(flowStartupWindow + convergenceTime, &CheckT2QueueSize, queueDiscs2.Get(0));
    Simulator::Stop(stopTime + TimeStep(1));

    Simulator::Run();

    ecnMarkStats << "ip\t\tT1\tT2" << std::endl;
    CM senders[] {SEND1, SEND2, SEND3};
    for (CM sender : senders)
    {
        for (const std::string& ip : cm_ips[sender])
        {
           ecnMarkStats << ip << "\t\t"
                << ecn_counter[SWITCH1][ip] << "\t"
                << ecn_counter[SWITCH2][ip] << std::endl;
        }
    }


    queueDiscs1.Get(0)->GetStats().Print(ecnMarkStats);
    queueDiscs2.Get(0)->GetStats().Print(ecnMarkStats);

    rxS1R1Throughput.close();
    rxS2R2Throughput.close();
    rxS3R1Throughput.close();
    fairnessIndex.close();
    t1QueueLength.close();
    t2QueueLength.close();
    ecnMarkStats.close();
    Simulator::Destroy();
    return 0;
}
