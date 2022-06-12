/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

//#include <ns3/custom-utils.h>
#include "custom-utils.h"
#include "utils.h"

NS_LOG_COMPONENT_DEFINE ("custom-utils");


namespace ns3 {

PrefixMappings GetSubnetworkPrefixMappings(std::string prefixesFile, std::string subnetwork_name) {

  //mapping
  PrefixMappings mappings;

  std::ifstream prefixes_file(prefixesFile);
  NS_ASSERT_MSG(prefixes_file, "Please provide a valid prefixes file");

  std::string line;
  std::string strip_a;
  std::string current_sub_test;

  bool subnetwork_found = false;

  std::string sim_prefix;
  std::string trace_prefix;

  while (std::getline(prefixes_file, line)) {

    //skip blank lines
    if (line.empty()) {
      continue;
    }

    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_sub_test;
      if (current_sub_test == subnetwork_name) {
        subnetwork_found = true;
      } else if (subnetwork_found == true) {
        break;
      }
    } else if (subnetwork_found == true) {
      std::istringstream lineStream(line);
      lineStream >> sim_prefix >> trace_prefix;

      //update all the structures
      mappings.trace_set.insert(trace_prefix);
      mappings.sim_set.insert(sim_prefix);
      mappings.sim_to_trace[sim_prefix] = trace_prefix;
      mappings.trace_to_sim[trace_prefix].insert(sim_prefix);
    }
  }
  return mappings;
};

//Reads a file with RTTs.
std::vector<double> GetSubnetworkRtts(std::string rttsFile, std::string subnet_name) {

  std::vector<double> rttVector;
  std::ifstream infile(rttsFile);

  NS_ASSERT_MSG(infile, "Please provide a valid file for reading RTT values");
  double rtt;
  std::string line;
  std::string strip_a;
  std::string current_sub_test;
  bool subnetwork_found = false;

  while (getline(infile, line)) {

    //skip blank lines
    if (line.empty()) {
      continue;
    }

    //Check if the line starts with #
    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_sub_test;
      if (current_sub_test == subnet_name) {
        subnetwork_found = true;
      } else if (subnetwork_found == true) {
        break;
      }
    } else if (subnetwork_found == true) {
      rtt = atof(line.c_str());
      rttVector.push_back(rtt);
    }
  }

  infile.close();
  return rttVector;
}

/*Gets trace prefix features, then we have to change that and set it to the simulation prefixes*/

std::unordered_map<std::string, PrefixFeatures>
GetPrefixFeatures(std::string prefixFeaturesFile, std::set<std::string> subnetwork_trace_prefixes) {

  std::unordered_map<std::string, PrefixFeatures> trace_prefix_features;

  std::ifstream prefix_features_file(prefixFeaturesFile);

  if (!prefix_features_file) {
    NS_LOG_WARN("!!! The features file file was not found !!!");
    return trace_prefix_features;
  }

  std::string prefix;

  PrefixFeatures features;

  while (prefix_features_file >> prefix >> features.loss >> features.minBurst >> features.maxBurst) {
    //checks that the prefix is found in this sub network
    if (subnetwork_trace_prefixes.find(prefix) != subnetwork_trace_prefixes.end()) {

      trace_prefix_features[prefix] = features;
    }
  }
  return trace_prefix_features;
};

std::unordered_map<std::string, std::vector<FailureEvent>>
GetPrefixFailures(std::string prefix_failure_file, std::string subnetwork_name) {

  // variables
  std::unordered_map<std::string, std::vector<FailureEvent>> prefix_to_events;

  std::ifstream infile(prefix_failure_file);

  if (!infile) {
    NS_LOG_WARN("!!! The prefix Failure file was not found !!!");
    return prefix_to_events;
  }

  std::string line;
  std::string strip_a;
  std::string current_sub_test;
  bool subnetwork_found = false;

  while (getline(infile, line)) {

    //skip blank lines
    if (line.empty()) {
      continue;
    }

    //Check if the line starts with #
    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_sub_test;
      if (current_sub_test == subnetwork_name) {
        subnetwork_found = true;
      } else if (subnetwork_found == true) {
        break;
      }
    } else if (subnetwork_found == true) {

      std::istringstream lineStream(line);
      std::string prefix;
      lineStream >> prefix;
      FailureEvent event;
      std::vector<FailureEvent> events_vector;
      prefix_to_events[prefix] = events_vector;

      while (lineStream >> event.failure_time >> event.recovery_time >> event.failure_intensity) {
        prefix_to_events[prefix].push_back(event);
      }
    }
  }

  infile.close();
  return prefix_to_events;

};

std::unordered_map<std::string, PrefixMetadata> LoadPrefixesMetadata
  (PrefixMappings mappings, std::unordered_map<std::string, PrefixFeatures> trace_prefixes_features) {

  std::unordered_map<std::string, PrefixMetadata> prefixes;

  for (auto prefix: mappings.sim_set) {
    PrefixMetadata metadata;
    metadata.trace_prefix = mappings.sim_to_trace[prefix];

    //Only if it was provided, otherwise we use default features (all 0)
    if (trace_prefixes_features.find(metadata.trace_prefix) != trace_prefixes_features.end()) {
      metadata.features = trace_prefixes_features[metadata.trace_prefix];
    }

    prefixes[prefix] = metadata;
  }

  return prefixes;
};

/* New function to load prefixes distributions used in the latest version of
Fancy, submission for NSDI 2022 (Fall) */
/* If there is a filter we only pick those prefixes */
std::vector<FlowMetadata> GetFlowsPerPrefixFromDist(std::string flows_per_prefix_file, std::unordered_set<uint32_t> filter) 
{
  std::vector<FlowMetadata> flows;
  std::ifstream flowsDist(flows_per_prefix_file);
  NS_ASSERT_MSG(flowsDist, "Please provide a valid prefixes dist file");

  std::string line;
  FlowMetadata flow;

  std::string current_prefix;
  std::string strip_a, strip_b;

  uint32_t protocol;

  uint32_t line_num = 0;

  bool skip = false;
  while (std::getline(flowsDist, line)) {
    line_num++;  
    //skip blank lines
    if (line.empty()) {
      continue;
    }

    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_prefix >> strip_b;

      uint32_t prefix = (Ipv4Address(current_prefix.c_str()).Get()) & 0xffffff00;
      
      if (filter.size() > 0 && filter.count(prefix) == 0)
      {
        skip = true;
      }
      else 
      {
        skip = false;
      }

    } 
    else if (!skip)
    {
      std::istringstream lineStream(line);
      lineStream >> flow.start_time >> flow.packets >> flow.duration >> flow.bytes >> flow.rtt >> protocol;
      flow.protocol = uint8_t(protocol);
      //std::cout << "line " <<  line_num << " " << protocol << std::endl;
      /* We store an ip address instead of a prefix to be a bit faster*/

      // removes /24
      std::string dst_ip = current_prefix.substr(0, current_prefix.find("/"));
      //replaces last 0 with a 1
      dst_ip = dst_ip.substr(0, dst_ip.length() - 1) + "1";

      flow.prefix = dst_ip;
      flows.push_back(flow);
    }
  }
  flowsDist.close();
  return flows;
}

/* New function to load prefixes distributions used in the latest version of
Fancy, submission for NSDI 2022 (Fall) */
std::unordered_set<uint32_t> GetPrefixesFromDistSet(std::string flow_dist_file) 
{
  std::unordered_set<uint32_t> prefixes;
  std::ifstream flowsDist(flow_dist_file);
  NS_ASSERT_MSG(flowsDist, "Please provide a valid prefixes dist file");

  std::string line;

  std::string current_prefix;
  std::string strip_a, strip_b;


  uint32_t line_num = 0;

  bool skip = false;
  while (std::getline(flowsDist, line)) {
    line_num++;  
    //skip blank lines
    if (line.empty()) {
      continue;
    }

    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_prefix >> strip_b;
      uint32_t prefix = (Ipv4Address(current_prefix.c_str()).Get()) & 0xffffff00;
      prefixes.insert(prefix);
    } 
    else 
    {
     continue;
    }
  }
  flowsDist.close();
  return prefixes;
}

std::vector<uint32_t> GetPrefixesFromDistVector(std::string flow_dist_file) 
{
  std::vector<uint32_t> prefixes;
  std::ifstream flowsDist(flow_dist_file);
  NS_ASSERT_MSG(flowsDist, "Please provide a valid prefixes dist file");

  std::string line;

  std::string current_prefix;
  std::string strip_a, strip_b;


  uint32_t line_num = 0;

  bool skip = false;
  while (std::getline(flowsDist, line)) {
    line_num++;  
    //skip blank lines
    if (line.empty()) {
      continue;
    }

    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_prefix >> strip_b;
      uint32_t prefix = (Ipv4Address(current_prefix.c_str()).Get()) & 0xffffff00;
      prefixes.push_back(prefix);
    } 
    else 
    {
     continue;
    }
  }
  flowsDist.close();
  return prefixes;
}


std::vector<FlowMetadata> GetFlowsPerPrefix(std::string flows_per_prefix_file) 
{
  std::vector<FlowMetadata> flows;
  std::ifstream flowsDist(flows_per_prefix_file);
  NS_ASSERT_MSG(flowsDist, "Please provide a valid prefixes dist file");

  std::string line;
  FlowMetadata flow;

  std::string current_prefix;
  std::string strip_a, strip_b;

  bool skip = false;
  while (std::getline(flowsDist, line)) {

    //skip blank lines
    if (line.empty()) {
      continue;
    }

    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_prefix >> strip_b;
    } 
    else 
    {
      std::istringstream lineStream(line);
      lineStream >> flow.bytes >> flow.duration >> flow.packets >> flow.rtt;
      flow.prefix = current_prefix;
      flows.push_back(flow);
    }
  }
  flowsDist.close();
  return flows;
}

std::vector<FlowMetadata> GetFlowsPerPrefix(std::string flows_per_prefix_file,
                                            std::unordered_map<std::string, std::set<std::string>> trace_to_sim_prefixes) {

  std::vector<FlowMetadata> flows;
  std::ifstream flowsDist(flows_per_prefix_file);
  NS_ASSERT_MSG(flowsDist, "Please provide a valid prefixes dist file");

  std::string line;
  FlowMetadata flow;

  std::string current_prefix;
  std::string strip_a, strip_b;

  bool skip = false;
  while (std::getline(flowsDist, line)) {

    //skip blank lines
    if (line.empty()) {
      continue;
    }

    if (0 == line.find("#")) {
      std::istringstream lineStream(line);
      lineStream >> strip_a >> current_prefix >> strip_b;

      //Check if its a valid prefix
      if (trace_to_sim_prefixes.count(current_prefix) == 0) {
        skip = true;
      } else {
        skip = false;
      }
    } else if (skip) {
      continue;
    } else {
      std::istringstream lineStream(line);
      lineStream >> flow.bytes >> flow.duration >> flow.packets >> flow.rtt;
      for (auto it: trace_to_sim_prefixes[current_prefix]) {
        flow.prefix = it;
        flows.push_back(flow);
      }
    }
  }
  flowsDist.close();
  return flows;
}



}
