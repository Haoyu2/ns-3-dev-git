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
#include "ns3/my-queue-disc.h"
#include <cstdint>

using namespace ns3;

#define REM "REM"
#define MYQ "MyQueue"
#define RED "RED"

typedef std::tuple<TrafficControlHelper, TrafficControlHelper> (*SetupQueueFunction)(); // function pointer type
inline std::string  activeQueue = REM ;
inline std::map<std::string, SetupQueueFunction> supportQueue;
inline Time abrupt = Seconds(0);
/*
 *  Testing Node and Queue Marking Threshold
 *
 * */

inline uint32_t numNodes = 2;
inline uint32_t counterTotal = 1000;
inline uint32_t threshold = 40;
/*
 *  Testing Time
 * */
// if flowStartupWindow == 0 every flow starts at same time
// else flow starts gradually within this window
inline Time flowStartupWindow = Seconds(1);
inline Time stopTime = Seconds(3);
inline Time progressInterval = MilliSeconds(100);
inline Time startRecordingTP = Seconds(0);
inline Time stopRecordingTP = stopTime;
inline Time intervalRecordingTP = Seconds(0.5);

// these row length has to be the same
inline std::vector<std::vector<uint64_t>> sinkBytes;
inline std::vector<std::vector<uint64_t>> clientBytes(1, std::vector<uint64_t>(10));
inline std::vector<std::vector<uint64_t>> markBytes;


inline Time startRecordingQS = Seconds(0);
inline Time stopRecordingQS = stopTime;
inline Time intervalRecordingQS = MilliSeconds(10);
inline std::vector<uint32_t> recordsQS;
inline std::vector<std::tuple<uint32_t, uint32_t, uint32_t, uint32_t>> queueCounterStats;

inline std::string dataRateNeck = "10Gbps";
inline std::string dataRateLeaf = "1Gbps";
inline std::string delayNeck = "10us";
inline std::string delayLeaf = "10us";

/*
 *  Testing Results Directory
 *
 * */
// Results folder
inline auto  data_dir = RESULT_DIR + "/figure16";

inline int widthRecordTimeTP = 4;
inline int widthRecordTimeQS = 6;
inline std::ofstream throughPutStream; // throughput record
inline int widthThroughPuts = 8;
inline std::ofstream queueLengthStream; // queueLength record
inline int widthQueueSize = 8;

inline std::vector<std::string> throughPutString;
inline std::vector<std::string> queueLengthString;



inline std::map<uint32_t, uint32_t> ipToIndex;

    /*
 *  Setting Functions
 *
 * */
void Set_CLI_Args(CommandLine& cmd, int argc, char* argv[]);
void Init(int argc, char* argv[]);
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
void CheckThroughputAndMarking();
void PrintQueueSize();
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFMyQueueDisc();
void InitVector(std::vector<uint64_t> v);
void DumpQueueSize(std::ofstream &output);
void DumpThroughput(std::ofstream &output, std::vector<std::vector<uint64_t>> data, bool );
void DumpVectorOfVector(std::ofstream &output, std::vector<std::vector<uint64_t>> data);
void TraceClient(std::size_t index, Ptr<const Packet> p);
std::tuple<TrafficControlHelper, TrafficControlHelper> SetTFRedQueueDisc();
#endif // NS3_CONFIG_H
