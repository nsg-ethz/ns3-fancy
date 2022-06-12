/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef TRAFFIC_APP_INSTALL_HELPERS_H
#define TRAFFIC_APP_INSTALL_HELPERS_H

#include <string.h>
#include <string>
#include "ns3/network-module.h"
#include <unordered_map>
#include <vector>

namespace ns3 {

void
InstallSink(Ptr<Node> node, uint16_t sinkPort, uint32_t duration, std::string protocol);
std::unordered_map <std::string, std::vector<uint16_t>>
InstallSinks(NodeContainer receivers, uint16_t start_port, uint16_t end_port, uint32_t duration, std::string protocol);

Ptr<Socket> InstallSimpleSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t sinkPort, DataRate dataRate, uint32_t numPackets, std::string protocol);

void InstallBulkSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size, double startTime,
					 Ptr<OutputStreamWrapper> fctFile = NULL, Ptr<OutputStreamWrapper> counterFile = NULL,
					 Ptr<OutputStreamWrapper> flowsFile = NULL, uint64_t flowId = 0,
					 uint64_t *recordedFlowsCounter = NULL, double *startRecordingTime = NULL,
					 double recordingTime = -1);

void InstallNormalBulkSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size, double startTime);
void InstallOnOffSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, DataRate dataRate, uint32_t packet_size, uint64_t max_size, double startTime);
void InstallRateSend(Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint32_t n_packets, uint64_t max_size, double duration, double rtt, double startTime);

void InstallRateSend(Ptr<Node> srcHost, std::string dst, uint16_t dport, uint32_t n_packets, uint64_t max_size, double duration, double rtt, double startTime, std::string protocol);

}

#endif /* TRAFFIC_APP_INSTALL_HELPERS_H */

