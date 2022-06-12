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

#ifndef P4_NAT_H
#define P4_NAT_H

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

struct MapCell
{
  uint32_t ip;
  Time last_accessed;
};

struct TranslatePort
{
  Ptr<NetDevice> port = NULL;
  uint32_t other_side_ip = 0;
};

struct NatPortInfo: PortInfo
{

  bool linkState;
  uint32_t failures_count = 0;
  uint32_t detected_count = 0;
  std::string link_name;

  bool is_nat_interface = false;
  std::unordered_map<std::string, MapCell> nat_map; 
};

class P4SwitchNAT : public P4SwitchNetDevice
{
public:
  static TypeId GetTypeId (void);
  P4SwitchNAT ();
  virtual ~P4SwitchNAT ();
  virtual void AddSwitchPort (Ptr<NetDevice> switchPort);
  virtual void Init ();
  virtual void SetDebug (bool state);

protected:

  virtual void DoDispose (void);

  // Parser
  void ParseNAT (Ptr<Packet> packet, pkt_info &meta);
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

  void CleanMapper(Time interval);
  
private:

  void InitPortInfo ();

  /*
  Switch Pipeline State 
  */
  std::vector<std::string> header_stack = {"ARP","IPV4", "UDP", "TCP"};  
  std::vector<std::string> header_stack_reverse;

  //HashUtils * m_hash;
  std::unique_ptr<HashUtils> m_hash;

  /* Simulation run state */
  //std::unique_ptr<SimulationState> m_simState;

  std::unordered_map<uint32_t, NatPortInfo> m_portsInfo; 
  
  Ptr<RateErrorModel> tm_em = CreateObject<RateErrorModel> ();
  Ptr<RateErrorModel> fail_em = CreateObject<RateErrorModel> ();

  double m_tm_drop_rate = 0;
  double m_fail_drop_rate = 0;

  std::vector<TranslatePort> m_natPorts;
  uint32_t m_numNatPorts = 1;
  Ptr<NetDevice> m_natInterface;

};

} // namespace ns3

#endif /* P4_NAT*/
