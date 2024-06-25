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
#include "ns3/my-utils.h"
#include "ns3/config-store.h"
#include <cstdint>

using namespace ns3;

/*
 *  Testing Node and Queue Marking Threshold
 *
 * */

inline uint32_t numNodes = 20;
inline uint32_t threshold = 10;
/*
 *  Testing Time
 * */
inline Time flowStartupWindow = Seconds(1);
inline Time convergenceTime = Seconds(3);
inline Time measurementWindow = Seconds(3);
inline Time startTime = Seconds(0);
inline int duration = 3;
inline Time stopTime = Seconds(duration);
inline Time progressInterval = MilliSeconds(100);

/*
 *  Testing Results Directory
 *
 * */
// Results folder
inline auto  data_dir = RESULT_DIR + "/dctcp_queue_dumbbell";

inline std::ofstream throughPuts; // throughput record
inline std::ofstream queueLength; // queueLength record

inline int recordingStep = 0;     //
inline Time recordingInterval = Seconds(0.5);     //
inline Time recordQueueSizeInterval = MilliSeconds(20);
inline uint64_t recordQueueTotal = (recordingInterval / recordQueueSizeInterval).GetInt();
inline std::vector<std::vector<uint64_t>> sinkBytes(duration*2+2, std::vector<uint64_t>(numNodes));
inline std::vector<std::vector<uint64_t>> markBytes(duration*2+2, std::vector<uint64_t>(numNodes));

inline int recordingQueueSizeStep = 0;
inline std::vector<std::vector<uint64_t>> queueSize(duration*2+2, std::vector<uint64_t>(recordQueueTotal));
inline std::map<uint32_t, uint32_t> ipToIndex;


/*
 *  Setting Functions
 *
 * */
void Init();
std::tuple<PointToPointHelper, PointToPointHelper> SetPointToPointHelper();
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyRedQueueDisc();
void SetDefaultConfigDCTCP();
void SetDefaultMyRedQueueDisc();
Ptr<PacketSink> SetupFlow(int iFlow, Ptr<Node> leftNode, Ipv4Address rightIP, Ptr<Node> rightNode);
void TraceSink(std::size_t index, Ptr<const Packet> p, const Address& a);
void MarkEvent(Ptr<const QueueDiscItem> item, const char* reason);
void IncrementCurrentRecordingStep();
void PrintThroughput();
void PrintMark();
void PrintProgress(Time interval);
void CheckQueueSize(Ptr<QueueDisc> queue);
void PrintQueueSize();
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyQueueDisc();
#endif // NS3_CONFIG_H
