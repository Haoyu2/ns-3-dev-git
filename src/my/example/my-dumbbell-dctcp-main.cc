#include "my-config.h"

#include "ns3/applications-module.h"
#include "ns3/assert.h"
#include "ns3/core-module.h"
#include "ns3/data-rate.h"
#include "ns3/internet-module.h"
#include "ns3/log.h"
#include "ns3/my-point-to-point-dumbbell.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/simulator.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ScratchSimulator");

// ipccc
/*
 *  Paper Abstract Due:	June 15th, 2024
 *
 *  All Paper/Poster Submissions Due:	July 1st, 2024
 *
 *  ICNC
 *  Paper submission:
 *  July 15, 2024
 *
 * */




int main(int argc, char* argv[])
{
    NS_LOG_UNCOND("Scratch Simulator");
    CommandLine cmd(__FILE__);
    Set_CLI_Args(cmd, argc, argv);
    Init(argc, argv);
    SetDefaultConfigDCTCP();

    auto [ neckHelper, leafHelper] = SetPointToPointHelper();
    auto [neckQueue, leafQueue] = supportQueue[activeQueue]();

    MyPointToPointDumbbellHelper dumbbell(
        numNodes, leafHelper,
        numNodes, leafHelper, neckHelper,
            neckQueue, leafQueue);
    dumbbell.AssignIpv4Addresses(
        Ipv4AddressHelper ("1.1.1.0", "255.255.255.0"),
        Ipv4AddressHelper ("2.2.2.0", "255.255.255.0"),
        Ipv4AddressHelper ("3.3.3.0", "255.255.255.0"));

    Ptr<QueueDisc> neckQ = dumbbell.GetNeckQueueContainer().Get(0);

    neckQ->TraceConnectWithoutContext("Mark", MakeCallback(MarkEvent));

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    std::vector<Ptr<PacketSink>> sinks(numNodes);

    for (uint32_t i = 0; i < numNodes; i++)
    {
        Ptr<PacketSink> sink = SetupFlow(
                                   i,
                                   dumbbell.GetLeft(i),
                                   dumbbell.GetRightIpv4Address(i),
                                   dumbbell.GetRight(i));
        auto src = dumbbell.GetLeftIpv4Address(i).Get();
        ipToIndex[src] = i;
        sinks.push_back(sink);
        sink->TraceConnectWithoutContext("Rx", MakeBoundCallback(&TraceSink, i));
    }

    // records queue sizes
    Simulator::Schedule(startRecordingQS,&CheckQueueSize, neckQ);
    // records throughput
    Simulator::Schedule(startRecordingTP + intervalRecordingTP,&CheckThroughputAndMarking);

    Simulator::Schedule(progressInterval, &PrintProgress, progressInterval);
    Simulator::Stop(stopTime + TimeStep(1));

    Simulator::Run();
    Simulator::Destroy();
    DumpQueueSize(queueLengthStream);
    DumpThroughput(throughPutStream, sinkBytes, true);
    DumpThroughput(throughPutStream, markBytes, false);
    neckQ->GetStats().Print(throughPutStream);
    DumpVectorOfVector(throughPutStream, sinkBytes);
    DumpVectorOfVector(throughPutStream, markBytes);
//    DumpVectorOfVector(throughPutStream, clientBytes);
    return 0;
}
