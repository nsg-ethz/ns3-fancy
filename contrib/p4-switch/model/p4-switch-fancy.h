/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Edgar Costa Molero <cedgar@ethz.ch>
 */

#ifndef P4_FANCY_H
#define P4_FANCY_H

#include "p4-switch-net-device.h"
#include "ns3/net-device.h"
#include "ns3/mac48-address.h"
#include "ns3/nstime.h"
#include "ns3/p4-switch-channel.h"
#include "ns3/event-id.h"
#include "ns3/flow-error-model.h"
#include "ns3/hash-utils.h"
#include "p4-switch-utils.h"
#include "fancy-header.h"

#include <stdint.h>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <any>
#include <algorithm>
#include <math.h>
#include <unordered_set>
#include <bitset>
#include <iostream>
#include <ctime>
#include <iomanip>
#include <memory>
#include <boost/dynamic_bitset.hpp>

namespace ns3 {

  //int TREE_DEPTH = 5;
  //int LAYER_SPLIT = 2;
  //int COUNTER_WIDTH = 16;
  //int COUNTER_BLOOM_FILTER_WIDTH = 16;
  //int NODES_IN_TREE = 31;//pow(4,2)-1; formula: (k^h -1) / (k-1), 7, 15, 31 k=split, h=depth
  //int REROUTE_BLOOM_FILTER_WIDTH = 10000;
  //int REROUTE_BLOOM_FILTER_NUM_HASHES=3;

  class Node;

  enum class GreyState
  {
    IDLE,
    START_ACK,
    COUNTING,
    WAIT_COUNTER_SEND,
    WAIT_COUNTER_RECEIVE,
    COUNTER_ACK,
  };

  enum class CostTypes
  {
    ABSOLUTE = 0, //0 
    RELATIVE = 1  //1 
  };

  struct HashCounter
  {
    /* Packet loss counter*/
    //uint32_t counters[COUNTER_WIDTH];
    std::vector<uint32_t> counters;

    /* last flow we have hashed */
    //ip_five_tuple last_flow[COUNTER_WIDTH];
    std::vector<ip_five_tuple> last_flow;

    /* Hashed flows */
    /* All flows hashed in a cell for meassuring debugging propuses*/
    std::vector<std::unordered_map<std::string, ip_five_tuple>> hashed_flows;

    /* bloom filters per cell to count flows*/
    //std::bitset<COUNTER_BLOOM_FILTER_WIDTH> bloom_filter[COUNTER_WIDTH];
    std::vector<boost::dynamic_bitset<>> bloom_filter;

    /* where we save previous maxes */
    //uint8_t max_cells[TREE_DEPTH][LAYER_SPLIT];
    std::vector<std::vector<uint8_t>> max_cells;
  };

  struct RerouteBloomFilter
  {
    bool set = false;
    //bool notified;
    /* this can be anything, a prefix or an entire flow */
    std::unordered_set<std::string> already_rerouted;
    uint8_t outPort = 0;
  };

  struct RerouteTopEntries
  {
    bool set = false;
    /* We just need a bool to indicate it here */
    bool already_rerouted = false;
    uint8_t outPort = 0;
  };

  struct GreyInfo
  {
    // Grey failure detection
    //uint64_t localCounter = 0;
    std::vector<uint64_t> localCounter;
    //GreyState greyState = GreyState::IDLE;
    std::vector<GreyState> greyState;
    //uint64_t currentSEQ = 0;
    std::vector<uint32_t> currentSEQ;
    //EventId  currentEvent;
    std::vector<EventId> currentEvent;

    /* Debug, saves last packet */
    std::vector<uint32_t> last_packet_seq;

    /* Tree data structure */
    std::vector<HashCounter> counter_tree;
    //HashCounter counter_tree[NODES_IN_TREE];
  };

  struct FancyPortInfo : PortInfo
  {
    Time last_time_received;
    Time last_time_sent;
    bool linkState;
    GreyInfo greyRecv;
    GreyInfo greySend;

    /* for the zooming structure */
    std::vector<RerouteBloomFilter> reroute;
    /* for the k top entries */
    std::vector<RerouteTopEntries> reroute_top;
    uint32_t failures_count = 0;
    uint32_t reroute_count = 0;

    std::string link_name;

  };

  class P4SwitchFancy : public P4SwitchNetDevice
  {
  public:

    static TypeId GetTypeId(void);
    P4SwitchFancy();
    virtual ~P4SwitchFancy();
    virtual void AddSwitchPort(Ptr<NetDevice> switchPort);
    virtual void Init();
    virtual void SetDebug(bool state);

    void DisableTopEntries(void);
    void DisableAllFSM(void);


  protected:

    std::map<GreyState, std::string> GreyStateToString = { {GreyState::IDLE, "IDLE"} ,{GreyState::START_ACK, "START_ACK"},
                                                          {GreyState::COUNTING, "COUNTING"}, {GreyState::WAIT_COUNTER_SEND, "WAIT_COUNTER_SEND"},
                                                          {GreyState::WAIT_COUNTER_RECEIVE, "WAIT_COUNTER_RECEIVE"} ,{GreyState::COUNTER_ACK, "COUNTER_ACK"} };

    // Parser
    void ParseFancy(Ptr<Packet> packet, pkt_info& meta);
    void ParseArp(Ptr<Packet> packet, pkt_info& meta);
    void ParseIpv4(Ptr<Packet> packet, pkt_info& meta);
    void ParseIpv6(Ptr<Packet> packet, pkt_info& meta);
    void ParseTransport(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol);
    // Main Pipeline
    virtual void DoVerifyChecksums(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoParser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol);
    virtual void DoIngress(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoTrafficManager(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoEgress(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoUpdateChecksums(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoDeparser(Ptr<Packet> packet, pkt_info& meta);
    virtual void DoEgressTrafficManager(Ptr<Packet> packet, pkt_info& meta);

    /* Failure detection specific methods */
    void SendProbe(const Time& delay, Ptr<const Packet> packet, pkt_info meta);
    void CheckPortState(const Time& delay, Ptr<NetDevice> port);
    void SendGreyAction(Ptr<NetDevice> outPort, uint8_t action, uint32_t id);
    void SendGreyCounter(Ptr<NetDevice> outPort, uint32_t id);

    void IngressCounterBox(Ptr<const Packet> packet, pkt_info& meta);
    void EgressCounterBox(Ptr<const Packet> packet, pkt_info& meta);

    /* Counting Functions */
    void PipelinedPacketCounting(GreyInfo& greyPortInfo, uint32_t id, pkt_info& meta, bool sender);
    void PacketCounting(GreyInfo& greyPortInfo, uint32_t id, pkt_info& meta, bool sender);

    /* State Resets at the receiver */
    void PipelinedInitReceiverStep(FancyPortInfo& portInfo, uint32_t id, uint16_t length, Buffer::Iterator& start, bool shift_history);
    void InitReceiverStep(FancyPortInfo& portInfo, uint32_t id, uint16_t length, Buffer::Iterator& start, bool shift_history);

    /* Counter Exchange Algorithms */
    void PipelinedCounterExchangeAlgorithm(FancyPortInfo& inPortInfo, uint32_t id, FancyHeader& fancy_hdr);
    void CounterExchangeAlgorithm(FancyPortInfo& inPortInfo, uint32_t id, FancyHeader& fancy_hdr);


    void GetBloomFilterHashes(ip_five_tuple& ip_five_tuple, uint32_t hash_indexes[], int unit_modulo, int bloom_filter_modulo);
    uint32_t GetFlowHash(ip_five_tuple& five_tuple, int hash_index, int modulo);
    uint32_t GetDstPrefixHash(ip_five_tuple& five_tuple, int hash_index, int modulo);

    /* Top k entries */
    void LoadTopFlowEntriesTable(std::string top_entries_file, int k);
    void LoadTopDstPrefixEntriesTable(std::string top_entries_file, int k);

    uint32_t MatchTopEntriesTable(pkt_info& meta);
    void ShowTopEntriesTable(void);

  private:

    void SetGreyState(FancyPortInfo& portInfo, GreyState newState, uint32_t id, bool in);
    void InitPortInfo(FancyPortInfo& portInfo);
    bool IsBloomFilterSet(FancyPortInfo& portInfo, const uint32_t hash_indexes[]);
    void SetBloomFilter(FancyPortInfo& portInfo, char hash_path[], uint32_t bloom_filter_hashes[]);


    /*
    Switch Pipeline State
    */
    std::vector<std::string> header_stack = { "FANCY", "ARP","IPV4", "UDP", "TCP" };
    std::vector<std::string> header_stack_reverse;

    //HashUtils * m_hash;
    std::unique_ptr<HashUtils> m_hash;

    // packet hash function
    /* pointer to our packet hash function */
    /* HOW TO CALL? (this->*m_packet_hash)(ip_five_tuple, i, unit_modulo) */
    /* This is used so we can have different ways of hashing a packet and then getting indexes */
    uint32_t(P4SwitchFancy::* m_packet_hash)(ip_five_tuple& five_tuple, int hash_index, int modulo);
    std::string(*m_packet_str_funct)(ip_five_tuple& flow);

    std::string m_packet_hash_type = "FiveTupleHash"; //DstPrefixHash
    // (this->*m_packet_hash)(ip_five_tuple, i, unit_modulo) (how to call)

    /* Simulation run state */
    std::unique_ptr<FancySimulationState> m_simState;

    std::unordered_map<uint32_t, FancyPortInfo> m_portsInfo;
    Ptr<RateErrorModel> tm_em = CreateObject<RateErrorModel>();
    Ptr<RateErrorModel> fail_em = CreateObject<RateErrorModel>();

    /* Top K Entries: TODO this should be done at a per-egress level but not
    strictly needed now */
    std::unordered_map<std::string, uint32_t> m_topEntries;
    uint32_t m_numTopEntries = 0;

    /* Zooming Algorithm Parameters */
    bool     m_treeEnableSoftDetections = true;
    bool     m_treeEnabled = true; /* if disabled the tree is not even started */
    bool     m_rerouteEnabled = true;
    bool     m_pipeline = true;
    bool     m_pipelineBoost = true;
    uint32_t m_treeDepth = 5;
    uint32_t m_layerSplit = 2;
    uint32_t m_counterWidth = 16;
    uint32_t m_counterBloomFilterWidth = 16;
    uint32_t m_nodesInTree = 31;//pow(4,2)-1; formula: (k^h -1) / (k-1), 7, 15, 31 k=split, h=depth
    uint32_t m_rerouteBloomFilterWidth = 10000;
    uint32_t m_rerouteBloomFilterNumHashes = 3;
    uint16_t m_maxCounterCollisions = 4;
    uint16_t m_uniformLossThreshold = 0;
    uint16_t m_costType = 1;
    double   m_rerouteMinCost = 0;
    /* zooming parameters */

    /* constant attributes that drive the state */
    double m_tm_drop_rate = 0;
    double m_fail_drop_rate = 1;
    double m_check_port_state_ms = 10;
    bool m_check_port_state_enable = true;
    double m_send_port_state_ms = m_check_port_state_ms / 2;
    double m_start_system_sec = 2;
    double m_ack_wait_time_ms = 5;
    double m_send_counter_wait_ms = 5;
    double m_time_between_campaing_ms = 2;

    double m_probing_time_zooming_ms = 200;
    double m_probing_time_top_entries_ms = 200;
    /**/

    // If this is bigger than 0, and we detect those prefixes we stop the
    // simulation (so its faster)
    uint32_t m_early_stop_counter = 0;
    uint32_t m_early_stop_delay_ms = 200;

    std::string m_outFile = "";
    bool m_enableSaveDrops = false;
    std::string m_topFile = "";
  };

} // namespace ns3

#endif /* P4_FANCY_H*/
