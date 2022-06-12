/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "p4-switch-utils.h"
#include <cstring>
#include <nlohmann/json.hpp>

NS_LOG_COMPONENT_DEFINE("p4-switch-utils");

using json = nlohmann::json;

namespace ns3 {

  bool sortbysec(const std::pair<uint32_t, double>& a,
    const std::pair<uint32_t, double>& b)
  {
    return (a.second > b.second);
  }

  int KArryTreeDepth(uint32_t split, uint32_t node_index)
  {
    if (split == 1)
    {
      return node_index;
    }

    double tmp = (node_index * (split - 1)) + 1;
    return int(std::log(tmp) / std::log(split));
  }

  /* filling buffers for the different hash functions */
  void
    IpFiveTupleToBuffer(char* data, ip_five_tuple& five_tuple)
  {
    std::memcpy(data, &five_tuple.src_ip, 4);
    std::memcpy(data + 4, &five_tuple.dst_ip, 4);
    std::memcpy(data + 8, &five_tuple.src_port, 2);
    std::memcpy(data + 10, &five_tuple.dst_port, 2);
    *(data + 12) = five_tuple.protocol;
  }

  void
    IpFiveTupleWithIdToBuffer(char* data, ip_five_tuple& five_tuple)
  {
    std::memcpy(data, &five_tuple.src_ip, 4);
    std::memcpy(data + 4, &five_tuple.dst_ip, 4);
    std::memcpy(data + 8, &five_tuple.id, 2);
    std::memcpy(data + 10, &five_tuple.src_port, 2);
    std::memcpy(data + 12, &five_tuple.dst_port, 2);
    *(data + 14) = five_tuple.protocol;
  }

  void
    DstPrefixToBuffer(char* data, ip_five_tuple& five_tuple)
  {
    uint32_t prefix = five_tuple.dst_ip & 0xffffff00;
    std::memcpy(data, &prefix, 4);
  }

  /* Flow string formatting */

  std::string
    IpFiveTupleToString(ip_five_tuple& flow)
  {
    std::stringstream tuple;
    tuple << flow.src_ip << " ";
    tuple << flow.dst_ip << " ";
    tuple << flow.src_port << " ";
    tuple << flow.dst_port << " ";
    tuple << int(flow.protocol);
    return tuple.str();
  }

  std::string
    DstPrefixToString(ip_five_tuple& flow)
  {
    return std::to_string(flow.dst_ip & 0xffffff00);
    //std::stringstream dst_prefix;
    //dst_prefix_str << dst_prefix;
    //return dst_prefix_str.str();
  }

  std::string
    DstPrefixToBeautifulString(ip_five_tuple& flow)
  {
    std::stringstream dst_prefix_str;
    dst_prefix_str << Ipv4Address(flow.dst_ip & 0xffffff00);
    return dst_prefix_str.str();
  }

  std::string
    IpFiveTupleToBeautifulString(ip_five_tuple& flow)
  {
    std::stringstream tuple;
    tuple << Ipv4Address(flow.src_ip) << " ";
    tuple << Ipv4Address(flow.dst_ip) << " ";
    tuple << flow.src_port << " ";
    tuple << flow.dst_port << " ";
    tuple << int(flow.protocol);
    return tuple.str();
  }

  void PrintDstPrefix(std::string message, ip_five_tuple& five_tuple)
  {
    std::cout << message;
    std::cout << DstPrefixToBeautifulString(five_tuple) << std::endl;
  }

  void PrintIpFiveTuple(std::string message, ip_five_tuple& five_tuple)
  {
    std::cout << message;
    PrintIpFiveTuple(five_tuple);
  }

  void PrintIpFiveTuple(ip_five_tuple& five_tuple)
  {
    std::cout << Ipv4Address(five_tuple.src_ip) << " " << Ipv4Address(five_tuple.dst_ip) << " " << five_tuple.src_port << " "
      << five_tuple.dst_port << " " << int(five_tuple.protocol) << " " << int(five_tuple.id) << std::endl;
  }


  /* Switch solutions OUTPUTs */

  /* Fancy FancySimulationState*/

  FancySimulationState::FancySimulationState(uint32_t depth, uint32_t bloom_hashes)
  {
    m_treeDepth = depth;
    m_rerouteBloomFilterNumHashes = bloom_hashes;
  }

  FancySimulationState::~FancySimulationState()
  {}

  void FancySimulationState::SetDetail(bool detail)
  {
    m_save_details = detail;
  }

  void
    FancySimulationState::SetSimulationStep(double timestamp, uint32_t step, uint32_t packets_sent, uint32_t packets_lost)
  {
    SimulationStep sim_step;
    sim_step.timestamp = timestamp;
    sim_step.step = step;
    sim_step.packets_sent = packets_sent;
    sim_step.packets_lost = packets_lost;
    simulation_steps.push_back(sim_step);
  }

  void
    FancySimulationState::SetSimulationTreeNode(uint32_t index)
  {
    if (m_save_details)
    {
      TreeNodeState node;
      node.index = index;
      simulation_steps.back().nodes.push_back(node);
    }
  }

  void
    FancySimulationState::SetCounterValues(uint32_t local_counter, uint32_t remote_counter, uint32_t bloom_count, uint32_t flow_count,
      boost::dynamic_bitset<>& bloom_filter)
  {
    if (m_save_details)
    {
      simulation_steps.back().nodes.back().local_counter.push_back(local_counter);
      simulation_steps.back().nodes.back().remote_counter.push_back(remote_counter);
      simulation_steps.back().nodes.back().bloom_count.push_back(bloom_count);
      simulation_steps.back().nodes.back().flow_count.push_back(flow_count);

      std::string buffer;
      to_string(bloom_filter, buffer);
      simulation_steps.back().nodes.back().bloom_filter.push_back(buffer);
    }
  }

  void
    FancySimulationState::SetSoftFailureEvent(double timestamp, uint8_t soft_type, char hash_path[],
      std::unordered_map<std::string, ip_five_tuple>& flows, uint32_t bloom_count,
      uint32_t local_counter, uint32_t remote_counter, uint32_t id, uint32_t depth)
  {

    SoftFailureEventState soft_failure_event;
    soft_failure_event.timestamp = timestamp;
    soft_failure_event.soft_type = soft_type;
    soft_failure_event.id = id;
    soft_failure_event.local_counter = local_counter;
    soft_failure_event.remote_counter = remote_counter;
    for (uint32_t i = 0; i < depth; i++)
    {
      soft_failure_event.hash_path.push_back(hash_path[i]);
    }

    soft_failure_event.flows = flows;
    soft_failure_event.bloom_count = bloom_count;
    soft_failure_event.flow_count = flows.size();
    soft_failure_event.depth = depth;
    soft_failure_events.push_back(soft_failure_event);
  }

  void
    FancySimulationState::SetSoftFailureEvent(double timestamp, uint8_t soft_type, uint32_t id, uint32_t local_counter, uint32_t remote_counter)
  {
    SoftFailureEventState soft_failure_event;
    soft_failure_event.timestamp = timestamp;
    soft_failure_event.soft_type = soft_type;
    soft_failure_event.id = id;
    soft_failure_event.local_counter = local_counter;
    soft_failure_event.remote_counter = remote_counter;
    soft_failure_event.bloom_count = 0;
    soft_failure_event.flow_count = 0;
    soft_failure_event.depth = 0;
    soft_failure_events.push_back(soft_failure_event);
  }



  void
    FancySimulationState::SetFailureEvent(double timestamp, char hash_path[], uint32_t bloom_filter_indexes[],
      std::unordered_map<std::string, ip_five_tuple>& flows, uint32_t bloom_count,
      uint32_t local_counter, uint32_t remote_counter, uint32_t id, uint32_t failure_number)
  {

    FailureEventState failure_event;
    failure_event.timestamp = timestamp;
    failure_event.id = id;
    failure_event.local_counter = local_counter;
    failure_event.remote_counter = remote_counter;
    for (uint32_t i = 0; i < m_treeDepth; i++)
    {
      failure_event.hash_path.push_back(hash_path[i]);
    }
    for (uint32_t i = 0; i < m_rerouteBloomFilterNumHashes; i++)
    {
      failure_event.bloom_filter_indexes.push_back(bloom_filter_indexes[i]);
    }
    failure_event.flows = flows;
    failure_event.bloom_count = bloom_count;
    failure_event.flow_count = flows.size();
    failure_event.failure_number = failure_number;
    failure_events.push_back(failure_event);

  }

  void
    FancySimulationState::SetFailureEvent(double timestamp, uint32_t id, uint32_t local_counter, uint32_t remote_counter, uint32_t failure_number)
  {

    FailureEventState failure_event;
    failure_event.timestamp = timestamp;
    failure_event.id = id;
    failure_event.local_counter = local_counter;
    failure_event.remote_counter = remote_counter;
    failure_event.bloom_count = 0;
    failure_event.flow_count = 0;
    failure_event.failure_number = failure_number;
    failure_events.push_back(failure_event);
  }

  void
    FancySimulationState::SetCollisionEvent(uint32_t step, uint32_t node_index, uint8_t  counter_cell, uint32_t num_collisions)
  {
    CollisionEventState collision_event;
    collision_event.step = step;
    collision_event.node_index = node_index;
    collision_event.counter_cell = counter_cell;
    collision_event.num_collisions = num_collisions;

    collision_events.push_back(collision_event);
  }

  void
    FancySimulationState::SetUniformFailureEvent(double timestamp, uint32_t step, uint16_t faulty_entries)
  {
    UniformFailureEventState uniform_failure_event;

    uniform_failure_event.timestamp = timestamp;
    uniform_failure_event.step = step;
    uniform_failure_event.faulty_entries = faulty_entries;

    uniform_failure_events.push_back(uniform_failure_event);
  }


  void
    FancySimulationState::SetMaxHistory(std::vector<std::vector<uint8_t>>& max_history)
  {
    if (m_save_details)
    {
      simulation_steps.back().nodes.back().max_history = max_history;
    }
  }

  void
    FancySimulationState::SetRerouteEvent(double timestamp, uint32_t bloom_filter_indexes[], uint32_t reroute_number,
      ip_five_tuple flow, uint32_t id)

  {
    RerouteEventState reroute_event;
    reroute_event.id = id;
    reroute_event.timestamp = timestamp;
    reroute_event.flow = flow;
    reroute_event.reroute_number = reroute_number;

    /* Set bloom filter indexes */
    for (uint32_t i = 0; i < m_rerouteBloomFilterNumHashes; i++)
    {
      reroute_event.bloom_filter_indexes.push_back(bloom_filter_indexes[i]);
    }
    reroute_events.push_back(reroute_event);
  }

  void
    FancySimulationState::SetRerouteEvent(double timestamp, uint32_t reroute_number, ip_five_tuple flow, uint32_t id)
  {
    RerouteEventState reroute_event;
    reroute_event.id = id;
    reroute_event.timestamp = timestamp;
    reroute_event.flow = flow;
    reroute_event.reroute_number = reroute_number;
    reroute_events.push_back(reroute_event);
  }

  void
    FancySimulationState::SaveInJson(std::string file_name)
  {

    json state;

    json reroutes;
    json failures;
    json soft_failures;
    json steps;
    json collisions;
    json uniform_failures;


    /* Add uniform failure events */
    for (uint32_t i = 0; i < uniform_failure_events.size(); i++)
    {
      json uniform_failure;

      uniform_failure["timestamp"] = uniform_failure_events[i].timestamp;
      uniform_failure["step"] = uniform_failure_events[i].step;
      uniform_failure["faulty_entries"] = uniform_failure_events[i].faulty_entries;

      uniform_failures.push_back(uniform_failure);
    }


    /* Add Collision Events */
    for (uint32_t i = 0; i < collision_events.size(); i++)
    {
      json collision;

      collision["step"] = collision_events[i].step;
      collision["node_index"] = collision_events[i].node_index;
      collision["counter_cell"] = collision_events[i].counter_cell;
      collision["num_collisions"] = collision_events[i].num_collisions;
      collisions.push_back(collision);
    }

    /* Add Reroutes Events */
    for (uint32_t i = 0; i < reroute_events.size(); i++)
    {
      json reroute;
      json indexes(reroute_events[i].bloom_filter_indexes);

      reroute["timestamp"] = reroute_events[i].timestamp;
      reroute["reroute_number"] = reroute_events[i].reroute_number;
      reroute["flow"] = IpFiveTupleToBeautifulString(reroute_events[i].flow);
      reroute["bloom_filter_indexes"] = indexes;
      reroute["id"] = reroute_events[i].id;
      reroutes.push_back(reroute);
    }

    /* Add Failure Events */

    for (uint32_t i = 0; i < failure_events.size(); i++)
    {

      json failure;
      json indexes(failure_events[i].bloom_filter_indexes);
      json hash_path(failure_events[i].hash_path);

      /* Convert Flows Nicely */
      std::vector<std::string> str_flows;
      for (auto it = failure_events[i].flows.begin(); it != failure_events[i].flows.end(); it++)
      {
        ip_five_tuple flow = it->second;
        std::string beautiful_flow = IpFiveTupleToBeautifulString(flow);
        str_flows.push_back(beautiful_flow);
      }

      json flows(str_flows);

      failure["timestamp"] = failure_events[i].timestamp;
      failure["hash_path"] = hash_path;
      failure["bloom_filter_indexes"] = indexes;
      failure["flows"] = flows;
      failure["bloom_count"] = failure_events[i].bloom_count;
      failure["flow_count"] = failure_events[i].flow_count;
      failure["id"] = failure_events[i].id;
      failure["local_counter"] = failure_events[i].local_counter;
      failure["remote_counter"] = failure_events[i].remote_counter;
      failure["failure_number"] = failure_events[i].failure_number;

      failures.push_back(failure);

    }

    /* Add Soft Failure Events */

    for (uint32_t i = 0; i < soft_failure_events.size(); i++)
    {
      json soft_failure;
      json hash_path(soft_failure_events[i].hash_path);

      /* Convert Flows Nicely */
      std::vector<std::string> str_flows;
      for (auto it = soft_failure_events[i].flows.begin(); it != soft_failure_events[i].flows.end(); it++)
      {
        ip_five_tuple flow = it->second;
        std::string beautiful_flow = IpFiveTupleToBeautifulString(flow);
        str_flows.push_back(beautiful_flow);
      }

      json flows(str_flows);

      soft_failure["timestamp"] = soft_failure_events[i].timestamp;
      soft_failure["type"] = soft_failure_events[i].soft_type;
      soft_failure["hash_path"] = hash_path;
      soft_failure["flows"] = flows;
      soft_failure["bloom_count"] = soft_failure_events[i].bloom_count;
      soft_failure["flow_count"] = soft_failure_events[i].flow_count;
      soft_failure["id"] = soft_failure_events[i].id;
      soft_failure["local_counter"] = soft_failure_events[i].local_counter;
      soft_failure["remote_counter"] = soft_failure_events[i].remote_counter;
      soft_failure["depth"] = soft_failure_events[i].depth;

      soft_failures.push_back(soft_failure);

    }

    /* Simulation Steps */
    for (uint32_t i = 0; i < simulation_steps.size(); i++)
    {
      json step;

      step["timestamp"] = simulation_steps[i].timestamp;
      step["step"] = simulation_steps[i].step;
      step["packets_sent"] = simulation_steps[i].packets_sent;
      step["packets_lost"] = simulation_steps[i].packets_lost;

      json nodes;

      for (uint32_t j = 0; j < simulation_steps[i].nodes.size(); j++)
      {
        json node;
        json local_counter(simulation_steps[i].nodes[j].local_counter);
        json remote_counter(simulation_steps[i].nodes[j].remote_counter);
        json bloom_count(simulation_steps[i].nodes[j].bloom_count);
        json flow_count(simulation_steps[i].nodes[j].flow_count);
        json bloom_filter(simulation_steps[i].nodes[j].bloom_filter);
        json max_history(simulation_steps[i].nodes[j].max_history);

        node["index"] = simulation_steps[i].nodes[j].index;
        node["local_counter"] = local_counter;
        node["remote_counter"] = remote_counter;
        node["bloom_count"] = bloom_count;
        node["flow_count"] = flow_count;
        node["bloom_filter"] = bloom_filter;
        node["max_history"] = max_history;

        nodes.push_back(node);

      }
      step["nodes"] = nodes;
      steps.push_back(step);
    }

    state["uniform_failures"] = uniform_failures;
    state["collisions"] = collisions;
    state["reroutes"] = reroutes;
    state["failures"] = failures;
    state["soft_failures"] = soft_failures;
    state["steps"] = steps;

    /* Save json to ouput file */
    std::fstream out_file;
    out_file.open(file_name, std::ios::out);
    out_file << state;
    out_file.close();

  }


  /* New systems output logger */

  // Net Seet */

  NetSeerSimulationState::NetSeerSimulationState(void)
  {

  }

  NetSeerSimulationState::~NetSeerSimulationState()
  {

  }

  void
    NetSeerSimulationState::SetFailureEvent(double timestamp, ip_five_tuple flow, uint32_t num_drops)
  {
    NetSeerFailureEventState failure_event;
    failure_event.timestamp = timestamp;
    failure_event.flow = flow;
    failure_event.num_drops = num_drops;
    failure_event.event_number = m_event_number;
    m_event_number++;
    failure_events.push_back(failure_event);
  }

  void
    NetSeerSimulationState::SaveInJson(std::string file_name)
  {
    json state;

    json failures;

    /* Add failure events */
    for (uint32_t i = 0; i < failure_events.size(); i++)
    {
      json failure;
      failure["timestamp"] = failure_events[i].timestamp;
      failure["flow"] = IpFiveTupleToBeautifulString(failure_events[i].flow);
      failure["event_number"] = failure_events[i].event_number;
      failure["num_drops"] = failure_events[i].num_drops;
      failures.push_back(failure);
    }

    state["failures"] = failures;

    std::fstream out_file;
    out_file.open(file_name, std::ios::out);
    out_file << state;
    out_file.close();
  }

  // Loss radar

  LossRadarSimulationState::LossRadarSimulationState(void)
  {

  }

  LossRadarSimulationState::~LossRadarSimulationState()
  {

  }

  void
    LossRadarSimulationState::SetFailureEvent(std::string link_name, double timestamp, std::vector<ip_five_tuple>& packets_lost,
      uint32_t non_pure_cells, uint32_t non_detected_packets, uint32_t total_packets_lost_in_batch)
  {
    LossRadarFailureEventState failure_event;
    failure_event.timestamp = timestamp;
    failure_event.packets_lost = packets_lost;
    failure_event.step = m_step_counter;
    failure_event.non_detected_packets = non_detected_packets;
    failure_event.non_pure_cells = non_pure_cells;
    failure_event.total_packets_lost_in_batch = total_packets_lost_in_batch;
    m_step_counter++;
    failure_events.push_back(failure_event);
  }

  void
    LossRadarSimulationState::SaveInJson(std::string file_name)
  {
    json state;
    json failures;

    /* Add failure events */
    for (uint32_t i = 0; i < failure_events.size(); i++)
    {
      json failure;
      failure["timestamp"] = failure_events[i].timestamp;
      failure["link_name"] = failure_events[i].link_name;
      failure["non_detected_packets"] = failure_events[i].non_detected_packets;
      failure["non_pure_cells"] = failure_events[i].non_pure_cells;
      failure["total_packets_lost_in_batch"] = failure_events[i].total_packets_lost_in_batch;
      failure["step"] = failure_events[i].step;

      std::vector<std::string> packets_lost_str;
      for (uint32_t j = 0; j < failure_events[i].packets_lost.size(); j++)
      {
        packets_lost_str.push_back(IpFiveTupleToBeautifulString(failure_events[i].packets_lost[j]));
      }

      json packets_lost(packets_lost_str);
      failure["packets_lost"] = packets_lost;
      failures.push_back(failure);
    }

    state["failures"] = failures;

    std::fstream out_file;
    out_file.open(file_name, std::ios::out);
    out_file << state;
    out_file.close();
  }


  /* TOP prefix stuff */
  std::vector<std::string> LoadTopPrefixes(std::string file)
  {
    std::vector<std::string> top_prefixes;
    std::string prefix, line;
    int bytes, packets;

    std::ifstream top_entries_file(file);
    NS_ASSERT_MSG(top_entries_file, "Provide a valid top entries file path " + file);

    while (std::getline(top_entries_file, line))
    {
      if (line.empty() || line.rfind("#", 0) == 0)
      {
        continue;
      }
      std::istringstream lineStream(line);
      lineStream >> prefix >> bytes >> packets;

      top_prefixes.push_back(prefix);
    }
    return top_prefixes;
  }


  std::vector<uint32_t> LoadTopPrefixesInt(std::string file)
  {
    std::vector<uint32_t> top_prefixes;
    std::vector<std::string> _top_prefixes = LoadTopPrefixes(file);
    for (int i = 0; i < _top_prefixes.size(); i++)
    {
      uint32_t prefix = (Ipv4Address(_top_prefixes[i].c_str()).Get()) & 0xffffff00;
      top_prefixes.push_back(prefix);
    }

    return top_prefixes;
  }

  std::vector<std::string> LoadPrefixList(std::string file)
  {
    std::vector<std::string> prefixes_list;
    std::string prefix, line;

    std::ifstream prefixes_list_file(file);
    NS_ASSERT_MSG(prefixes_list_file, "Provide a valid prefixes list file path " + file);

    while (std::getline(prefixes_list_file, line))
    {
      if (line.empty() || line.rfind("#", 0) == 0)
      {
        continue;
      }
      std::istringstream lineStream(line);
      lineStream >> prefix;

      prefixes_list.push_back(prefix);
    }
    return prefixes_list;
  }


}