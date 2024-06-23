#ifndef MY_UTILS_H
#define MY_UTILS_H

#include <fstream>
#include <sstream>
#include <string>
#include <vector>
#include "ns3/packet.h"
#include "ns3/nstime.h"
#include "ns3/simulator.h"
#include "ns3/core-module.h"

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

enum CM{SEND1,SEND2,SEND3,SWITCH1,SWITCH2,RECEIVER1,RECEIVER2};

inline int S1_R1_FLOWS = 10, S2_R2_FLOWS = 20, S3_R1_FLOWS = 10;

inline char MY_RedQueueDisc[] = "ns3::MyRedQueueDisc";
inline char RedQueueDisc[] = "ns3::RedQueueDisc";
inline char* BottleNeckQueueDisc = MY_RedQueueDisc;

inline int MinTH_SWITCH1 = 40;
inline int MaxTH_SWITCH1 = 75;
inline int MinTH_SWITCH2 = 20;
inline int MaxTH_SWITCH2 = 55;




inline std::string outputFilePath = ".";
inline std::string tcpTypeId = "TcpDctcp";
inline Time flowStartupWindow = Seconds(1);
inline Time convergenceTime = Seconds(3);
inline Time measurementWindow = Seconds(1);
inline bool enableSwitchEcn = true;
inline Time progressInterval = MilliSeconds(100);


inline std::stringstream filePlotQueue1;
inline std::stringstream filePlotQueue2;

// Throughput data stream on receiver
inline std::ofstream rxS1R1Throughput;
inline std::ofstream rxS2R2Throughput;
inline std::ofstream rxS3R1Throughput;


inline std::ofstream fairnessIndex;
inline std::ofstream t1QueueLength;
inline std::ofstream t2QueueLength;
inline std::ofstream ecnMarkStats;

inline std::vector<uint64_t> rxS1R1Bytes(S1_R1_FLOWS);
inline std::vector<uint64_t> rxS2R2Bytes(S2_R2_FLOWS);
inline std::vector<uint64_t> rxS3R1Bytes(S3_R1_FLOWS);

// marking counter for each IP
inline std::map<std::string , long> counters;
inline std::map<CM, std::vector<std::string>> cm_ips;
inline std::map<CM, std::map<std::string, long>> ecn_counter;

inline struct {
    long nPackets;
    long nBits;
    Time sojourn;
} T1DequeueStats, T2DequeueStats;


inline long T1DEQUEUE = 0;
inline long T2DEQUEUE = 0;

void Init();

void CLI(CommandLine& cmd, int argc, char* argv[]);

void NS3DefaultConfig();

// Simulation progress
void PrintProgress(Time interval);

void IncrementCurrentSecond();

void PrintThroughput();

// Jain's fairness index:  https://en.wikipedia.org/wiki/Fairness_measure
void PrintFairness();

void TraceS1R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a);

void TraceS2R2Sink(std::size_t index, Ptr<const Packet> p, const Address& a);

void TraceS3R1Sink(std::size_t index, Ptr<const Packet> p, const Address& a);


void CreateFolderIfNotExists(const char * path);



void CheckT1QueueSize(Ptr<QueueDisc> queue);

void CheckT2QueueSize(Ptr<QueueDisc> queue);

void MarkEventT2R1(Ptr<const QueueDiscItem> item, const char* reason);

void MarkEvent(CM cm, Ptr<const QueueDiscItem> item, const char* reason);

std::string IPV4ToStringNode(uint32_t m_address);
std::string IPV4ToStringPacket(uint32_t m_address);

void DequeueEvent(CM cm, Ptr<const QueueDiscItem> item);

#endif
