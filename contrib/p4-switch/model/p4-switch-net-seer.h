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

#ifndef P4_NET_SEER_H
#define P4_NET_SEER_H

#include "p4-switch-net-device.h"
#include "ns3/net-device.h"
#include "ns3/mac48-address.h"
#include "ns3/nstime.h"
#include "ns3/p4-switch-channel.h"
#include "ns3/event-id.h"
#include "ns3/flow-error-model.h"
#include "ns3/hash-utils.h"
#include "p4-switch-utils.h"
#include "net-seer-header.h"

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
  struct NetSeerPacketInfo
  {
    uint32_t src_ip = 0;
    uint32_t dst_ip = 0;
    uint8_t protocol = 0;
    uint16_t src_port = 0;
    uint16_t dst_port = 0;
    // used as sequence number in the ring buffer
    // used as counter for the event cache memory
    uint32_t seq = 0;
    bool reported = false;
  };

  struct NetSeerPortInfo : PortInfo
  {
    Time last_time_received;
    Time last_time_sent;
    bool linkState;

    uint32_t failures_count = 0;
    uint32_t detected_count = 0;

    uint32_t reroute_count = 0;

    /* Expected as sender */
    uint32_t sender_next_expected_seq = 0;

    /* Expected as receiver */
    uint32_t receiver_next_expected_seq = 0;

    /* we use net seer packet info for this although seq should be called counter */
    std::vector<NetSeerPacketInfo> event_cache;

    /* ring buffer used to save packets */
    std::vector<NetSeerPacketInfo> ring_buffer;

    std::string link_name;
  };

  class P4SwitchNetSeer : public P4SwitchNetDevice
  {
  public:
    static TypeId GetTypeId(void);
    P4SwitchNetSeer();
    virtual ~P4SwitchNetSeer();
    virtual void AddSwitchPort(Ptr<NetDevice> switchPort);
    virtual void Init();
    virtual void SetDebug(bool state);

  protected:

    virtual void DoDispose(void);

    // Parser
    void ParseNetSeer(Ptr<Packet> packet, pkt_info& meta);
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

    /* Net seer stuff */
    ip_five_tuple PacketInfoToFiveTuple(NetSeerPacketInfo packet);
    void SaveFlow(NetSeerPortInfo& port, pkt_info& meta);
    void CheckPacketLosses(NetSeerPortInfo& port, uint32_t seq1, uint32_t seq2);
    void ReportPacket(NetSeerPortInfo& port, NetSeerPacketInfo packet);
    void SendNACK(Ptr<NetDevice> outPort, uint32_t seq1, uint32_t seq2, uint32_t times);
    bool ComparePacketInfos(NetSeerPacketInfo pkt1, NetSeerPacketInfo pkt2);

  private:

    void InitPortInfo(NetSeerPortInfo& portInfo);

    /*
    Switch Pipeline State
    */
    std::vector<std::string> header_stack = { "NETSEER", "ARP","IPV4", "UDP", "TCP" };
    std::vector<std::string> header_stack_reverse;

    //HashUtils * m_hash;
    std::unique_ptr<HashUtils> m_hash;

    /* Simulation run state */
    std::unique_ptr<NetSeerSimulationState> m_simState;

    std::unordered_map<uint32_t, NetSeerPortInfo> m_portsInfo;
    Ptr<RateErrorModel> tm_em = CreateObject<RateErrorModel>();
    Ptr<RateErrorModel> fail_em = CreateObject<RateErrorModel>();

    // Net Seer params
    uint32_t m_bufferSize = 1024;
    uint32_t m_eventCacheSize = 256;
    uint32_t m_eventCounter = 128;

    double m_tm_drop_rate = 0;
    double m_fail_drop_rate = 0;

    std::string m_outFile = "";

    // If this is bigger than 0, and we detect those prefixes we stop the
    // simulation (so its faster)
    uint32_t m_early_stop_counter = 0;
    std::unordered_set<uint32_t> m_unique_detected_prefixes;

  };

} // namespace ns3

#endif /* P4_NET_SEER_H*/
