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
#include "p4-switch-net-seer.h"
#include "ns3/node.h"
#include "ns3/channel.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/ethernet-header.h"
#include "ns3/ipv4-header.h"
#include "ns3/tcp-header.h"
#include "ns3/udp-header.h"
#include "ns3/arp-header.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/utils.h"

using namespace std;
namespace ns3 {

  NS_LOG_COMPONENT_DEFINE("P4SwitchNetSeer");

  NS_OBJECT_ENSURE_REGISTERED(P4SwitchNetSeer);

#define IPV4 0x0800
#define ARP  0x0806
#define IPV6 0x86DD
#define NETSEER 0xABCD
#define TCP  6
#define UDP  17

  /* Net Seer Actions */
#define SEQ 1 
#define NACK 2

  TypeId
    P4SwitchNetSeer::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::P4SwitchNetSeer")
      .SetParent<P4SwitchNetDevice>()
      .SetGroupName("P4-Switch")
      .AddConstructor<P4SwitchNetSeer>()
      .AddAttribute("NumberOfCells", "Number of cells per port",
        UintegerValue(1024),
        MakeUintegerAccessor(&P4SwitchNetSeer::m_bufferSize),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("EventCacheSize", "Number of cells in the event cache",
        UintegerValue(256),
        MakeUintegerAccessor(&P4SwitchNetSeer::m_eventCacheSize),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("EventCounter", "Threshold before reporting an event multiple times",
        UintegerValue(128),
        MakeUintegerAccessor(&P4SwitchNetSeer::m_eventCounter),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("TmDropRate", "Percentage of traffic that gets dropped by the traffic manager.",
        DoubleValue(0),
        MakeDoubleAccessor(&P4SwitchNetSeer::m_tm_drop_rate),
        MakeDoubleChecker<double>(0, 1))
      .AddAttribute("FailDropRate", "Percentage of traffic that gets dropped when a prefix is affected.",
        DoubleValue(0),
        MakeDoubleAccessor(&P4SwitchNetSeer::m_fail_drop_rate),
        MakeDoubleChecker<double>(0, 1))
      .AddAttribute("OutFile", "Name of the output file for the data generated.",
        StringValue(""),
        MakeStringAccessor(&P4SwitchNetSeer::m_outFile),
        MakeStringChecker())
      .AddAttribute("EarlyStopCounter", "Stops simulation before",
        UintegerValue(0),
        MakeUintegerAccessor(&P4SwitchNetSeer::m_early_stop_counter),
        MakeUintegerChecker<uint32_t>())
      ;
    return tid;
  }

  P4SwitchNetSeer::P4SwitchNetSeer()
    :
    header_stack_reverse(header_stack)
  {
    std::reverse(header_stack_reverse.begin(), header_stack_reverse.end());
  }

  P4SwitchNetSeer::~P4SwitchNetSeer()
  {
    NS_LOG_FUNCTION_NOARGS();
    if (m_hash)
    {
      m_hash->Clean();
    }

    if (m_outFile != "")
    {
      m_simState->SaveInJson(m_outFile);

      // Report events that did not trigger at the end


      // save packet drops
      if (m_drop_times.size() > 0)
      {
        std::string drops_file = m_outFile;
        drops_file.replace(drops_file.find(".json"), 5, ".drops");
        SaveDrops(drops_file);
      }

    }
  }

  void
    P4SwitchNetSeer::AddSwitchPort(Ptr<NetDevice> switchPort)
  {
    NS_LOG_FUNCTION_NOARGS();

    P4SwitchNetDevice::AddSwitchPort(switchPort);

    uint32_t ifIndex = switchPort->GetIfIndex();
    NetSeerPortInfo& portInfo = m_portsInfo[ifIndex];
    //InitPortInfo(portInfo);

    /* With this we copy the parents hiden attribute info */
    /* https://stackoverflow.com/questions/57997870/nested-struct-attributes-inheritance#57997920 */

    PortInfo& switchInfo = P4SwitchNetDevice::m_portsInfo[ifIndex];
    portInfo.portDevice = switchInfo.portDevice;
    portInfo.otherPortDevice = switchInfo.otherPortDevice;
    portInfo.switchPort = switchInfo.switchPort;

    portInfo.last_time_received = Simulator::Now();
    portInfo.last_time_sent = Simulator::Now();
    portInfo.linkState = true;

    Ptr<Node> thisSide = switchPort->GetNode();
    Ptr<Node> otherSide = portInfo.otherPortDevice->GetNode();
    std::string thisSideName = Names::FindName(thisSide);
    std::string otherSideName = Names::FindName(otherSide);

    /* Set link name */
    portInfo.link_name = thisSideName + "->" + otherSideName;
    NS_LOG_UNCOND("Interface mapping: " << portInfo.link_name << " : " << switchPort->GetIfIndex());
  }

  void
    P4SwitchNetSeer::SetDebug(bool state)
  {
    m_enableDebug = state;
  }

  void
    P4SwitchNetSeer::Init()
  {
    // Forwarding table is set here
    P4SwitchNetDevice::Init();

    /* allocates the simulation state object */
    m_simState = std::make_unique<NetSeerSimulationState>();

    /* Sets tm drop error model */
    tm_em->SetAttribute("ErrorRate", DoubleValue(m_tm_drop_rate));
    tm_em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    /* Set the link error rate (check the conditions since it checks tos field) */
    fail_em->SetAttribute("ErrorRate", DoubleValue(m_fail_drop_rate));
    fail_em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    /* set the number of hashes we need for the algorithm */
    m_hash = std::make_unique<HashUtils>(1);

    /* Allocate port structure memories */
    for (uint32_t i = 0; i < m_ports.size(); i++)
    {
      uint32_t ifIndex = m_ports[i]->GetIfIndex();
      Ptr<NetDevice> switchPort = m_ports[i];
      NetSeerPortInfo& portInfo = m_portsInfo[ifIndex];
      InitPortInfo(portInfo);

      /* Start State Machine */
      portInfo.last_time_received = Simulator::Now();
      portInfo.last_time_sent = Simulator::Now();
      portInfo.linkState = true;

      Ptr<Node> otherSide = portInfo.otherPortDevice->GetNode();
      std::string otherSideName = Names::FindName(otherSide);

      if (otherSideName.empty())
      {
        NS_FATAL_ERROR("The node in the other side was not named.");
      }

      // The state machine is lunched only if the other side is a switch
      if (otherSideName.find("s") != std::string::npos)
      {
        portInfo.switchPort = true;

        /* Some logic maybe*/

      }
    }
  }

  void
    P4SwitchNetSeer::InitPortInfo(NetSeerPortInfo& portInfo)
  {

    /* Initialize port data structures */
    portInfo.sender_next_expected_seq = 0;
    portInfo.receiver_next_expected_seq = 0;
    portInfo.ring_buffer = std::vector<NetSeerPacketInfo>(m_bufferSize);
    portInfo.event_cache = std::vector<NetSeerPacketInfo>(m_eventCacheSize);

    /* set the number of hashes we need for the algorithm */
    m_hash = std::make_unique<HashUtils>(1);

  }

  void P4SwitchNetSeer::DoParser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
  {
    NS_LOG_FUNCTION_NOARGS();

    switch (protocol)
    {
    case NETSEER:
    {
      ParseNetSeer(packet, meta);
      break;
    }
    case IPV4:
    {
      ParseIpv4(packet, meta);
      break;
    }
    case ARP:
    {
      ParseArp(packet, meta);

      break;
    }
    case IPV6:
    {
      ParseIpv6(packet, meta);
      break;
    }
    }
  }

  void P4SwitchNetSeer::ParseNetSeer(Ptr<Packet> packet, pkt_info& meta)
  {
    NetSeerHeader net_seer_hdr;
    packet->RemoveHeader(net_seer_hdr);
    meta.headers["NETSEER"] = net_seer_hdr;
    /* Parse next layer*/
    Parser(packet, meta, net_seer_hdr.GetNextHeader());
  }

  void P4SwitchNetSeer::ParseArp(Ptr<Packet> packet, pkt_info& meta)
  {
    ArpHeader arp_hdr;
    packet->RemoveHeader(arp_hdr);
    meta.headers["ARP"] = arp_hdr;
  }

  void P4SwitchNetSeer::ParseIpv4(Ptr<Packet> packet, pkt_info& meta)
  {
    Ipv4Header ipv4_hdr;
    packet->RemoveHeader(ipv4_hdr);
    meta.headers["IPV4"] = ipv4_hdr;
    ParseTransport(packet, meta, ipv4_hdr.GetProtocol());
  }

  void P4SwitchNetSeer::ParseTransport(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
  {
    switch (protocol)
    {
    case TCP:
    {
      TcpHeader tcp_hdr;
      packet->RemoveHeader(tcp_hdr);
      meta.headers["TCP"] = tcp_hdr;

      break;
    }
    case UDP:
    {
      UdpHeader udp_hdr;
      packet->RemoveHeader(udp_hdr);
      meta.headers["UDP"] = udp_hdr;
      break;
    }
    }
  }

  void P4SwitchNetSeer::ParseIpv6(Ptr<Packet> packet, pkt_info& meta)
  {

  }

  void
    P4SwitchNetSeer::DoVerifyChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void
    P4SwitchNetSeer::SendNACK(Ptr<NetDevice> outPort, uint32_t seq1, uint32_t seq2, uint32_t times)
  {
    uint32_t ifIndex = outPort->GetIfIndex();
    NetSeerPortInfo& portInfo = m_portsInfo[ifIndex];

    Ptr<Packet> packet = Create<Packet>();
    NetSeerHeader net_seer_hdr;

    net_seer_hdr.SetAction(NACK);
    net_seer_hdr.SetSeq1(seq1);
    net_seer_hdr.SetSeq2(seq2);
    // net_seer_hdr.SetNextHeader(meta.protocol);

    pkt_info meta(outPort->GetAddress(), portInfo.otherPortDevice->GetAddress(), NETSEER);
    meta.headers["NETSEER"] = net_seer_hdr;
    meta.outPort = outPort;

    /* skip stuff */
    meta.internalPacket = true;

    /* Send several times like in the paper */
    for (uint32_t i = 0; i < times; i++)
    {
      TrafficManager(packet, meta);
    }

  }

  void
    P4SwitchNetSeer::DoIngress(Ptr<const Packet> packet, pkt_info& meta)
  {

    NS_LOG_FUNCTION_NOARGS();

    /* Get flow tuple */
    meta.flow = GetFlowFiveTuple(meta);

    // Do Normal forwarding
    if (m_forwardingType == P4SwitchNetDevice::ForwardingType::PORT_FORWARDING)
    {
      PortForwarding(meta);
    }
    else if (m_forwardingType == P4SwitchNetDevice::ForwardingType::L3_SPECIAL_FORWARDING)
    {
      L3SpecialForwarding(meta);
    }

    /* set input timestamp for debugging */
    NetSeerPortInfo& portInfo = m_portsInfo[meta.inPort->GetIfIndex()];
    portInfo.last_time_received = Simulator::Now();

    /* Do ingress logic */
    if (meta.headers.count("NETSEER") > 0)
    {
      NetSeerHeader& net_seer_header = std::any_cast<NetSeerHeader&>(meta.headers["NETSEER"]);
      uint8_t action = net_seer_header.GetAction();

      /* Packet that comes from the other side and goes to the egress pipe */
      if (action == NACK)
      {
        NS_LOG_DEBUG("NACK AT INGRESS");
        meta.outPort = meta.inPort;
      }

      else if (action == SEQ)
      {
        /* We check if the sequence is wrong */
        uint32_t seq = net_seer_header.GetSeq1();
        if (seq != portInfo.receiver_next_expected_seq)
        {

          /* We generate a new packet that will
           * be used to notify the other side
           */

           /* SEQ where the losses start*/
          uint32_t seq1 = portInfo.receiver_next_expected_seq;
          /* Good SEQ*/
          uint32_t seq2 = seq;

          /* next expected seq*/
          portInfo.receiver_next_expected_seq = seq + 1;

          NS_LOG_DEBUG("Missmatch in sequences detected: " << seq1 << "<->" << seq2 << "(not included)");

          /* Sends packet */
          SendNACK(meta.inPort, seq1, seq2, 1);

        }
        else {
          /* Increase next expected seq */
          NS_LOG_DEBUG("Sequence number expected " << seq << " " << Simulator::Now().GetSeconds());
          portInfo.receiver_next_expected_seq++;
        }
      }
    }
  }

  void
    P4SwitchNetSeer::DoTrafficManager(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    /* Lets emulate congestion drops */

    /* We just used this to show that
       congestion does not affect us*/

    if (m_tm_drop_rate > 0)
    {
      // Not sure why this copy was here in first place
      Ptr<Packet> pkt = packet->Copy();
      /* drop packet */
      if (tm_em->IsCorrupt(pkt))
      {
        /* show packet dropped */
        //std::cout << "TM PACKET DROPPED AT (" << Names::FindName(m_node) << ")" << std::endl;
        //PrintIpFiveTuple(meta.flow);
        return;
      }
    }
    Egress(packet, meta);
  }

  void
    P4SwitchNetSeer::SaveFlow(NetSeerPortInfo& port, pkt_info& meta)
  {
    /* gets circular index */
    uint32_t buffer_index = port.sender_next_expected_seq % m_bufferSize;

    /* Save packet */
    port.ring_buffer[buffer_index].src_ip = meta.flow.src_ip;
    port.ring_buffer[buffer_index].dst_ip = meta.flow.dst_ip;
    port.ring_buffer[buffer_index].protocol = meta.flow.protocol;
    port.ring_buffer[buffer_index].src_port = meta.flow.src_port;
    port.ring_buffer[buffer_index].dst_port = meta.flow.dst_port;
    port.ring_buffer[buffer_index].seq = port.sender_next_expected_seq;
    port.ring_buffer[buffer_index].reported = false;
  }

  ip_five_tuple
    P4SwitchNetSeer::PacketInfoToFiveTuple(NetSeerPacketInfo packet)
  {
    ip_five_tuple flow;

    flow.src_ip = packet.src_ip;
    flow.dst_ip = packet.dst_ip;
    flow.src_port = packet.src_port;
    flow.dst_port = packet.dst_port;
    flow.protocol = packet.protocol;
    return flow;
  }

  bool
    P4SwitchNetSeer::ComparePacketInfos(NetSeerPacketInfo pkt1, NetSeerPacketInfo pkt2)
  {
    if (pkt1.src_ip == pkt2.src_ip &&
      pkt1.dst_ip == pkt2.dst_ip &&
      pkt1.src_port == pkt2.src_port &&
      pkt1.dst_port == pkt2.dst_port &&
      pkt1.protocol == pkt2.protocol)
    {
      return true;
    }

    return false;
  }

  /* TODO: before we close the program we could report all the events that are still in the cache */
  //void
  //  P4SwitchNetSeer::ReportAllCahedEvents()
  //{
//
  //}

  void
    P4SwitchNetSeer::ReportPacket(NetSeerPortInfo& port, NetSeerPacketInfo packet)
  {

    /* Store packet to the event cache */
    ip_five_tuple flow = PacketInfoToFiveTuple(packet);

    char s[13];
    IpFiveTupleToBuffer(s, flow);

    /* hash index */
    uint32_t hash_index = m_hash->GetHash(s, 13, 0, m_eventCacheSize);

    //std::cout << Ipv4Address(packet.dst_ip) << " " << hash_index << std::endl;
    /* Cache algorithm -> Algorithm 1 In the paper */

    /* if the packet in the cache is the same we increase the counter */
    if (ComparePacketInfos(port.event_cache[hash_index], packet))
    {
      port.event_cache[hash_index].seq++;
      /* we use seq as counter*/
      if (port.event_cache[hash_index].seq >= m_eventCounter)
      {
        /* Report Event */
        NS_LOG_DEBUG(
          "EVENT LOSS DETECTED AT ("
          << Names::FindName(m_node) << ")"
          << " with SEQ->" << packet.seq);

        NS_LOG_DEBUG(
          Ipv4Address(flow.src_ip)
          << " " << Ipv4Address(flow.dst_ip)
          << " " << flow.src_port
          << " " << flow.dst_port
          << " " << int(flow.protocol)
          << " " << int(flow.id));

        /* log to output */
        m_simState->SetFailureEvent(Simulator::Now().GetSeconds(), flow, port.event_cache[hash_index].seq);
        m_unique_detected_prefixes.insert(flow.dst_ip & 0xffffff00);

        /* Reset counter */
        port.event_cache[hash_index].seq = 0;
      }
    }
    // we evict due to a collision
    else
    {
      /* Count current packet so we do +1 */
      uint32_t counter = port.event_cache[hash_index].seq + 1;

      port.event_cache[hash_index].src_ip = packet.src_ip;
      port.event_cache[hash_index].dst_ip = packet.dst_ip;
      port.event_cache[hash_index].src_port = packet.src_port;
      port.event_cache[hash_index].dst_port = packet.dst_port;
      port.event_cache[hash_index].protocol = packet.protocol;
      port.event_cache[hash_index].seq = 0;

      /* Collision and eviction +  Report event */
      NS_LOG_DEBUG(
        "EVENT LOSS DETECTED AT ("
        << Names::FindName(m_node) << ")"
        << " with SEQ->" << packet.seq);

      NS_LOG_DEBUG(
        Ipv4Address(flow.src_ip)
        << " " << Ipv4Address(flow.dst_ip)
        << " " << flow.src_port
        << " " << flow.dst_port
        << " " << int(flow.protocol)
        << " " << int(flow.id));

      /* log to output */
      m_simState->SetFailureEvent(Simulator::Now().GetSeconds(), flow, counter);
      m_unique_detected_prefixes.insert(flow.dst_ip & 0xffffff00);
    }
    /* Early stop simulation */
    if (m_early_stop_counter > 0 && m_unique_detected_prefixes.size() == m_early_stop_counter)
    {
      std::cout << "EARLY STOP SIMULATION" << std::endl;
      Simulator::Stop();
    }
  }

  void
    P4SwitchNetSeer::CheckPacketLosses(NetSeerPortInfo& port, uint32_t seq1, uint32_t seq2)
  {

    for (uint32_t seq = seq1; seq < seq2; seq++)
    {
      uint32_t buffer_index = seq % m_bufferSize;
      /* packet loss, however only report if seqs are the same */
      if ((port.ring_buffer[buffer_index].seq == seq) && (!port.ring_buffer[buffer_index].reported))
      {
        /* set reported flag just in case */
        port.ring_buffer[buffer_index].reported = true;
        port.detected_count++;

        /* Report packet lost */
        ReportPacket(port, port.ring_buffer[buffer_index]);
      }
      /* for debugging might remove later */
      /* this can happen when there is a ring wrap? */
      else
      {
        /* we only print the info message if it was because of seq and not because of not being reported */
        if (!port.ring_buffer[buffer_index].reported)
        {
          NS_LOG_DEBUG("Sequence does not match the buffer seq " << port.ring_buffer[buffer_index].seq << " != " << seq);
        }
      }
    }
  }

  void
    P4SwitchNetSeer::DoEgress(Ptr<const Packet> packet, pkt_info& meta)
  {

    NS_LOG_FUNCTION_NOARGS();

    /* Internal packet, we do not do anything */
    if (meta.internalPacket)
    {
      NS_LOG_DEBUG("Skips egress");
      return;
    }

    /* Do egress logic */
    NetSeerPortInfo& outPortInfo = m_portsInfo[meta.outPort->GetIfIndex()];

    if (outPortInfo.switchPort)
    {
      /* if we sending to a switch and the header is not here we add it */
      if (not(meta.headers.count("NETSEER") > 0))
      {
        NetSeerHeader net_seer_hdr;
        net_seer_hdr.SetAction(SEQ);
        net_seer_hdr.SetSeq1(outPortInfo.sender_next_expected_seq);
        net_seer_hdr.SetNextHeader(meta.protocol);
        meta.protocol = NETSEER;
        meta.headers["NETSEER"] = net_seer_hdr;
      }

      /* Get net seer header*/
      NetSeerHeader& net_seer_header = std::any_cast<NetSeerHeader&>(meta.headers["NETSEER"]);
      uint8_t action = net_seer_header.GetAction();

      /* We received a nack from the ohter side */
      if (action == NACK)
      {
        /* Report losses in the case there is something wrong */
        CheckPacketLosses(outPortInfo, net_seer_header.GetSeq1(), net_seer_header.GetSeq2());

        /* drop packet */
        meta.drop_flag = true;
      }
      /* normal seq tagging */
      else
      {
        /* save flow in the data structure */
        SaveFlow(outPortInfo, meta);

        /* Increase next expected seq */
        outPortInfo.sender_next_expected_seq++;
      }

    }
    /* check if we have to remove the header just in case */
    else
    {
      /* Remove net seer header*/
      if (meta.headers.count("NETSEER") > 0)
      {
        /* Remove net seer header */
        meta.protocol = std::any_cast<NetSeerHeader>(meta.headers["NETSEER"]).GetNextHeader();
        meta.headers.erase("NETSEER");
      }
    }
  }

  void
    P4SwitchNetSeer::DoUpdateChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void
    P4SwitchNetSeer::DoDeparser(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    // Deparse valid headers if there are headers to deparse
    if (meta.headers.size() > 0)
    {
      for (auto hdr_type : header_stack_reverse)
      {
        if (meta.headers.count(hdr_type) > 0)
        {
          if (hdr_type == "NETSEER")
          {
            packet->AddHeader(std::any_cast<NetSeerHeader>(meta.headers[hdr_type]));
          }
          else if (hdr_type == "ARP")
          {
            packet->AddHeader(std::any_cast<ArpHeader>(meta.headers[hdr_type]));
          }
          else if (hdr_type == "IPV4")
          {
            packet->AddHeader(std::any_cast<Ipv4Header>(meta.headers[hdr_type]));
          }
          else if (hdr_type == "TCP")
          {
            packet->AddHeader(std::any_cast<TcpHeader>(meta.headers[hdr_type]));
          }
          else if (hdr_type == "UDP")
          {
            packet->AddHeader(std::any_cast<UdpHeader>(meta.headers[hdr_type]));
          }
        }
      }
    }
  }

  void
    P4SwitchNetSeer::DoEgressTrafficManager(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    NetSeerPortInfo& outPortInfo = m_portsInfo[meta.outPort->GetIfIndex()];

    if (meta.headers.count("IPV4") > 0)
    {
      // drop packet at the egress
      Ipv4Header& ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
      uint8_t tos = ipv4_hdr.GetTos();

      /* Here is where we decide if a packet has to be dropped or not
         as an easy way our sender the TOS field to a number, if that number
         is this switch ID then we drop the packet with "m_fail_drop_rate" probability
      */

      if (tos == m_switchId || meta.gray_drop)
      {
        /* only normal traffic, we do not drop good traffic */
        //Ptr<Packet> pkt = packet->Copy(); 
        /* drop packet */
        if (m_fail_drop_rate == 1 || fail_em->IsCorrupt(packet))
        {
          /* show packet dropped */
          //std::cout << "EGRESS PACKET DROPPED AT (" << Names::FindName(m_node) << ")" << std::endl;
          //PrintIpFiveTuple(meta.flow);
          m_drop_times.push_back(Simulator::Now());
          outPortInfo.failures_count++;
          return;
        }
      }
    }

    // Before the packet leaves the switch
    // Update Send Timestamp
    outPortInfo.last_time_sent = Simulator::Now();
    /* Removed the sense of l2 swtich we had before, now for each packet we modify the mac address, useful to track what happens */
    SendOutFrom(packet, meta.outPort, meta.outPort->GetAddress(), outPortInfo.otherPortDevice->GetAddress(), meta.protocol);
    //SendOutFrom(packet, meta.outPort, meta.src, meta.dst, meta.protocol);
  }

  void
    P4SwitchNetSeer::DoDispose()
  {
    NS_LOG_FUNCTION_NOARGS();

    std::cout << "\nStats for device: " << Names::FindName(m_node) << std::endl;

    for (uint32_t i = 0; i < m_ports.size(); i++)
    {
      uint32_t ifIndex = m_ports[i]->GetIfIndex();
      NetSeerPortInfo& portInfo = m_portsInfo[ifIndex];

      if (portInfo.failures_count > 0)
      {
        std::cout << "Port " << i << " Packets dropped: " << portInfo.failures_count << " Packets detected: " << portInfo.detected_count << std::endl;
      }
    }

    P4SwitchNetDevice::DoDispose();
  }

} // namespace ns3
