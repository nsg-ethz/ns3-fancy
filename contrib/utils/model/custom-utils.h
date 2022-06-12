/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef SWIFT_UTILS_H
#define SWIFT_UTILS_H

#include <vector>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include "ns3/core-module.h"
#include "ns3/network-module.h"

namespace ns3 {

    struct FlowMetadata {
        std::string prefix;
        uint32_t packets;
        double duration;
        uint64_t bytes;
        double rtt;
        double start_time;
        uint8_t protocol;
    };

    struct PrefixFeatures {
      double loss = 0;
      uint16_t minBurst = 1;
      uint16_t maxBurst = 1;
      double out_of_order = 0;
    };

    struct PrefixMetadata {
        std::string trace_prefix;
        PrefixFeatures features;
        NetDeviceContainer link;
        Ptr<Node> server;
    };

    struct FailureEvent {
        double failure_time = -1;
        double recovery_time = -1;
        float failure_intensity = 0;
    };

    struct PrefixMappings {
        std::unordered_map<std::string, std::string> sim_to_trace;
        std::unordered_map<std::string, std::set<std::string>> trace_to_sim;
        std::set<std::string> trace_set;
        std::set<std::string> sim_set;
    };
    

    /* for fancy */
    std::vector<FlowMetadata> GetFlowsPerPrefix(std::string flows_per_prefix_file);
    /* for fancy resubmission */
    std::vector<FlowMetadata> GetFlowsPerPrefixFromDist(std::string flows_per_prefix_file, std::unordered_set<uint32_t> filter);
    /* for blink */
    std::vector<FlowMetadata> GetFlowsPerPrefix(std::string flows_per_prefix_file, std::unordered_map<std::string, std::set<std::string>> trace_to_sim_prefixes);

    std::unordered_map<std::string, std::vector<FailureEvent>> GetPrefixFailures(std::string prefix_failure_file, std::string subnetwork_name);
    std::vector<double> GetSubnetworkRtts(std::string rttsFile, std::string subnet_name);
    PrefixMappings GetSubnetworkPrefixMappings(std::string prefixesFile, std::string subnetwork_name);
    std::unordered_map<std::string, PrefixFeatures> GetPrefixFeatures(std::string prefixFeaturesFile, std::set<std::string> subnetwork_trace_prefixes);
    std::unordered_map<std::string, PrefixMetadata> LoadPrefixesMetadata
            (PrefixMappings mappings, std::unordered_map<std::string, PrefixFeatures> trace_prefixes_features);
    
    std::unordered_set<uint32_t> GetPrefixesFromDistSet(std::string flow_dist_file);

    std::vector<uint32_t> GetPrefixesFromDistVector(std::string flow_dist_file);
        

    //New headers
    template<typename T>
    std::vector<T> ReadLines(std::string fileName, uint32_t maxLines = 20000000) {

        std::vector<T> lines;
        T line;
        uint32_t line_count = 0;

        std::ifstream file_in(fileName);
        NS_ASSERT_MSG(file_in, "Invalid File " + fileName);

        while (file_in >> line and (line_count < maxLines or maxLines == 0)) {
            lines.push_back(line);
            line_count++;
        }
        return lines;
    }

}

#endif /* SWIFT_UTILS_H */

