//
// Created by haoyu on 6/18/2024.
//

#ifndef NS3_CONFIG_H
#define NS3_CONFIG_H
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/point-to-point-layout-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/assert.h"
#include "ns3/data-rate.h"
#include <cstdint>

using namespace ns3;

inline double threshold = 10;
inline uint32_t numNodes = 20;

inline Time flowStartupWindow = Seconds(1);
inline Time convergenceTime = Seconds(3);
inline Time measurementWindow = Seconds(3);
inline Time startTime = Seconds(0);

inline int duration = 3;
inline Time stopTime = Seconds(duration);
inline Time progressInterval = MilliSeconds(100);

inline std::ofstream throughPuts("throughPutsMyQueue-10.dat");
inline int currentSecond = 0;
inline std::vector<std::vector<uint64_t>> sinkBytes(duration*2+2, std::vector<uint64_t>(numNodes));
inline std::vector<std::vector<uint64_t>> markBytes(duration*2+2, std::vector<uint64_t>(numNodes));
inline std::vector<uint32_t> queueSize;
inline std::map<uint32_t, uint32_t> ipToindex;

void Init();
std::tuple<PointToPointHelper, PointToPointHelper> SetPointToPointHelper();
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyRedQueueDisc();
void SetDefaultConfigDCTCP();
void SetDefaultMyRedQueueDisc();
Ptr<PacketSink> SetupFlow(int iFlow, Ptr<Node> leftNode, Ipv4Address rightIP, Ptr<Node> rightNode);
void TraceSink(std::size_t index, Ptr<const Packet> p, const Address& a);
void MarkEvent(Ptr<const QueueDiscItem> item, const char* reason);
void IncrementCurrentSecond();
void PrintThroughput();
void PrintMark();
void PrintProgress(Time interval);
void CheckQueueSize(Ptr<QueueDisc> queue);
void PrintQueueSize();
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyQueueDisc();
#endif // NS3_CONFIG_H
