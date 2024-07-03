//
// Created by haoyu on 6/18/2024.
//

#include "my-config.h"



#include <iomanip>
#include <iostream>

using namespace ns3;

void PrintTPHeader(std::ofstream &output)
{
    output << "Throughput are converted to MB/s\n"
              "The last measure window may not have a full measurement window\n"
              "Marked packets are number\n";
    output << std::setw(3) << " ";
    Time time1 = startRecordingTP + intervalRecordingTP;
    for (; time1 <= stopRecordingTP + intervalRecordingTP;)
    {
        output << std::setfill(' ') << std::setw(widthThroughPuts)
                  << std::fixed << std::showpoint
                  << std::setprecision(3)
                  <<  (time1 > stopRecordingTP ? stopRecordingTP : time1)  .GetSeconds() << "s ";
        time1 += intervalRecordingTP;
    }
    output << std::endl;
}

void PrintQSHeader(std::ofstream &output)
{
    output << std::setw(widthRecordTimeQS) <<  " ";

    output << std::setw(widthQueueSize) << "(nPackets) ";

    output << std::endl;
}

/*
 *
 *
 --AQ=REM
 --MarkingThreshold=40
 --NumberNodes=2
 --DataRateNeck=10Gbps
 --DataRateLeaf=10Gbps
 --DelayNeck=50us
 --DelayLeaf=25us
 --FlowStartupWindow=0s
 --StopTime=3s
 --StartRecordingQS=1.55s
 --StopRecordingQS=1.56s
 --IntervalRecordingQS=10ms

 *
 *
 * */

void Set_CLI_Args(CommandLine& cmd, int argc, char* argv[])
{
    supportQueue[REM] = SetTFMyRedQueueDisc;
    supportQueue[MYQ] = SetTFMyQueueDisc;

    cmd.AddValue("AQ", "Active queue algorithm at bottle neck", activeQueue);
    cmd.AddValue("Abrupt", "A half of testing flows starts late abruptly ", abrupt);
    cmd.AddValue("MarkingThreshold", "ECN Marking Threshold at Queue", threshold);
    cmd.AddValue("NumberNodes", "Total number of node for flow", numNodes);
    cmd.AddValue("CounterTotal", "Total number of node for flow", counterTotal);

    cmd.AddValue("DataRateNeck", "Data rate at bottle neck link", dataRateNeck);
    cmd.AddValue("DataRateLeaf", "Data rate at leaf link", dataRateLeaf);
    cmd.AddValue("DelayNeck", "Link delay at bottle neck", delayNeck);
    cmd.AddValue("DelayLeaf", "Link delay at leaf", delayLeaf);

    cmd.AddValue("FlowStartupWindow", "if 0 every flow starts at same time", flowStartupWindow);
    cmd.AddValue("StopTime", "The stop time of simulation", stopTime);
    cmd.AddValue("ProgressInterval", "Print progress every default 100ms", progressInterval);

    cmd.AddValue("StartRecordingTP", "Starting time for recording throughput", startRecordingTP);
    cmd.AddValue("StopRecordingTP", "Stopping time for recording throughput", stopRecordingTP);
    cmd.AddValue("IntervalRecordingTP", "Interval time for recording throughput default 0.5s", intervalRecordingTP);

    cmd.AddValue("StartRecordingQS", "Starting time for recording throughput", startRecordingQS);
    cmd.AddValue("StopRecordingQS", "Stopping time for recording throughput", stopRecordingQS);
    cmd.AddValue("IntervalRecordingQS", "Interval time for recording throughput default 10ms", intervalRecordingQS);
    cmd.Parse(argc, argv);

    stopRecordingTP = stopRecordingTP > stopTime ? stopTime : stopRecordingTP;
    if (stopTime < flowStartupWindow
        || stopTime < startRecordingTP
        || stopTime < stopRecordingTP
        || stopTime < startRecordingQS
        || stopTime < stopRecordingQS
        )
    {
        NS_FATAL_ERROR ("Stop time is smaller than flowStartupWindow/startRecordingTP/stopRecordingTP");
    }

    if (supportQueue.find("activeQueue") != supportQueue.end())
    {
        NS_FATAL_ERROR ("Has not supported this active queue algorithm");
    }
}

auto AppendVector(int n)
{
    std::vector<uint64_t> a(n);
    return a;
}

void Init(int argc, char* argv[])
{

    sinkBytes.push_back(AppendVector(numNodes));
    markBytes.push_back(AppendVector(numNodes));

    data_dir = data_dir + "/" + activeQueue + "/ra"
               + "/CT_" +std::to_string(counterTotal)
               + "-TH" +  std::to_string(threshold)
               + "_N" + std::to_string(numNodes)
               + "_NK" + dataRateNeck + "_LF" + dataRateLeaf
               + "_NK" + delayNeck + "_LF" + delayLeaf
        ;
    CreateFolderIfNotExists(data_dir.c_str());

    std::ofstream argStream(data_dir + "/args");
    for (int i = 0; i < argc; ++i)
    {
        argStream << argv[i] << "\n";
    }
    argStream.close();
    throughPutStream.open(data_dir + "/through_puts.dat");
    PrintTPHeader(throughPutStream);
    queueLengthStream.open(data_dir + "/queue_length.dat");
    PrintQSHeader(queueLengthStream);

}

std::tuple<PointToPointHelper, PointToPointHelper> SetPointToPointHelper()
{
    PointToPointHelper leafHelper;
    leafHelper.SetDeviceAttribute("DataRate", StringValue(dataRateLeaf));
    leafHelper.SetChannelAttribute("Delay", StringValue(delayLeaf));

    PointToPointHelper neckHelper;
    neckHelper.SetDeviceAttribute("DataRate", StringValue(dataRateNeck));
    neckHelper.SetChannelAttribute("Delay", StringValue(delayNeck));

    return { neckHelper, leafHelper};
}


void SetDefaultConfigDCTCP()
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));
}

Ptr<PacketSink> SetupFlow(int iFlow, Ptr<Node> leftNode, Ipv4Address rightIP, Ptr<Node> rightNode)
{
    static uint16_t port = 5000;
    port++;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

    ApplicationContainer sinkApp = sinkHelper.Install(rightNode);
    Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
    sinkApp.Start(Seconds(0));
    sinkApp.Stop(stopTime);

    OnOffHelper clientHelper1("ns3::TcpSocketFactory", Address());
    clientHelper1.SetAttribute("OnTime",
                               StringValue("ns3::ConstantRandomVariable[Constant=1]"));
    clientHelper1.SetAttribute("OffTime",
                               StringValue("ns3::ConstantRandomVariable[Constant=0]"));
    clientHelper1.SetAttribute("DataRate", DataRateValue(DataRate(dataRateLeaf)));
    clientHelper1.SetAttribute("PacketSize", UintegerValue(1000));

    ApplicationContainer clientApps1;
    AddressValue remoteAddress(InetSocketAddress(rightIP, port));
    clientHelper1.SetAttribute("Remote", remoteAddress);
    clientApps1.Add(clientHelper1.Install(leftNode));
    if (abrupt != Seconds(0))
    {
        if (iFlow < (numNodes/2))
        {
            clientApps1.Start(Seconds(0));
        } else
        {
            clientApps1.Start(abrupt + MilliSeconds(10)* iFlow);
//            clientApps1.Get(0)->TraceConnectWithoutContext("Tx", MakeBoundCallback(&TraceClient, iFlow));
        }
    }else
    {
        // this is for grace startup not for abrupt
        clientApps1.Start(iFlow * flowStartupWindow / numNodes );
    };
    clientApps1.Stop(stopTime);

    return packetSink;
}


void TraceClient(std::size_t index, Ptr<const Packet> p)
{
    clientBytes[0][index%10]++;
}

void TraceSink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    sinkBytes.back()[index] += p->GetSize();
}
void MarkEvent(Ptr<const QueueDiscItem> item, const char* reason)
{
    auto p = DynamicCast<const Ipv4QueueDiscItem, const QueueDiscItem>(item);
    auto ipMark = p->GetHeader().GetSource().Get();
    auto i = ipToIndex[ipMark];
    markBytes.back()[i]++;
}


void DumpQueueSize(std::ofstream &output)
{
    Time time1= startRecordingQS;
    for (int i = 0; i < recordsQS.size();i++)
    {
        output << std::setw(widthRecordTimeQS) << std::fixed << std::setprecision(3)
                          << time1.GetSeconds () << "s "
                          << std::setw(widthQueueSize) <<  recordsQS[i] ;

        if (activeQueue == MYQ)
        {
            auto [total, categories, mean, current] = queueCounterStats[i];
            output << " total " << total << " cat" << categories << " mean" << mean << " current" << current;
        }
        output << "\n";
        time1 += intervalRecordingQS;
    }
    output << std::endl;
}

void CheckQueueSize(Ptr<QueueDisc> queue)
{
    uint32_t qSize = queue->GetNPackets();
    if (activeQueue == MYQ)
    {
        auto myQueue = DynamicCast<MyQueueDisc, QueueDisc>(queue);
        auto stats = myQueue->GetQueueCounter().GetState();
        queueCounterStats.push_back(stats);
    }
    recordsQS.push_back(qSize);
    if (Simulator::Now() < stopRecordingQS)
    {
        Simulator::Schedule(intervalRecordingQS, &CheckQueueSize, queue);
    }
}

double ToMbs(uint64_t bits)
{
    return (bits * 8) / (intervalRecordingTP.GetSeconds()) / 1e6;
}

//
void DumpThroughput(std::ofstream &output, std::vector<std::vector<uint64_t>> data,  bool toMb)
{
    for (int i = 0; i < numNodes; ++i)
    {
        output << std::setfill(' ') << std::setw(4) << i << " ";
        for (int j = 0; j < data.size(); ++j)
        {
            output << std::setfill(' ') << std::setw(widthThroughPuts)
                   << std::fixed << std::showpoint
                   << std::setprecision(toMb ? 1 : 0)
                   <<  (toMb ? ToMbs(data[j][i]) : data[j][i]) << " ";
        }
        output << "\n";
    }
    output << "\n";
    output << "Total";
    for (int i = 0; i < data.size(); ++i)
    {
        uint64_t num = 0;
        for (int j = 0; j < numNodes; ++j)
        {
            num += data[i][j];
        }
        output << std::setfill(' ') << std::setw(widthThroughPuts)
               << std::fixed << std::showpoint
               << std::setprecision(toMb ? 1 : 0)
               << (toMb? ToMbs(num) : num) << " ";
    }
    output << "\n\n\n";
}

void DumpVectorOfVector(std::ofstream &output, std::vector<std::vector<uint64_t>> data)
{
    output << "Dumped vector value:\n";
    for (int i = 0; i < data.size(); ++i)
    {
        for (auto n : data[i])
        {
            output << n << " ";
        }
        output << "\n";
    }
    output<<"\n\n";
}

void CheckThroughputAndMarking()
{
    sinkBytes.push_back(AppendVector(numNodes));
    markBytes.push_back(AppendVector(numNodes));
    if (Simulator::Now() < stopRecordingTP)
    {
        Simulator::Schedule(intervalRecordingTP, &CheckThroughputAndMarking);
    }
}



void InitVector(std::vector<uint64_t> v)
{
    for (int i = 0; i < v.size(); ++i)
    {
        v[i] = 0;
    }
}

void PrintProgress(Time interval)
{
    static const clock_t begin_time = std::clock();
    double real = float(std::clock() - begin_time) / CLOCKS_PER_SEC;
    std::cout << "Progress to " << std::fixed << std::setprecision(1)
              << Simulator::Now().GetSeconds() << " seconds simulation time\t"
              << Time(std::to_string(real) + "s").GetMinutes() << " minutes in real time" << std::endl;
    Simulator::Schedule(interval, &PrintProgress, interval);
}


