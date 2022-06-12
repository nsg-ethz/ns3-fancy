/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef TRAFFIC_SCHEDULER_H
#define TRAFFIC_SCHEDULER_H

#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/utils-module.h"
#include <unordered_map>
#include <vector>

namespace ns3 {

    struct TrafficPrefixStats {
        uint32_t packets;
        uint64_t bytes;
        uint32_t flows;
        double first_packet;
        double last_packet;
        double first_packet_after_failure;
    };
    /* Traffic scheduler to evaluate the loss of signal after a failure, this function in based on StatefulSyntheticTrafficScheduler */
    std::vector<FlowMetadata>
        StatefulSyntheticTrafficSchedulerOneShot(
            std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node, double rtt,
            uint32_t seed, uint32_t total_flows, uint32_t prefixes, DataRate bw, double start_time, double warm_up_time,
            double send_duration, uint32_t elephant_flows, double elephant_byte_share,
            uint16_t start_port, uint16_t end_port, std::string output_file, double udp_share);
    /* Traffic scheduler used for E2 and pure synthetic TCP flows */
    std::vector<FlowMetadata> StatefulSyntheticTrafficScheduler(
        std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node,
        double rtt, uint32_t seed, uint32_t flows_per_sec, uint32_t prefixes,
        DataRate bw, double start_time, double duration, uint16_t start_port, uint16_t end_port,
        std::string output_file, double udp_share);

    /* This will be used to emulate some caida trace and its based on some statistics */
    std::unordered_map<std::string, TrafficPrefixStats>
        StatefulTrafficScheduler(
            std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node,
            std::vector<double> rtt_cdf, std::string flowDistFile, uint32_t seed, uint32_t flows_per_sec,
            double start_time, double duration, double failure_time, uint16_t start_port, uint16_t end_port,
            std::string output_file);

    std::unordered_map<std::string, TrafficPrefixStats>
        StatefulTraceTrafficScheduler(std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node,
            std::vector<double> rtt_cdf, std::string flowDistFile,
            uint32_t seed, double start_time, double duration,
            double failure_time, uint16_t start_port, uint16_t end_port, std::string output_file);

    std::unordered_map<std::string, TrafficPrefixStats>
        HybridTrafficScheduler(std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node,
            std::unordered_set<uint32_t> prefixes_to_send, std::vector<double> rtt_cdf, double rtt_shift,
            std::string flowDistFile, uint32_t seed, double start_time, double duration,
            double failure_time, uint16_t start_port, uint16_t end_port,
            std::string output_file);

}
#endif /* TRAFFIC_S CHEDULER_H */
