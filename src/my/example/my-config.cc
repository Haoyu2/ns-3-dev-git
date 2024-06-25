//
// Created by haoyu on 6/18/2024.
//

#include "my-config.h"



#include <iomanip>
#include <iostream>

using namespace ns3;


void PrintHeader(std::ofstream &output)
{
    output <<  "     " ;
    for (Time t = recordingInterval; t < stopTime; t+=recordingInterval)
    {

        output << " " << std::setw(6) <<  std::fixed << std::showpoint
                    << std::setprecision(1) << t.GetSeconds() << "s";

    }
    output << std::endl;
}

void Init()
{
    stopTime = Seconds(duration);
    data_dir = data_dir + "/" +  std::to_string(threshold)+"duration_" + std::to_string(duration) + "s";
    CreateFolderIfNotExists(data_dir.c_str());

    throughPuts.open(data_dir + "/through_puts.dat");
    PrintHeader(throughPuts);
    queueLength.open(data_dir + "/queue_length.dat");
    PrintHeader(queueLength);


    // Output config store to txt format
    Config::SetDefault("ns3::ConfigStore::Filename", StringValue(data_dir +  "/output-attributes.txt"));
    Config::SetDefault("ns3::ConfigStore::FileFormat", StringValue("RawText"));
    Config::SetDefault("ns3::ConfigStore::Mode", StringValue("Save"));
}

std::tuple<PointToPointHelper, PointToPointHelper> SetPointToPointHelper()
{
    PointToPointHelper leafHelper;
    leafHelper.SetDeviceAttribute("DataRate", StringValue("1Gbps"));
    leafHelper.SetChannelAttribute("Delay", StringValue("10us"));

    PointToPointHelper neckHelper;
    neckHelper.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    neckHelper.SetChannelAttribute("Delay", StringValue("10us"));

    return { neckHelper, leafHelper};
}


void SetDefaultConfigDCTCP()
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::TcpDctcp"));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));
}

void SetDefaultMyRedQueueDisc()
{
    // Set default parameters for RED queue disc
    Config::SetDefault("ns3::MyRedQueueDisc::UseEcn", BooleanValue(true));
    // ARED may be used but the queueing delays will increase; it is disabled
    // here because the SIGCOMM paper did not mention it
    // Config::SetDefault ("ns3::RedQueueDisc::ARED", BooleanValue (true));
    // Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
    Config::SetDefault("ns3::MyRedQueueDisc::UseHardDrop", BooleanValue(false));
    Config::SetDefault("ns3::MyRedQueueDisc::MeanPktSize", UintegerValue(1500));
    // Triumph and Scorpion switches used in DCTCP Paper have 4 MB of buffer
    // If every packet is 1500 bytes, 2666 packets can be stored in 4 MB
    Config::SetDefault("ns3::MyRedQueueDisc::MaxSize", QueueSizeValue(QueueSize("2666p")));
    // DCTCP tracks instantaneous queue length only; so set QW = 1
    Config::SetDefault("ns3::MyRedQueueDisc::QW", DoubleValue(1));
    Config::SetDefault("ns3::MyRedQueueDisc::MinTh", DoubleValue(20));
    Config::SetDefault("ns3::MyRedQueueDisc::MaxTh", DoubleValue(60));
}


// traffic control layer
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyRedQueueDisc()
{
    SetDefaultMyRedQueueDisc();
    TrafficControlHelper neckQueue;
    neckQueue.SetRootQueueDisc("ns3::MyRedQueueDisc",
                               "LinkBandwidth",
                               StringValue("10Gbps"),
                               "LinkDelay",
                               StringValue("10us"),
                               "MinTh",
                               DoubleValue(threshold),
                               "MaxTh",
                               DoubleValue(threshold));

    TrafficControlHelper leafQueue;
    leafQueue.SetRootQueueDisc("ns3::MyRedQueueDisc",
                             "LinkBandwidth",
                             StringValue("1Gbps"),
                             "LinkDelay",
                             StringValue("10us"),
                             "MinTh",
                             DoubleValue(threshold),
                             "MaxTh",
                             DoubleValue(threshold));

    return {neckQueue, leafQueue};
}



void SetDefaultMyQueueDisc()
{
    // Set default parameters for RED queue disc
    Config::SetDefault("ns3::MyQueueDisc::UseEcn", BooleanValue(true));
    // ARED may be used but the queueing delays will increase; it is disabled
    // here because the SIGCOMM paper did not mention it
    // Config::SetDefault ("ns3::RedQueueDisc::ARED", BooleanValue (true));
    // Config::SetDefault ("ns3::RedQueueDisc::Gentle", BooleanValue (true));
    Config::SetDefault("ns3::MyQueueDisc::UseHardDrop", BooleanValue(false));
    Config::SetDefault("ns3::MyQueueDisc::MeanPktSize", UintegerValue(1500));
    // Triumph and Scorpion switches used in DCTCP Paper have 4 MB of buffer
    // If every packet is 1500 bytes, 2666 packets can be stored in 4 MB
    Config::SetDefault("ns3::MyQueueDisc::MaxSize", QueueSizeValue(QueueSize("2666p")));
    // DCTCP tracks instantaneous queue length only; so set QW = 1
    Config::SetDefault("ns3::MyQueueDisc::QW", DoubleValue(1));
    Config::SetDefault("ns3::MyQueueDisc::MinTh", DoubleValue(20));
    Config::SetDefault("ns3::MyQueueDisc::MaxTh", DoubleValue(60));
}


// traffic control layer
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyQueueDisc()
{
    SetDefaultMyQueueDisc();
    TrafficControlHelper neckQueue;
    neckQueue.SetRootQueueDisc("ns3::MyQueueDisc",
                               "LinkBandwidth",
                               StringValue("10Gbps"),
                               "LinkDelay",
                               StringValue("10us"),
                               "MinTh",
                               DoubleValue(threshold),
                               "MaxTh",
                               DoubleValue(threshold));

    TrafficControlHelper leafQueue;
    leafQueue.SetRootQueueDisc("ns3::MyQueueDisc",
                               "LinkBandwidth",
                               StringValue("1Gbps"),
                               "LinkDelay",
                               StringValue("10us"),
                               "MinTh",
                               DoubleValue(threshold),
                               "MaxTh",
                               DoubleValue(threshold));

    return {neckQueue, leafQueue};
}

Ptr<PacketSink> SetupFlow(int iFlow, Ptr<Node> leftNode, Ipv4Address rightIP, Ptr<Node> rightNode)
{
    static uint16_t port = 5000;
    port++;
    Address sinkLocalAddress(InetSocketAddress(Ipv4Address::GetAny(), port));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddress);

    ApplicationContainer sinkApp = sinkHelper.Install(rightNode);
    Ptr<PacketSink> packetSink = sinkApp.Get(0)->GetObject<PacketSink>();
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
    AddressValue remoteAddress(InetSocketAddress(rightIP, port));
    clientHelper1.SetAttribute("Remote", remoteAddress);
    clientApps1.Add(clientHelper1.Install(leftNode));
    clientApps1.Start(iFlow * flowStartupWindow / numNodes + startTime);
    clientApps1.Stop(stopTime);

    return packetSink;
}

void
IncrementCurrentRecordingStep()
{
    recordingStep++;
    recordingQueueSizeStep = 0;
    if (recordingInterval*recordingStep <= stopTime) {
        Simulator::Schedule (recordingInterval, &IncrementCurrentRecordingStep);
    }
}

void TraceSink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    sinkBytes[recordingStep][index] += p->GetSize();
}
void MarkEvent(Ptr<const QueueDiscItem> item, const char* reason)
{
    auto p = DynamicCast<const Ipv4QueueDiscItem, const QueueDiscItem>(item);
    auto ipMark = p->GetHeader().GetSource().Get();
    auto i = ipToIndex[ipMark];
    markBytes[recordingStep][i]++;
}


void PrintThroughput()
{
    throughPuts << "   # how many M-bits received for each node" <<std::endl;
    for (std::size_t j = 0; j < numNodes; j++)
    {
        throughPuts << std::setw(3) << j << " " ;
        for (std::size_t i = 0; i < sinkBytes.size(); ++i)
        {

            throughPuts << "  "<< std::setw(6)
                        <<  std::fixed << std::showpoint << std::setprecision(1)
                        << (sinkBytes[i][j] * 8) / 1e6;

        }
        throughPuts << std::endl;
    }

    throughPuts << std::setw(3) << numNodes  << " " ;
    for (std::size_t i = 0; i < sinkBytes.size(); ++i)
    {
        uint64_t total = 0;
        for (std::size_t j = 0; j < numNodes; ++j)
        {
            total += sinkBytes[i][j];
        }
        throughPuts << "  "<< std::setw(6)
                    <<  std::fixed << std::showpoint << std::setprecision(1)
                    << (total * 8) / 1e6 ;
    }
}

void PrintMark()
{
    auto data = markBytes;
    std::cout << "\n\n" <<std::endl;
    std::cout << "  # how many packets get marked for each node" << std::endl;
    for (std::size_t j = 0; j < numNodes; j++)
    {
        throughPuts << std::setw(3) << j << " " ;
        for (std::size_t i = 0; i < data.size(); ++i)
        {

            throughPuts << "  "<< std::setw(6)
                        <<  std::fixed << std::showpoint << std::setprecision(1)
                        << data[i][j]  ;

        }
        throughPuts << std::endl;
    }

    throughPuts << std::setw(3) << numNodes  << " " ;
    for (std::size_t i = 0; i < data.size(); ++i)
    {
        uint64_t total = 0;
        for (std::size_t j = 0; j < numNodes; ++j)
        {
            total += data[i][j];
        }
        throughPuts << "  "<< std::setw(6)
                    <<  std::fixed << std::showpoint << std::setprecision(1)
                    << total;
    }
}

void PrintQueueSize()
{
    auto data = queueSize;
    std::cout << "\n\n\n";
    for (std::size_t j = 0; j < recordQueueTotal; j++)
    {
        queueLength << std::setw(3) << j << " " ;
        for (std::size_t i = 0; i < data.size(); ++i)
        {

            queueLength << "  "<< std::setw(6)
                        <<  std::fixed << std::showpoint << std::setprecision(1)
                        << data[i][j]  ;

        }
        queueLength << std::endl;
    }
}

void CheckQueueSize(Ptr<QueueDisc> queue)
{

    // 1500 byte packets
    uint32_t qSize = queue->GetNPackets();
    queueSize[recordingStep][recordingQueueSizeStep++] = qSize;
    Simulator::Schedule(recordQueueSizeInterval, &CheckQueueSize, queue);
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


