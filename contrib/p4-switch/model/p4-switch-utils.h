/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef P4_SWITCH_UTILS_H
#define P4_SWITCH_UTILS_H

#include <string.h>
#include <stdint.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <utility>

#include "ns3/log.h"
#include "ns3/ipv4-address.h"
#include "ns3/log.h"

#include <unordered_map>
#include <boost/dynamic_bitset.hpp>

namespace ns3 {

  struct ip_five_tuple
  {
    uint32_t src_ip = 0;
    uint32_t dst_ip = 0;
    uint8_t protocol = 0;
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    /* we add more fields used by others*/
    uint16_t id = 0;
    //uint8_t tos = 0; 
  };

  int KArryTreeDepth(uint32_t split, uint32_t node_index);

  std::string
    IpFiveTupleToString(ip_five_tuple& flow);

  std::string
    IpFiveTupleToBeautifulString(ip_five_tuple& flow);

  void
    IpFiveTupleToBuffer(char* data, ip_five_tuple& five_tuple);

  void
    IpFiveTupleWithIdToBuffer(char* data, ip_five_tuple& five_tuple);

  void
    DstPrefixToBuffer(char* data, ip_five_tuple& five_tuple);

  std::string
    DstPrefixToString(ip_five_tuple& five_tuple);

  std::string
    DstPrefixToBeautifulString(ip_five_tuple& five_tuple);

  void
    PrintDstPrefix(std::string message, ip_five_tuple& five_tuple);

  void
    PrintIpFiveTuple(ip_five_tuple& five_tuple);

  void
    PrintIpFiveTuple(std::string message, ip_five_tuple& five_tuple);

  /* Simulation State Object. Serialization/Deserialization */

  bool sortbysec(const std::pair<uint32_t, double>& a,
    const std::pair<uint32_t, double>& b);

  std::vector<std::string> LoadTopPrefixes(std::string file);
  std::vector<uint32_t> LoadTopPrefixesInt(std::string file);

  std::vector<std::string> LoadPrefixList(std::string file);

  struct TreeNodeState
  {
    uint32_t index;
    std::vector<uint32_t> local_counter;
    std::vector<uint32_t> remote_counter;
    std::vector<uint32_t> bloom_count;
    std::vector<uint32_t> flow_count;
    std::vector<std::string> bloom_filter;
    std::vector<std::vector<uint8_t>> max_history;
  };

  struct CollisionEventState
  {
    uint32_t step;
    uint32_t node_index;
    uint8_t  counter_cell;
    uint32_t num_collisions;
  };

  struct UniformFailureEventState
  {
    double timestamp;
    uint32_t step;
    uint16_t faulty_entries;
  };

  struct FailureEventState
  {
    double timestamp;
    uint32_t id;
    uint32_t local_counter;
    uint32_t remote_counter;
    std::vector<char> hash_path;
    std::vector<uint32_t> bloom_filter_indexes;
    std::unordered_map<std::string, ip_five_tuple> flows;
    uint32_t bloom_count;
    uint32_t flow_count;
    uint32_t failure_number;
  };

  struct SoftFailureEventState
  {
    double timestamp;
    uint8_t soft_type;
    uint32_t id;
    uint32_t local_counter;
    uint32_t remote_counter;
    std::vector<char> hash_path;
    std::unordered_map<std::string, ip_five_tuple> flows;
    uint32_t bloom_count;
    uint32_t flow_count;
    uint32_t depth;
  };

  struct RerouteEventState
  {
    double timestamp;
    uint32_t id;
    std::vector<uint32_t> bloom_filter_indexes;
    uint32_t reroute_number;
    ip_five_tuple flow;
  };

  struct SimulationStep
  {
    double timestamp;
    uint32_t step;
    /* Global */
    uint32_t packets_sent;
    uint32_t packets_lost;
    std::vector<TreeNodeState> nodes;
  };

  class FancySimulationState
  {
  public:
    FancySimulationState(uint32_t depth, uint32_t bloom_hashes);
    ~FancySimulationState();

    /* tree nodes */
    void SetSimulationStep(double timestamp, uint32_t step, uint32_t packets_sent, uint32_t packets_lost);
    void SetSimulationTreeNode(uint32_t index);
    void SetCounterValues(uint32_t local_counter, uint32_t remote_counter, uint32_t bloom_count, uint32_t flow_count,
      boost::dynamic_bitset<>& bloom_filter);

    void SetFailureEvent(double timestamp, char hash_path[], uint32_t bloom_filter_indexes[],
      std::unordered_map<std::string, ip_five_tuple>& flows, uint32_t bloom_count,
      uint32_t local_counter, uint32_t remote_counter, uint32_t id, uint32_t failure_number);

    void SetSoftFailureEvent(double timestamp, uint8_t soft_type, char hash_path[],
      std::unordered_map<std::string, ip_five_tuple>& flows, uint32_t bloom_count,
      uint32_t local_counter, uint32_t remote_counter, uint32_t id, uint32_t depth);

    void SetSoftFailureEvent(double timestamp, uint8_t soft_type, uint32_t id, uint32_t local_counter, uint32_t remote_counter);

    void SetFailureEvent(double timestamp, uint32_t id, uint32_t local_counter, uint32_t remote_counter, uint32_t failure_number);

    void SetCollisionEvent(uint32_t step, uint32_t node_index, uint8_t  counter_cell, uint32_t num_collisions);

    void SetMaxHistory(std::vector<std::vector<uint8_t>>& max_history);
    void SetRerouteEvent(double timestamp, uint32_t bloom_filter_indexes[], uint32_t reroute_number, ip_five_tuple flow, uint32_t id);
    void SetRerouteEvent(double timestamp, uint32_t reroute_number, ip_five_tuple flow, uint32_t id);
    void SetUniformFailureEvent(double timestamp, uint32_t step, uint16_t faulty_entries);

    void SaveInJson(std::string file_name);

    void SetDetail(bool enabled);

  protected:

  private:
    std::vector<SimulationStep> simulation_steps;
    std::vector<FailureEventState> failure_events;
    std::vector<SoftFailureEventState> soft_failure_events;
    std::vector<RerouteEventState> reroute_events;
    std::vector<CollisionEventState> collision_events;
    std::vector<UniformFailureEventState> uniform_failure_events;

    uint32_t m_treeDepth = 0;
    uint32_t m_rerouteBloomFilterNumHashes = 0;

    bool m_save_details = false;
  };


  /* Net Seer state object */
  struct NetSeerFailureEventState
  {
    double timestamp;
    ip_five_tuple flow;
    uint32_t num_drops;
    uint32_t event_number;
  };

  class NetSeerSimulationState
  {
  public:
    NetSeerSimulationState();
    ~NetSeerSimulationState();

    void SetFailureEvent(double timestamp, ip_five_tuple flow, uint32_t num_drops);
    void SaveInJson(std::string file_name);

  protected:

  private:
    std::vector<NetSeerFailureEventState> failure_events;
    uint32_t m_event_number = 0;
  };

  /* Loss radar */
  struct LossRadarFailureEventState
  {
    double timestamp;
    std::string link_name;
    std::vector<ip_five_tuple> packets_lost;
    uint32_t non_pure_cells;
    uint32_t non_detected_packets;
    uint32_t total_packets_lost_in_batch;
    uint32_t step;
  };

  class LossRadarSimulationState
  {
  public:
    LossRadarSimulationState();
    ~LossRadarSimulationState();

    void SetFailureEvent(std::string link_name, double timestamp, std::vector<ip_five_tuple>& packets, uint32_t non_pure_cells,
      uint32_t non_detected_packets, uint32_t total_packets_lost_in_batch);
    void SaveInJson(std::string file_name);

  protected:

  private:
    std::vector<LossRadarFailureEventState> failure_events;
    uint32_t m_step_counter = 0;
  };


}

#endif /* P4_SWITCH_UTILS_H */