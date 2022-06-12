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
#include "p4-switch-nat.h"
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

NS_LOG_COMPONENT_DEFINE ("P4SwitchNAT");

NS_OBJECT_ENSURE_REGISTERED (P4SwitchNAT);

#define IPV4 0x0800
#define ARP  0x0806
#define IPV6 0x86DD
#define TCP  6
#define UDP  17

TypeId
P4SwitchNAT::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::P4SwitchNAT")
    .SetParent<P4SwitchNetDevice> ()
    .SetGroupName("P4-Switch")
    .AddConstructor<P4SwitchNAT> ()                                                                                                                  
    .AddAttribute ("TmDropRate", "Percentage of traffic that gets dropped by the traffic manager.",
                DoubleValue (0),
                MakeDoubleAccessor (&P4SwitchNAT::m_tm_drop_rate),
                MakeDoubleChecker<double> (0,1))
    .AddAttribute ("FailDropRate", "Percentage of traffic that gets dropped when a prefix is affected.",
                DoubleValue (0),
                MakeDoubleAccessor (&P4SwitchNAT::m_fail_drop_rate),
                MakeDoubleChecker<double> (0,1))                                                           
  ;
  return tid;
}

P4SwitchNAT::P4SwitchNAT ()
  :
    header_stack_reverse(header_stack)
{
   std::reverse(header_stack_reverse.begin(), header_stack_reverse.end());
}

P4SwitchNAT::~P4SwitchNAT()
{
  NS_LOG_FUNCTION_NOARGS ();
  if (m_hash)
  {
    m_hash->Clean ();
  }

}

void 
P4SwitchNAT::AddSwitchPort (Ptr<NetDevice> switchPort)
{
  NS_LOG_FUNCTION_NOARGS ();
 
  P4SwitchNetDevice::AddSwitchPort(switchPort);

  uint32_t ifIndex = switchPort->GetIfIndex();
  NatPortInfo &portInfo = m_portsInfo[ifIndex];

  //NS_LOG_DEBUG("Address of original port info: " <<  &portInfo);
  //  
  //InitPortInfo();
//
  /* With this we copy the parents hiden attribute info */
  /* https://stackoverflow.com/questions/57997870/nested-struct-attributes-inheritance#57997920 */
//
  PortInfo &switchInfo = P4SwitchNetDevice::m_portsInfo[ifIndex];
  portInfo.portDevice = switchInfo.portDevice;
  portInfo.otherPortDevice = switchInfo.otherPortDevice;
  portInfo.switchPort = switchInfo.switchPort;

  portInfo.linkState = true;

  Ptr<Node> thisSide = switchPort->GetNode();
  Ptr<Node> otherSide = portInfo.otherPortDevice->GetNode();
  std::string thisSideName = Names::FindName(thisSide);
  std::string otherSideName = Names::FindName(otherSide);

  
  ///* Set link name */
  portInfo.link_name = thisSideName + "->" + otherSideName;
  NS_LOG_UNCOND("Interface mapping: " << portInfo.link_name << " : " << switchPort->GetIfIndex());

  /* Set interface nat type */
  if (otherSideName.empty())
  {
    NS_FATAL_ERROR("The node in the other side was not named.");
  } 

  // The state machine is lunched only if the other side is a switch
  if (otherSideName.find("s") != std::string::npos)
  {
    portInfo.is_nat_interface = true;     
    m_natInterface = switchPort; 
  }
  else
  {
      /* set nat port info */
      portInfo.is_nat_interface = false;
      TranslatePort nat_port_info;
      nat_port_info.port = switchPort;
      nat_port_info.other_side_ip = GetNodeIp(otherSide).Get();
      m_natPorts.push_back(nat_port_info);
  }

}

void 
P4SwitchNAT::CleanMapper(Time interval)
{

}

void 
P4SwitchNAT::SetDebug (bool state)
{
  m_enableDebug = state;
}

void
P4SwitchNAT::Init()
{  
  // Forwarding table is set here
  P4SwitchNetDevice::Init();

  /* Sets tm drop error model */
  tm_em->SetAttribute ("ErrorRate", DoubleValue (m_tm_drop_rate));
  tm_em->SetAttribute ("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    /* Sets tm drop error model */
  fail_em->SetAttribute ("ErrorRate", DoubleValue (m_fail_drop_rate));
  fail_em->SetAttribute ("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

  /* set the number of hashes we need for the algorithm */
  m_hash = std::make_unique<HashUtils> (1);

  m_numNatPorts = m_natPorts.size();

}

void
P4SwitchNAT::InitPortInfo ()
{

}

void P4SwitchNAT::DoParser(Ptr<Packet> packet, pkt_info &meta, uint16_t protocol)
{
  NS_LOG_FUNCTION_NOARGS ();

  switch (protocol)
    {
      case IPV4:
      {
        ParseIpv4(packet, meta);
        break;
      }
      case ARP:
      {
        ParseArp (packet, meta);

        break;
      }
      case IPV6:
        {
          ParseIpv6 (packet, meta);
          break;
        }
    } 
}

void P4SwitchNAT::ParseArp (Ptr<Packet> packet, pkt_info &meta)
{
  ArpHeader arp_hdr;
  packet->RemoveHeader(arp_hdr);
  meta.headers["ARP"] = arp_hdr;
}

void P4SwitchNAT::ParseIpv4 (Ptr<Packet> packet, pkt_info &meta)
{
  Ipv4Header ipv4_hdr;
  packet->RemoveHeader(ipv4_hdr);
  meta.headers["IPV4"] = ipv4_hdr;
  ParseTransport(packet, meta, ipv4_hdr.GetProtocol());
}

void P4SwitchNAT::ParseTransport (Ptr<Packet> packet, pkt_info &meta, uint16_t protocol)
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

void P4SwitchNAT::ParseIpv6 (Ptr<Packet> packet, pkt_info &meta)
{

}

void 
P4SwitchNAT::DoVerifyChecksums(Ptr<const Packet> packet, pkt_info &meta)
{
  NS_LOG_FUNCTION_NOARGS ();
}


void 
P4SwitchNAT::DoIngress(Ptr<const Packet> packet, pkt_info &meta)
{

  NS_LOG_FUNCTION_NOARGS ();  

  if (meta.headers.count("IPV4")> 0)
  {
      
    Ipv4Header & ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
  
    // Set transport ports such that udp and tcp are unified
    ip_five_tuple flow = GetFlowFiveTuple(meta);
    NatPortInfo &portInfo = m_portsInfo[meta.inPort->GetIfIndex()];

    /* if it comes from outside */
    if (portInfo.is_nat_interface)
    {
      MapCell cell;
      cell.ip = flow.dst_ip;
      cell.last_accessed = Simulator::Now();

      char s[13];
      IpFiveTupleToBuffer(s, flow);    
      uint32_t hash_index = m_hash->GetHash(s, 13, 0, m_numNatPorts);

      TranslatePort nat_port = m_natPorts[hash_index];
      meta.outPort = nat_port.port;
    
      flow.dst_ip = nat_port.other_side_ip;
      std::string str_flow = IpFiveTupleToString(flow);

      NatPortInfo &outInfo = m_portsInfo[meta.outPort->GetIfIndex()];
      outInfo.nat_map[str_flow] = cell;

      ipv4_hdr.SetDestination(Ipv4Address(nat_port.other_side_ip));

    }
    /* if it comes from the network we are translating to */
    else
    {
      ip_five_tuple reverse_flow;

      reverse_flow.src_ip = flow.dst_ip;
      reverse_flow.dst_ip = flow.src_ip;
      reverse_flow.src_port = flow.dst_port;
      reverse_flow.dst_port = flow.src_port;
      reverse_flow.protocol = flow.protocol;

      MapCell cell = portInfo.nat_map[IpFiveTupleToString(reverse_flow)];
      ipv4_hdr.SetSource(Ipv4Address(cell.ip));

      /* Forwarding has to be done */
      meta.outPort = m_natInterface;

    }
  }

}

void
P4SwitchNAT::DoDispose ()
{
  NS_LOG_FUNCTION_NOARGS ();
  P4SwitchNetDevice::DoDispose ();
}

void 
P4SwitchNAT::DoTrafficManager(Ptr<const Packet> packet, pkt_info &meta)
{
  NS_LOG_FUNCTION_NOARGS ();

  if (m_tm_drop_rate > 0)
  { 
    // Not sure why this copy was here in first place
    Ptr<Packet> pkt = packet->Copy();
    /* drop packet */
    if (tm_em->IsCorrupt(pkt))
    {
      return;
    }
  }

  Egress(packet, meta);
}


void 
P4SwitchNAT::DoEgress(Ptr<const Packet> packet, pkt_info &meta)
{
  
  NS_LOG_FUNCTION_NOARGS ();

}

void 
P4SwitchNAT::DoUpdateChecksums(Ptr<const Packet> packet, pkt_info &meta)
{
  NS_LOG_FUNCTION_NOARGS ();
}

void 
P4SwitchNAT::DoDeparser(Ptr<Packet> packet, pkt_info &meta)
{
  NS_LOG_FUNCTION_NOARGS ();

   // Deparse valid headers if there are headers to deparse
  if (meta.headers.size() > 0)
  {
    for (auto hdr_type: header_stack_reverse)
    {
      if (meta.headers.count(hdr_type) > 0)
      {
        if (hdr_type == "ARP")
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
P4SwitchNAT::DoEgressTrafficManager(Ptr<Packet> packet, pkt_info &meta)
{
  NS_LOG_FUNCTION_NOARGS ();

  // To remove 

  if (meta.headers.count("IPV4")> 0)
  {
    // drop packet at the egress
    Ipv4Header & ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
    /* gets 4 lsb */
    uint8_t lo_tos = getTosLo(ipv4_hdr.GetTos());

    /* Here is where we decide if a packet has to be dropped or not
       as an easy way our sender the TOS field to a number, if that number
       is this switch ID then we drop the packet with "m_fail_drop_rate" probability
    */
  
    if (lo_tos == m_switchId)
    {
      /* only normal traffic, we do not drop good traffic */
      //Ptr<Packet> pkt = packet->Copy();
      /* drop packet */   

      /* I believe the == 1 is to do it faster*/ 
      if (m_fail_drop_rate == 1 or fail_em->IsCorrupt(packet))
      {
        return;
      }     
    }
  } 

  /* we get it from the base class because the nat does not have port info */
  PortInfo &outPortInfo = P4SwitchNetDevice::m_portsInfo[meta.outPort->GetIfIndex()];
  SendOutFrom(packet, meta.outPort, meta.outPort->GetAddress(), outPortInfo.otherPortDevice->GetAddress(), meta.protocol);
}

} // namespace ns3
  