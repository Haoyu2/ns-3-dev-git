#include "utils.h"


#include <filesystem>
#include <iostream>


void Init()
{
    cm_ips[SEND1] = {};
    cm_ips[SEND2] = {};
    cm_ips[SEND3] = {};
    std::string RESULT_DIR = std::string (PROJECT_SOURCE_PATH) + "/A";
    // Q1 is DCTCP queue
    std::string folder = RESULT_DIR + "/multi-hop-topology/dctcp-queue/"
                                + std::to_string(MaxTH_SWITCH1) + "-"
                                + std::to_string(MaxTH_SWITCH2);
    CreateFolderIfNotExists(folder.c_str());
    rxS1R1Throughput.open(  folder + "/dctcp-example-s1-r1-throughput.dat", std::ios::out);
    rxS1R1Throughput << "#Time(s) flow thruput(Mb/s)" << std::endl;
    rxS2R2Throughput.open(folder + "/dctcp-example-s2-r2-throughput.dat", std::ios::out);
    rxS2R2Throughput << "#Time(s) flow thruput(Mb/s)" << std::endl;
    rxS3R1Throughput.open(folder + "/dctcp-example-s3-r1-throughput.dat", std::ios::out);
    rxS3R1Throughput << "#Time(s) flow thruput(Mb/s)" << std::endl;
    fairnessIndex.open(folder + "/dctcp-example-fairness.dat", std::ios::out);
    t1QueueLength.open(folder + "/dctcp-example-t1-length.dat", std::ios::out);
    t1QueueLength << "#Time(s) qlen(pkts) qlen(us)" << std::endl;
    t2QueueLength.open(folder + "/dctcp-example-t2-length.dat", std::ios::out);
    t2QueueLength << "#Time(s) qlen(pkts) qlen(us)" << std::endl;
    ecnMarkStats.open(folder + "/ecn_marks_flow_counts.dat", std::ios::out);
}

void CLI(CommandLine& cmd, int argc, char* argv[])
{
    cmd.AddValue("MaxTH_SWITCH1", "ns-3 TCP TypeId", MaxTH_SWITCH1);
    cmd.AddValue("MaxTH_SWITCH2", "ns-3 TCP TypeId", MaxTH_SWITCH2);
    cmd.AddValue("tcpTypeId", "ns-3 TCP TypeId", tcpTypeId);
    cmd.AddValue("flowStartupWindow",
                 "startup time window (TCP staggered starts)",
                 flowStartupWindow);
    cmd.AddValue("convergenceTime", "convergence time", convergenceTime);
    cmd.AddValue("measurementWindow", "measurement window", measurementWindow);
    cmd.AddValue("enableSwitchEcn", "enable ECN at switches", enableSwitchEcn);
    cmd.Parse(argc, argv);
}

void NS3DefaultConfig()
{
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", StringValue("ns3::" + tcpTypeId));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::DelAckCount", UintegerValue(2));
    GlobalValue::Bind("ChecksumEnabled", BooleanValue(false));

    // Set default parameters for RED queue disc
    Config::SetDefault("ns3::MyRedQueueDisc::UseEcn", BooleanValue(enableSwitchEcn));
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

void
IncrementCurrentRecordingStep()
{
    for (std::size_t i = 0; i < 10; i++)
    {
        rxS1R1Bytes[i] = 0;
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        rxS2R2Bytes[i] = 0;
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        rxS3R1Bytes[i] = 0;
    }
    T1DEQUEUE = 0;
    T2DEQUEUE = 0;
    T1DequeueStats.nBits = 0;
    T1DequeueStats.nPackets = 0;
    T1DequeueStats.sojourn = Time(0);
    T2DequeueStats.nBits = 0;
    T2DequeueStats.nPackets = 0;
    T2DequeueStats.sojourn = Time(0);
}


void
CreateFolderIfNotExists(const char* path)
{
    // Define the path to the folder
    std::filesystem::path folderPath(path);

    // Check if the folder exists
    if (!std::filesystem::exists(folderPath))
    {
        // Create the folder
        if (std::filesystem::create_directories(folderPath))
        {
            std::cout << "Folder created successfully: " << folderPath << std::endl;
        }
        else
        {
            std::cerr << "Failed to create folder: " << folderPath << std::endl;
        }
    }
    else
    {
        std::cout << "Folder already exists: " << folderPath << std::endl;
    }
}

void PrintProgress(Time interval)
{
    static const clock_t begin_time = std::clock();
    double real = float(std::clock() - begin_time) / CLOCKS_PER_SEC;
    std::cout << "Progress to " << std::fixed << std::setprecision(1)
              << Simulator::Now().GetSeconds() << " seconds simulation time\t"
              << Time(std::to_string(real) + "s").GetMinutes() << " real time" << std::endl;
    Simulator::Schedule(interval, &PrintProgress, interval);
}

void TraceS1R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    rxS1R1Bytes[index] += p->GetSize();
}

void TraceS2R2Sink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    rxS2R2Bytes[index] += p->GetSize();
}

void TraceS3R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a)
{
    rxS3R1Bytes[index] += p->GetSize();
}


void PrintThroughput()
{

    rxS1R1Throughput << std::fixed << std::setprecision(3)
                        << Simulator::Now().GetSeconds() << "s ";
    rxS2R2Throughput << std::fixed << std::setprecision(3)
                     << Simulator::Now().GetSeconds() << "s ";
    rxS3R1Throughput << std::fixed << std::setprecision(3)
                     << Simulator::Now().GetSeconds() << "s ";

    for (std::size_t i = 0; i < 10; i++)
    {
        rxS1R1Throughput << std::fixed << std::setprecision(3)
            << (rxS1R1Bytes[i] * 8) / (measurementWindowTP.GetSeconds()) / 1e6
                         << "\t\t";
    }
    rxS1R1Throughput << std::endl;
    for (std::size_t i = 0; i < 20; i++)
    {
        rxS2R2Throughput << std::fixed << std::setprecision(3)
            << (rxS2R2Bytes[i] * 8) / (measurementWindowTP.GetSeconds()) / 1e6
                         << "\t\t";
    }
    rxS2R2Throughput << std::endl;
    for (std::size_t i = 0; i < 10; i++)
    {
        rxS3R1Throughput << std::fixed << std::setprecision(3)
            << (rxS3R1Bytes[i] * 8) / (measurementWindowTP.GetSeconds()) / 1e6
                         << "\t\t";
    }
    rxS3R1Throughput << std::endl;
    IncrementCurrentRecordingStep();
    if (Simulator::Now() < (flowStartupWindow + convergenceTime + Seconds(0.2)))
    {
        Simulator::Schedule( measurementWindowTP, &PrintThroughput);
    }

}


// Jain's fairness index:  https://en.wikipedia.org/wiki/Fairness_measure
void PrintFairness()
{
    double average = 0;
    uint64_t sumSquares = 0;
    uint64_t sum = 0;
    double fairness = 0;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += rxS1R1Bytes[i];
        sumSquares += (rxS1R1Bytes[i] * rxS1R1Bytes[i]);
    }
    average = ((sum / 10) * 8 / measurementWindow.GetSeconds()) / 1e6;
    fairness = static_cast<double>(sum * sum) / (10 * sumSquares);
    fairnessIndex << "Average throughput for S1-R1 flows: " << std::fixed << std::setprecision(2)
                  << average << " Mbps; fairness: " << std::fixed << std::setprecision(3)
                  << fairness << std::endl;
    average = 0;
    sumSquares = 0;
    sum = 0;
    fairness = 0;
    for (std::size_t i = 0; i < 20; i++)
    {
        sum += rxS2R2Bytes[i];
        sumSquares += (rxS2R2Bytes[i] * rxS2R2Bytes[i]);
    }
    average = ((sum / 20) * 8 / measurementWindow.GetSeconds()) / 1e6;
    fairness = static_cast<double>(sum * sum) / (20 * sumSquares);
    fairnessIndex << "Average throughput for S2-R2 flows: " << std::fixed << std::setprecision(2)
                  << average << " Mbps; fairness: " << std::fixed << std::setprecision(3)
                  << fairness << std::endl;
    average = 0;
    sumSquares = 0;
    sum = 0;
    fairness = 0;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += rxS3R1Bytes[i];
        sumSquares += (rxS3R1Bytes[i] * rxS3R1Bytes[i]);
    }
    average = ((sum / 10) * 8 / measurementWindow.GetSeconds()) / 1e6;
    fairness = static_cast<double>(sum * sum) / (10 * sumSquares);
    fairnessIndex << "Average throughput for S3-R1 flows: " << std::fixed << std::setprecision(2)
                  << average << " Mbps; fairness: " << std::fixed << std::setprecision(3)
                  << fairness << std::endl;
    sum = 0;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += rxS1R1Bytes[i];
    }
    for (std::size_t i = 0; i < 20; i++)
    {
        sum += rxS2R2Bytes[i];
    }
    fairnessIndex << "Aggregate user-level throughput for flows through T1: "
                  << static_cast<double>(sum * 8) / 1e9 << " Gbps" << std::endl;
    sum = 0;
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += rxS3R1Bytes[i];
    }
    for (std::size_t i = 0; i < 10; i++)
    {
        sum += rxS1R1Bytes[i];
    }
    fairnessIndex << "Aggregate user-level throughput for flows to R1: "
                  << static_cast<double>(sum * 8) / 1e9 << " Gbps" << std::endl;
}

void CheckT1QueueSize(Ptr<QueueDisc> queue)
{

    // 1500 byte packets
    uint32_t qSize = queue->GetNPackets();
    Time backlog = Seconds(static_cast<double>(qSize * 1500 * 8) / 1e10); // 10 Gb/s
    // report size in units of packets and ms
    Time sojourn = T1DequeueStats.nPackets == 0 ? Time(0) : (T1DequeueStats.sojourn / T1DequeueStats.nPackets);
    t1QueueLength << std::fixed << std::setprecision(2) << Simulator::Now().GetSeconds()
                  << "  " <<std::setw(5) << qSize
                  << "  " <<std::setw(5) << backlog.GetMicroSeconds()
                  << "  " <<std::setw(5) << sojourn.GetMicroSeconds()
                  << "  " <<std::setw(5) << T1DequeueStats.nBits / MilliSeconds(10).GetSeconds() / 1e6
                  << "  " <<std::setw(5) << T1DequeueStats.nPackets
                  << std::endl;
    T1DequeueStats.nBits = 0;
    T1DequeueStats.nPackets = 0;
    T1DequeueStats.sojourn = Time(0);
    // check queue size every 1/100 of a second
    Simulator::Schedule(MilliSeconds(10), &CheckT1QueueSize, queue);
}

void
CheckT2QueueSize(Ptr<QueueDisc> queue)
{
    uint32_t qSize = queue->GetNPackets();
    Time backlog = Seconds(static_cast<double>(qSize * 1500 * 8) / 1e9); // 1 Gb/s
    Time sojourn = T2DequeueStats.nPackets == 0 ? Time(0) : (T2DequeueStats.sojourn / T2DequeueStats.nPackets);

    // report size in units of packets and ms
    t2QueueLength << std::fixed << std::setprecision(2) << Simulator::Now().GetSeconds()
                  << " "<<std::setw(5) << qSize
                  << " "<<std::setw(5) << backlog.GetMicroSeconds()
                  << "  " <<std::setw(5) << sojourn.GetMicroSeconds()
                  << "  " <<std::setw(5) << T2DequeueStats.nBits / MilliSeconds(10).GetSeconds() / 1e6
                  << "  " <<std::setw(5) << T2DequeueStats.nPackets
                  << std::endl;
    T2DequeueStats.nBits = 0;
    T2DequeueStats.nPackets = 0;
    T2DequeueStats.sojourn = Time(0);
    // check queue size every 1/100 of a second
    Simulator::Schedule(MilliSeconds(10), &CheckT2QueueSize, queue);
}



// ip to string for packet, little endian
std::string IPV4ToStringPacket(uint32_t m_address){
    std::string res = std::to_string((m_address >> 0) & 0xff);
    res += ".";
    res += std::to_string((m_address >> 8) & 0xff);
    res += ".";
    res += std::to_string((m_address >> 16) & 0xff);
    res += ".";
    res += std::to_string((m_address >> 24) & 0xff);
    return res;
}

// ip to string for node, big endian
std::string IPV4ToStringNode(uint32_t m_address){
    std::string res = std::to_string((m_address >> 24) & 0xff);
    res += ".";
    res += std::to_string((m_address >> 16) & 0xff);
    res += ".";
    res += std::to_string((m_address >> 8) & 0xff);
    res += ".";
    res += std::to_string((m_address >> 0) & 0xff);
    return res;
}

void MarkEventT2R1(Ptr<const QueueDiscItem> item, const char* reason)
{

    Ptr<const Ipv4QueueDiscItem> media =
        Ptr<const Ipv4QueueDiscItem>(dynamic_cast<const Ipv4QueueDiscItem*>(PeekPointer(item)));
    media->GetHeader().GetDestination();
    uint32_t address = media->GetHeader().GetSource().Get();
    std::string key = IPV4ToStringPacket(address);
    counters[key]++;
}

void MarkEvent(CM cm, Ptr<const QueueDiscItem> item, const char* reason)
{

    Ptr<const Ipv4QueueDiscItem> media =
        Ptr<const Ipv4QueueDiscItem>(dynamic_cast<const Ipv4QueueDiscItem*>(PeekPointer(item)));
    media->GetHeader().GetDestination();
    uint32_t address = media->GetHeader().GetSource().Get();
    std::string key = IPV4ToStringNode(address);
    ecn_counter[cm][key]++;
}

void DequeueEvent(CM cm, Ptr<const QueueDiscItem> item)
{
    if (cm == SWITCH1) {
        T1DequeueStats.nPackets++;
        T1DequeueStats.nBits += item->GetSize();
        T1DequeueStats.sojourn += Simulator::Now() - item->GetTimeStamp();
    }
    if (cm == SWITCH2) {
        T2DequeueStats.nPackets++;
        T2DequeueStats.nBits += item->GetSize();
        T2DequeueStats.sojourn += Simulator::Now() - item->GetTimeStamp();
    }
}
