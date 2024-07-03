#include "my-config.h"

using namespace ns3;


// REM means MyRedQueueDisc
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
                               StringValue(dataRateNeck),
                               "LinkDelay",
                               StringValue(delayNeck),
                               "MinTh",
                               DoubleValue(threshold),
                               "MaxTh",
                               DoubleValue(threshold));

    TrafficControlHelper leafQueue;
    leafQueue.SetRootQueueDisc("ns3::MyRedQueueDisc",
                               "LinkBandwidth",
                               StringValue(dataRateLeaf),
                               "LinkDelay",
                               StringValue(delayLeaf),
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


// traffic control layer MyQueue
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyQueueDisc()
{
    SetDefaultMyQueueDisc();
    TrafficControlHelper neckQueue;
    neckQueue.SetRootQueueDisc("ns3::MyQueueDisc",
                               "LinkBandwidth",
                               StringValue(dataRateNeck),
                               "LinkDelay",
                               StringValue(delayNeck),
                               "QueueCounterTotal",
                               UintegerValue(counterTotal),
                               "MinTh",
                               DoubleValue(threshold),
                               "MaxTh",
                               DoubleValue(threshold));

    TrafficControlHelper leafQueue;
    leafQueue.SetRootQueueDisc("ns3::MyQueueDisc",
                               "LinkBandwidth",
                               StringValue(dataRateLeaf),
                               "LinkDelay",
                               StringValue(delayLeaf),
                               "MinTh",
                               DoubleValue(threshold),
                               "MaxTh",
                               DoubleValue(threshold));

    return {neckQueue, leafQueue};
}
