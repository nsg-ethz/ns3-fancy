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

#ifndef P4_LOSS_RADAR_H
#define P4_LOSS_RADAR_H

#include "p4-switch-net-device.h"
#include "ns3/net-device.h"
#include "ns3/mac48-address.h"
#include "ns3/nstime.h"
#include "ns3/p4-switch-channel.h"
#include "ns3/event-id.h"
#include "ns3/flow-error-model.h"
#include "ns3/hash-utils.h"
#include "p4-switch-utils.h"

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

namespace ns3 {

class Node;
struct LossRadarPacketInfo 
{
  uint32_t src_ip = 0;
  uint32_t dst_ip = 0;
  uint8_t protocol = 0;
  uint16_t src_port = 0;
  uint16_t dst_port =0;
  uint16_t id = 0;
  uint32_t pkt_counter = 0;
};

struct LossRadarPortInfo: PortInfo
{ 
  Time last_time_received;
  Time last_time_sent;
  bool linkState;

  /* TODO maybe rempve */
  uint32_t failures_count = 0;
  uint32_t reroute_count = 0;

  /* The batch id is stored in the ipv4 tos field in the 4 upper bits */
  /* thus we can use a max of 16 batch ids, however, we most probably will use 2*/
  uint8_t current_batch_id = 0; 

  /* Vector of vectors containing our xors */
  /* We split per batch first, and then we hash in the buckets */
  std::vector<std::vector<LossRadarPacketInfo>> um_info;
  std::vector<std::vector<LossRadarPacketInfo>> dm_info;

  std::string link_name;

};

class P4SwitchLossRadar : public P4SwitchNetDevice
{
public:
  static TypeId GetTypeId (void);
  P4SwitchLossRadar ();
  virtual ~P4SwitchLossRadar ();
  virtual void AddSwitchPort (Ptr<NetDevice> switchPort);
  virtual void Init ();
  virtual void SetDebug (bool state);

  LossRadarPortInfo & GetPortInfo(uint32_t i);

protected:

  virtual void DoDispose (void);

  // Parser
  void ParseArp (Ptr<Packet> packet, pkt_info &meta);
  void ParseIpv4 (Ptr<Packet> packet, pkt_info &meta);
  void ParseIpv6 (Ptr<Packet> packet, pkt_info &meta);
  void ParseTransport(Ptr<Packet> packet, pkt_info &meta, uint16_t protocol);  
  // Main Pipeline
  virtual void DoVerifyChecksums(Ptr<const Packet> packet, pkt_info &meta);
  virtual void DoParser(Ptr<Packet> packet, pkt_info &meta, uint16_t protocol);
  virtual void DoIngress(Ptr<const Packet> packet, pkt_info &meta);
  virtual void DoTrafficManager(Ptr<const Packet> packet, pkt_info &meta);
  virtual void DoEgress(Ptr<const Packet> packet, pkt_info &meta);
  virtual void DoUpdateChecksums(Ptr<const Packet> packet, pkt_info &meta);
  virtual void DoDeparser(Ptr<Packet> packet, pkt_info &meta);
  virtual void DoEgressTrafficManager(Ptr<Packet> packet, pkt_info &meta);

  // Time events
  void UpdateBatchId(Ptr<NetDevice> port, const Time &delay);
  void Controller(Ptr<NetDevice> port, uint8_t batch_id);

  // Helpers
  void GetMeterHashIndexes(ip_five_tuple &flow, uint32_t hash_indexes[]);


  // Loss radar main methods 
  void DmMeter( LossRadarPortInfo &portInfo, ip_five_tuple &flow, uint32_t hash_indexes[], uint8_t batch_id);
  void UmMeter( LossRadarPortInfo &portInfo, ip_five_tuple &flow, uint32_t hash_indexes[], uint8_t batch_id);
  
private:

  void InitPortInfo (LossRadarPortInfo & portInfo);

  /*
  Switch Pipeline State 
  */
  std::vector<std::string> header_stack = {"LOSS_RADAR", "ARP","IPV4", "UDP", "TCP"};  
  std::vector<std::string> header_stack_reverse;

  //HashUtils * m_hash;
  std::unique_ptr<HashUtils> m_hash;

  /* Simulation run state */
  std::unique_ptr<LossRadarSimulationState> m_simState;

  std::unordered_map<uint32_t, LossRadarPortInfo> m_portsInfo; 
  Ptr<RateErrorModel> tm_em = CreateObject<RateErrorModel> ();
  Ptr<RateErrorModel> fail_em = CreateObject<RateErrorModel> ();

  // Loss Radar parameters 
  uint16_t m_numBatches = 2;
  uint32_t m_numCells = 2048;
  uint16_t m_numHashes = 3;
  double m_batchTimeMs = 100; //100ms

  double m_tm_drop_rate = 0;
  double m_fail_drop_rate = 0;

  std::string m_outFile = "";

  uint32_t m_pkt_counter1 = 0;
  uint32_t m_pkt_counter2 = 0;
};

} // namespace ns3

#endif /* P4_LOSS_RADAR_H*/
