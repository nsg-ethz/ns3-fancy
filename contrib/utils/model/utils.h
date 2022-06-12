/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef UTILS_H
#define UTILS_H

#include <string.h>
#include <cmath>
#include <fstream>
#include <string>
#include <vector>
#include <unordered_map>
#include <algorithm>

#include "sys/types.h"
#include <ctime>

/* Operating dependant imports */
#ifdef __APPLE__
    #include <vector>
#elif __linux__
    #include "sys/sysinfo.h"
#endif

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mobility-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/csma-module.h"

namespace ns3 {

/* ... */
extern Ptr<UniformRandomVariable> random_variable;

/* Allows us to use the TOS field for multiple things */
uint8_t getTosHi(uint8_t tos);
uint8_t getTosLo(uint8_t tos);
uint8_t setTosHi(uint8_t tos, uint8_t tos_hi);
struct IpMask {
  std::string ip;
  std::string mask;
};

Ipv4Address GetNodeIp(std::string node_name);

Ipv4Address GetNodeIp(Ptr<Node> node);

Ptr<NetDevice> GetNodeNetDevice(Ptr<Node> node);
Ptr<NetDevice> GetNodeNetDevice(std::string node_name);

Ptr<Node> GetNode(std::string name);

std::string GetNodeName(Ptr<Node> node);

std::string Ipv4AddressToString(Ipv4Address address);

IpMask GetIpMask(std::string prefix);

uint64_t BytesFromRate(DataRate dataRate, double time);

std::vector<std::pair<double, uint64_t>> GetDistribution(std::string distributionFile);

uint64_t GetFlowSizeFromDistribution(std::vector<std::pair<double, uint64_t>> distribution, double uniformSample);

std::pair<uint16_t, uint16_t> GetHostPositionPair(std::string name);

void PrintNow(double delay);

void PrintMemUsage(double delay);

void PrintNowMem(double delay, std::clock_t starting_time);

void PrintNowMemStop(double delay, std::clock_t starting_time, double stop_after);

void SaveNow(double delay, Ptr<OutputStreamWrapper> file);

uint64_t HashString(std::string message);

double MeasureInterfaceLoad(Ptr<Queue<Packet>> q, double next_schedule, std::string name, DataRate linkBandwidth);

void PrintQueueSize(Ptr<Queue<Packet>> q);

void SetUniformDropRate(NetDeviceContainer link_to_change, double drop_rate);

void SetFlowErrorModel(NetDeviceContainer link);

void ChangeFlowErrorDropRate(NetDeviceContainer link, double drop_rate);

void ClearFlowErrorModel(NetDeviceContainer link);

void SetFlowErrorNormalDropRate(NetDeviceContainer link, double drop_rate);

void SetFlowErrorNormalBurstSize(NetDeviceContainer link, uint16_t min, uint16_t max);

void SetFlowErrorModelFromFeatures(NetDeviceContainer link, double flow_drop_rate, double normal_drop_rate, uint16_t minBurst, uint16_t maxBurst);

void FailLink(NetDeviceContainer link_to_fail);

void RecoverLink(NetDeviceContainer link_to_recover);

void LinkUp(NetDeviceContainer link);

void LinkDown(NetDeviceContainer link);

uint64_t LeftMostPowerOfTen(uint64_t number);

//gets a random element from a vector!
template<typename T>
T RandomFromVector(std::vector<T> &vect, Ptr<UniformRandomVariable> rand) {
  return vect[rand->GetInteger(0, vect.size() - 1)];
}

double FindClosest(std::vector<double> vect, double value);

Ptr<Queue<Packet>> GetPointToPointNetDeviceQueue(PointToPointNetDevice netDevice);

bool file_exists(const std::string &name);

std::string data_rate_to_str(DataRate rate);

}

#endif /* UTILS_H */

