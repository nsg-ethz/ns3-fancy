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
#include "p4-switch-net-device.h"
#include "ns3/utils.h"
#include "ns3/node.h"
#include "ns3/channel.h"
#include "ns3/packet.h"
#include "ns3/log.h"
#include "ns3/boolean.h"
#include "ns3/simulator.h"
#include "ns3/uinteger.h"
#include "ns3/names.h"
#include "ns3/global-value.h"

#include <fstream> 

using namespace std;

namespace ns3 {

  NS_LOG_COMPONENT_DEFINE("P4SwitchNetDevice");

  NS_OBJECT_ENSURE_REGISTERED(P4SwitchNetDevice);

  TypeId
    P4SwitchNetDevice::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::P4SwitchNetDevice")
      .SetParent<NetDevice>()
      .SetGroupName("P4-Switch")
      .AddConstructor<P4SwitchNetDevice>()
      .AddAttribute("Mtu", "The MAC-level Maximum Transmission Unit",
        UintegerValue(1500),
        MakeUintegerAccessor(&P4SwitchNetDevice::SetMtu,
          &P4SwitchNetDevice::GetMtu),
        MakeUintegerChecker<uint16_t>())
      // L2 learning attributes                  
      .AddAttribute("EnableLearning",
        "Enable the learning mode of the Learning Switch",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchNetDevice::m_enableLearning),
        MakeBooleanChecker())
      .AddAttribute("ExpirationTime",
        "Time it takes for learned MAC state entry to expire.",
        TimeValue(Seconds(300)),
        MakeTimeAccessor(&P4SwitchNetDevice::m_expirationTime),
        MakeTimeChecker())
      .AddAttribute("EnableDebug", "Enable or disable the debugging",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchNetDevice::m_enableDebug),
        MakeBooleanChecker())
      .AddAttribute("ForwardingType", "Type of Forwarding For this switch.",
        EnumValue(ForwardingType::PORT_FORWARDING),
        MakeEnumAccessor(&P4SwitchNetDevice::m_forwardingType),
        MakeEnumChecker(ForwardingType::PORT_FORWARDING, "PortForwarding",
          ForwardingType::L3_SPECIAL_FORWARDING, "L3SpecialForwarding",
          ForwardingType::L2_FORWARDING, "L2Forwarding"))
      ;
    return tid;
  }

  P4SwitchNetDevice::P4SwitchNetDevice()
    : m_enableDebug(false),
    m_enableDebugGlobal(false),
    m_node(0),
    m_ifIndex(0),
    m_enableL2Forwarding(true)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_channel = CreateObject<P4SwitchChannel>();
  }

  P4SwitchNetDevice::~P4SwitchNetDevice()
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void
    P4SwitchNetDevice::Init(void)
  {
    /* Load L2 Tables */
    m_name = Names::FindName(m_node);
    if (m_enableL2Forwarding)
    {
      //L2ForwardingFillTable("control_plane/s" + std::to_string(m_switchId) + "-commands.txt");
      // port to port forwarding
      FillTables("control_plane/s" + std::to_string(m_switchId) + "-port-commands.txt");

    }

    BooleanValue debug;
    GlobalValue::GetValueByName("debugGlobal", debug);
    m_enableDebugGlobal = debug.Get();

    /* Set bandwidth data structures */
    for (uint32_t i = 0; i < m_ports.size(); i++)
    {
      m_monitorOutBw[m_ports[i]->GetIfIndex()] = 0;
      m_monitorInBw[m_ports[i]->GetIfIndex()] = 0;
    }

  }

  ip_five_tuple
    P4SwitchNetDevice::GetFlowFiveTuple(pkt_info& meta)
  {
    ip_five_tuple five_tuple;

    if (meta.headers.count("IPV4") > 0)
    {
      Ipv4Header& ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
      five_tuple.src_ip = ipv4_hdr.GetSource().Get();
      five_tuple.dst_ip = ipv4_hdr.GetDestination().Get();
      five_tuple.protocol = ipv4_hdr.GetProtocol();
      five_tuple.id = ipv4_hdr.GetIdentification();

    }
    if (meta.headers.count("TCP") > 0 && five_tuple.protocol == 6)
    {
      TcpHeader& tcp_hdr = std::any_cast<TcpHeader&>(meta.headers["TCP"]);
      five_tuple.src_port = tcp_hdr.GetSourcePort();
      five_tuple.dst_port = tcp_hdr.GetDestinationPort();
    }
    else if (meta.headers.count("UDP") > 0 && five_tuple.protocol == 17)
    {
      UdpHeader& udp_hdr = std::any_cast<UdpHeader&>(meta.headers["UDP"]);
      five_tuple.src_port = udp_hdr.GetSourcePort();
      five_tuple.dst_port = udp_hdr.GetDestinationPort();
    }
    return five_tuple;
  }

  void
    P4SwitchNetDevice::FillTables(std::string conf_path)
  {
    /* This probably should not be in the Switch base classe */

    /* Forwarding for the old scenario */
      // Do Normal forwarding

    if (m_forwardingType == P4SwitchNetDevice::ForwardingType::PORT_FORWARDING)
    {
      PortForwardingFillTable(conf_path);
    }
    else if (m_forwardingType == P4SwitchNetDevice::ForwardingType::L3_SPECIAL_FORWARDING)
    {
      L3SpecialForwardingFillTable();
    }
  }

  void
    P4SwitchNetDevice::PrintBandwidth(double delay, uint32_t intf_index, uint64_t prev_counter, bool out)
  {

    /* from intf index get the interface (just for the name)*/
    PortInfo& portInfo = m_portsInfo[intf_index];

    Ptr<Node> this_side = portInfo.portDevice->GetNode();
    Ptr<Node> other_side = portInfo.otherPortDevice->GetNode();

    std::string thisSideName = Names::FindName(this_side);
    std::string otherSideName = Names::FindName(other_side);

    std::string full_link;

    /* compute bw rate */
    uint64_t total_bytes_sent;

    /* link direction */
    if (out)
    {
      full_link = thisSideName + "->" + otherSideName;
      total_bytes_sent = m_monitorOutBw[intf_index];
    }
    else
    {
      full_link = otherSideName + "->" + thisSideName;
      total_bytes_sent = m_monitorInBw[intf_index];
    }

    /* compute bytes difference */
    uint64_t rate_per_sec = uint64_t((total_bytes_sent - prev_counter) / delay);
    prev_counter = total_bytes_sent;

    std::string bw = data_rate_to_str(DataRate(rate_per_sec * 8));

    /* print */
    std::cout << "\033[1;34mBw(" << full_link << "): " << bw << "\033[0m" << std::endl;

    Simulator::Schedule(Seconds(m_bwPrintFrequency), &P4SwitchNetDevice::PrintBandwidth, this, m_bwPrintFrequency, intf_index, prev_counter, out);
  }

  void
    P4SwitchNetDevice::StartBandwidthMeasurament(void)
  {
    /* Enable the bandwidth printings */
    for (uint32_t i = 0; i < m_enableMonitorOutPorts.size(); i++)
    {
      Simulator::Schedule(Seconds(m_bwPrintFrequency), &P4SwitchNetDevice::PrintBandwidth, this, m_bwPrintFrequency, m_enableMonitorOutPorts[i], 0, true);
    }

    for (uint32_t i = 0; i < m_enableMonitorInPorts.size(); i++)
    {
      Simulator::Schedule(Seconds(m_bwPrintFrequency), &P4SwitchNetDevice::PrintBandwidth, this, m_bwPrintFrequency, m_enableMonitorInPorts[i], 0, false);
    }
  }

  void
    P4SwitchNetDevice::EnableOutBandwidthPrint(Ptr<NetDevice> port)
  {
    m_enableMonitorOutPorts.push_back(port->GetIfIndex());
  }

  void
    P4SwitchNetDevice::EnableInBandwidthPrint(Ptr<NetDevice> port)
  {
    m_enableMonitorInPorts.push_back(port->GetIfIndex());
  }

  void
    P4SwitchNetDevice::SetBandwidthPrintInterval(double interval)
  {
    m_bwPrintFrequency = interval;
  }

  void P4SwitchNetDevice::SaveDrops(std::string outFile)
  {
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> file =
      asciiTraceHelper.CreateFileStream(outFile);

    for (auto const& d : m_drop_times)
    {
      *(file->GetStream()) << d.GetSeconds() << "\n";
    }
    file->GetStream()->flush();
  }

  void
    P4SwitchNetDevice::SetDebug(bool state)
  {
    m_enableDebug = state;
  }

  void
    P4SwitchNetDevice::DoDispose()
  {
    NS_LOG_FUNCTION_NOARGS();
    for (std::vector< Ptr<NetDevice> >::iterator iter = m_ports.begin(); iter != m_ports.end(); iter++)
    {
      *iter = 0;
    }
    m_ports.clear();
    m_channel = 0;
    m_node = 0;
    NetDevice::DoDispose();
  }

  // Switch Pipeline Skeleton
  void
    P4SwitchNetDevice::ReceiveFromDevice(Ptr<NetDevice> incomingPort, Ptr<const Packet> packet, uint16_t protocol,
      Address const& src, Address const& dst, PacketType packetType)
  {
    NS_LOG_FUNCTION_NOARGS();

    // All promiscuous packets are sent up, this is done for tracing pruposes i guess
    if (!m_promiscRxCallback.IsNull())
    {
      m_promiscRxCallback(this, packet, protocol, src, dst, packetType);
    }


    /* Process Packet and Generate metadata for it */
    Ptr<Packet> pkt = packet->Copy();
    pkt_info meta(src, dst, protocol);
    // Set Incoming port
    meta.inPort = incomingPort;
    // Saves the packet type: for us, broadcast, or unicast (assumed from the mac address)
    meta.packetType = packetType;

    // Parser
    Parser(pkt, meta, meta.protocol);

    // Call the ingress this triggers the pipeline sequence 
    //std::cout << "META ADDRESS " << &meta << std::endl;
    Ingress(pkt, meta);
  }

  void P4SwitchNetDevice::Parser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
  {
    NS_LOG_FUNCTION_NOARGS();

    DoParser(packet, meta, protocol);

  }

  void
    P4SwitchNetDevice::DoParser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void P4SwitchNetDevice::VerifyChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    DoVerifyChecksums(packet, meta);

  }

  void
    P4SwitchNetDevice::DoVerifyChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void P4SwitchNetDevice::Ingress(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    /* count input bw*/
    m_monitorInBw[meta.inPort->GetIfIndex()] += packet->GetSize() + 18;

    DoIngress(packet, meta);

    /* TO REMOVE Experiment*/
    //m_input_count++;
    //if (m_input_count % 10000 == 0)
    //{
    //  std::cout << Names::FindName(m_node) << " Sim Time to send packets: " << Simulator::Now().GetSeconds() - m_time_difference.GetSeconds//() << std::endl;
    //  std::cout << Names::FindName(m_node) << " Real Time to send packets: " << (float (clock () - m_real_time_difference) / //CLOCKS_PER_SEC) << std::endl;
    //  m_time_difference = Simulator::Now();
    //  m_real_time_difference = std::clock();
  //
    //  Ptr<Queue<Packet>> q = DynamicCast<CsmaNetDevice>(meta.outPort)->GetQueue();
    //  std::cout << Names::FindName(m_node) << " Queue occupancy " << q->GetNPackets() << std::endl;
    //}

    TrafficManager(packet, meta);
  }

  void
    P4SwitchNetDevice::DoIngress(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    if (m_enableLearning)
    {
      L2Learning(packet, meta);
    }
  }

  void P4SwitchNetDevice::TrafficManager(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    /* We run things in the following order */

    /*
     * if the packet was marked to be dropped
     * we do not let it pass
     */

    if (meta.drop_flag == true)
    {
      return;
    }
    /*
     * If the packet carries a clone flag, we make a copy and send it through
     */

    if (meta.cloneType != 0)
    {
      pkt_info meta_copy = CopyMeta(meta);
      meta_copy.outPort = meta.cloneOutPort;

      /* Clean original packet */
      meta.cloneType = 0;
      meta.cloneOutPort = NULL;

      switch (meta.cloneType)
      {
      case I2E:
        meta_copy.instanceType = 2;
        Simulator::ScheduleNow(&P4SwitchNetDevice::TrafficManager, this, packet, meta_copy);
        break;
      case E2E:
        meta_copy.instanceType = 3;
        Simulator::ScheduleNow(&P4SwitchNetDevice::TrafficManager, this, packet, meta_copy);
        break;
      }
    }

    /*
     * Broadcast Implementation
     */
    if (meta.broadcast_flag)
    {
      for (std::vector< Ptr<NetDevice> >::iterator iter = m_ports.begin();
        iter != m_ports.end(); iter++)
      {
        Ptr<NetDevice> port = *iter;
        if (port != meta.inPort)
        {
          // This packet gets dropped
          pkt_info meta_copy = CopyMeta(meta);
          meta_copy.outPort = port;
          Simulator::ScheduleNow(&P4SwitchNetDevice::TrafficManager, this, packet, meta_copy);
        }
      }

      /* This packet does not need to go through */
      return;
    }

    /* If the packet does not have an output port here
     * we drop it
     */

    if (meta.outPort == NULL)
    {
      return;
    }

    /* An empty queueing system */
    DoTrafficManager(packet, meta);

  }

  void
    P4SwitchNetDevice::DoTrafficManager(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    Egress(packet, meta);
  }

  void P4SwitchNetDevice::Egress(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    DoEgress(packet, meta);

    UpdateChecksums(packet, meta);
  }

  void
    P4SwitchNetDevice::DoEgress(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void P4SwitchNetDevice::UpdateChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    DoUpdateChecksums(packet, meta);

    Ptr<Packet> pkt = packet->Copy();
    Deparser(pkt, meta);
  }

  void
    P4SwitchNetDevice::DoUpdateChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void P4SwitchNetDevice::Deparser(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    DoDeparser(packet, meta);

    EgressTrafficManager(packet, meta);
  }

  void
    P4SwitchNetDevice::DoDeparser(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void P4SwitchNetDevice::EgressTrafficManager(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    /* We run things in the following order */

    /*
     * if the packet was marked to be dropped
     * we do not let it pass
     */

    if (meta.drop_flag == true)
    {
      return;
    }

    if (meta.outPort == NULL)
    {
      return;
    }

    /* An empty queueing system */
    DoEgressTrafficManager(packet, meta);
    //std::cout << "META ADDRESS " << &meta << std::endl;
    //Simulator::Stop(Seconds(0));
  }

  void
    P4SwitchNetDevice::DoEgressTrafficManager(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    SendOutFrom(packet, meta.outPort, meta.src, meta.dst, meta.protocol);
  }

  void P4SwitchNetDevice::SendOutFrom(Ptr<Packet> packet, Ptr<NetDevice> outPort, const Address& src, const Address& dst, uint16_t protocolNumber)
  {
    NS_LOG_FUNCTION_NOARGS();
    if (outPort != NULL)
    {

      /* Count bandwidth in the case we have installed a meter for this port*/
      //std::cout << "packet size " << packet->GetSize() << " " << protocolNumber << std::endl;
      /* We add 18 for the ethernet header */
      m_monitorOutBw[outPort->GetIfIndex()] += packet->GetSize() + 18;

      /* Send Packet */
      outPort->SendFrom(packet, src, dst, protocolNumber);
    }
  }

  P4SwitchNetDevice::pkt_info P4SwitchNetDevice::CopyMeta(pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    pkt_info meta_copy(meta.src, meta.dst, meta.protocol);
    meta_copy.inPort = meta.inPort;
    meta_copy.outPort = meta.outPort;
    meta_copy.packetType = meta.packetType;
    meta_copy.headers = meta.headers;
    return meta_copy;
  }

  void P4SwitchNetDevice::Clone(Ptr<const Packet> packet, Ptr<NetDevice> outPort, CloneType cloneType, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    meta.cloneOutPort = outPort;
    meta.cloneType = cloneType;

  }

  /*
   * Forwarding Tables
   * =================
   *
   * Here we can find a set of different methods to forward packets, ranging from
   * basic port forwarding, l2 learning or L3 forwarding.
   */

   // L2 Forwarding

  void P4SwitchNetDevice::L2ForwardingFillTable(std::string table_path)
  {
    std::ifstream l2_table_file(table_path);
    NS_ASSERT_MSG(l2_table_file, "Provide a valid table file path");

    std::string line;
    std::string mac_addr;
    uint32_t out_port;

    while (std::getline(l2_table_file, line))
    {
      if (line.empty())
      {
        continue;
      }

      std::istringstream lineStream(line);
      lineStream >> mac_addr >> out_port;

      Mac48Address dstMac(mac_addr.c_str());
      Ptr<NetDevice> outPort = m_ports[out_port];
      NS_LOG_DEBUG("L2 Forwarding fill table: " << mac_addr << " " << out_port);
      m_L2Table[dstMac] = outPort;
    }
  }

  void P4SwitchNetDevice::L2Forwarding(pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    Mac48Address dst48 = Mac48Address::ConvertFrom(meta.dst);

    std::map<Mac48Address, Ptr<NetDevice>>::iterator iter = m_L2Table.find(dst48);
    if (iter != m_L2Table.end())
    {
      meta.outPort = iter->second;
    }
    else
    {
      meta.outPort = NULL;
      //Broadcast(meta);
    }
  }

  // Port based forwarding

  void P4SwitchNetDevice::PortForwardingFillTable(std::string table_path)
  {
    std::ifstream port_table_file(table_path);
    NS_ASSERT_MSG(port_table_file, "Provide a valid table file path");

    std::string line;
    uint32_t in_port;
    uint32_t out_port;

    m_PortTable.clear();

    while (std::getline(port_table_file, line))
    {
      if (line.empty())
      {
        continue;
      }

      std::istringstream lineStream(line);
      lineStream >> in_port >> out_port;
      Ptr<NetDevice> outPort = m_ports[out_port];
      NS_LOG_DEBUG("Port Forwarding fill table: " << in_port << " " << out_port);
      m_PortTable[in_port] = outPort;
    }
  }

  void P4SwitchNetDevice::PortForwarding(pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    uint32_t port_index = meta.inPort->GetIfIndex();

    std::map<uint32_t, Ptr<NetDevice>>::iterator iter = m_PortTable.find(port_index);
    if (iter != m_PortTable.end())
    {
      meta.outPort = iter->second;
    }
    else
    {
      meta.outPort = NULL;
    }
  }

  // Special L3 forwarding: We do not allow lpm forwarding. We just use /32
  // address indexing. This forwarding is special for our dumbell topology all the
  // leaves will have ips to be redirected to whereas  when there is a miss packets get 
  // forwarded to the middle link.

  void P4SwitchNetDevice::L3SpecialForwardingFillTable()
  {
    NS_LOG_FUNCTION_NOARGS();

    m_L3Table.clear();

    /* Iterate between all directly connected hosts and get ips*/
    for (uint32_t i = 0; i < m_portsInfo.size(); i++)
    {
      PortInfo& portInfo = m_portsInfo[i];

      Ptr<NetDevice> outPort = portInfo.portDevice;

      Ptr<Node> otherSide = portInfo.otherPortDevice->GetNode();
      std::string otherSideName = Names::FindName(otherSide);

      /* There is no ipv4 stack in the other side, for example the other side its
      a P4 switch */
      if (otherSide->GetObject<Ipv4>() == 0)
      {
        NS_LOG_DEBUG("Switch node without interface: " << otherSideName);
        m_L3Gateway = outPort;
        continue;
      }

      Ipv4Address ip = GetNodeIp(otherSide);
      NS_LOG_DEBUG("Looking for nodes: " << otherSideName << " " << ip);
      m_L3Table[ip.Get()] = std::make_pair(outPort, false);
    }
  }

  void P4SwitchNetDevice::L3SpecialForwardingSetFailures(std::vector<std::pair<uint32_t, Ptr<NetDevice>>> prefixes)
  {
    for (uint32_t i = 0; i < prefixes.size(); i++)
    {
      std::pair<uint32_t, Ptr<NetDevice>> prefix = prefixes[i];

      if (prefix.second == NULL)
      {
        m_L3Table[prefix.first] = std::make_pair(m_L3Gateway, true);
      }
      else
      {
        m_L3Table[prefix.first] = std::make_pair(prefix.second, true);
      }
    }
  }

  void P4SwitchNetDevice::L3SpecialForwardingRemoveFailures(std::vector<std::pair<uint32_t, Ptr<NetDevice>>> prefixes)
  {
    for (uint32_t i = 0; i < prefixes.size(); i++)
    {
      std::pair<uint32_t, Ptr<NetDevice>> prefix = prefixes[i];

      if (prefix.second == NULL)
      {
        m_L3Table[prefix.first] = std::make_pair(m_L3Gateway, false);
      }
      else
      {
        m_L3Table[prefix.first] = std::make_pair(prefix.second, false);
      }
    }
  }


  void P4SwitchNetDevice::L3SpecialForwarding(pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    std::unordered_map<uint32_t, std::pair<Ptr<NetDevice>, bool>>::iterator iter = m_L3Table.find(meta.flow.dst_ip);
    if (iter != m_L3Table.end())
    {
      meta.outPort = iter->second.first;
      meta.gray_drop = iter->second.second;
    }
    else
    {
      //std::cout << "Got gateway" <<  m_L3Table.size() << " " << meta.flow.dst_ip << std::endl;
      // set default port. (can this be problematic when packet should not be forwarded???)
      meta.outPort = m_L3Gateway;
      meta.gray_drop = false;
    }
  }

  void P4SwitchNetDevice::L2Learning(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    Ptr<NetDevice> incomingPort = meta.inPort;
    PacketType packetType = meta.packetType;
    uint16_t protocol = meta.protocol;
    Mac48Address src48 = Mac48Address::ConvertFrom(meta.src);
    Mac48Address dst48 = Mac48Address::ConvertFrom(meta.dst);

    switch (packetType)
    {
      // If packet is for me, we learn the mac address, and send it to a highr layer. The higher layer
      // checks the ethernet protocol to decide what to do with this packet.
    case PACKET_HOST:
      if (dst48 == m_address)
      {
        Learn(src48, incomingPort);
        m_rxCallback(this, packet, protocol, meta.src);
      }
      break;

      // If the packet is a broadcast or a multicast we Broadcast it but we also process it 
      // with higher layers, for example an arp request that comes to us will be answered 
      // by the ARP hook (if insitalled...)
    case PACKET_BROADCAST:
    case PACKET_MULTICAST:
      m_rxCallback(this, packet, protocol, meta.src);
      ForwardBroadcast(incomingPort, src48, meta);
      break;

      // If the destination is not us, how do they know it? would be nice to find this. If for some
      // reason the dst mac is us (when would that happen???) we receive the packet and do the learning.
      // Otherwise we do unicast forward, but why? How do we know that forward unicast will work? maybe it calls
      // broadcast inside
    case PACKET_OTHERHOST:
      // The lower level net device might be sending a packet to others, but the others might be our own bridge mac address!
      // Then we take it for us and do not broadcast?
      if (dst48 == m_address)
      {
        Learn(src48, incomingPort);
        m_rxCallback(this, packet, protocol, meta.src);
      }
      else
      {
        ForwardUnicast(incomingPort, packet, protocol, src48, dst48, meta);
      }
      break;
    }
  }

  void
    P4SwitchNetDevice::ForwardUnicast(Ptr<NetDevice> incomingPort, Ptr<const Packet> packet,
      uint16_t protocol, Mac48Address src, Mac48Address dst, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    //NS_LOG_DEBUG ("LearningSwitchForward (incomingPort=" << incomingPort->GetInstanceTypeId ().GetName ()
    //                                                     << ", packet=" << packet << ", protocol="<<protocol
    //                                                     << ", src=" << src << ", dst=" << dst << ")");

    Learn(src, incomingPort);
    Ptr<NetDevice> outPort = GetLearnedState(dst);
    if (outPort != NULL && outPort != incomingPort)
    {
      NS_LOG_LOGIC("Learning switch state says to use port `" << outPort->GetInstanceTypeId().GetName() << "'");
      meta.outPort = outPort;
    }
    else
    {
      NS_LOG_LOGIC("No learned state: send through all ports");
      Broadcast(meta);
    }
  }

  void
    P4SwitchNetDevice::ForwardBroadcast(Ptr<NetDevice> incomingPort, Mac48Address src, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    Learn(src, incomingPort);
    Broadcast(meta);
  }

  void
    P4SwitchNetDevice::Broadcast(pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
    meta.broadcast_flag = true;
  }

  void
    P4SwitchNetDevice::Learn(Mac48Address source, Ptr<NetDevice> port)
  {
    NS_LOG_FUNCTION_NOARGS();
    if (m_enableLearning)
    {
      LearnedState& state = m_learnState[source];
      state.associatedPort = port;
      state.expirationTime = Simulator::Now() + m_expirationTime;
    }
  }

  Ptr<NetDevice>
    P4SwitchNetDevice::GetLearnedState(Mac48Address source)
  {
    NS_LOG_FUNCTION_NOARGS();
    if (m_enableLearning)
    {
      Time now = Simulator::Now();
      std::map<Mac48Address, LearnedState>::iterator iter =
        m_learnState.find(source);
      if (iter != m_learnState.end())
      {
        LearnedState& state = iter->second;
        if (state.expirationTime > now)
        {
          return state.associatedPort;
        }
        else
        {
          m_learnState.erase(iter);
        }
      }
    }
    return NULL;
  }

  uint32_t
    P4SwitchNetDevice::GetNSwitchPorts(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_ports.size();
  }


  Ptr<NetDevice>
    P4SwitchNetDevice::GetSwitchPort(uint32_t n) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_ports[n];
  }

  void
    P4SwitchNetDevice::AddSwitchPort(Ptr<NetDevice> switchPort)
  {
    NS_LOG_FUNCTION_NOARGS();

    NS_ASSERT(switchPort != this);
    if (!Mac48Address::IsMatchingType(switchPort->GetAddress()))
    {
      NS_FATAL_ERROR("Device does not support eui 48 addresses: cannot be added to switch.");
    }
    if (!switchPort->SupportsSendFrom())
    {
      NS_FATAL_ERROR("Device does not support SendFrom: cannot be added to switch.");
    }
    // Becomes the first mac address the first interface
    if (m_address == Mac48Address())
    {
      m_address = Mac48Address::ConvertFrom(switchPort->GetAddress());
    }

    NS_LOG_DEBUG("RegisterProtocolHandler for " << switchPort->GetInstanceTypeId().GetName());
    m_node->RegisterProtocolHandler(MakeCallback(&P4SwitchNetDevice::ReceiveFromDevice, this),
      0, switchPort, true);


    m_ports.push_back(switchPort);
    // Channels are not used at all
    m_channel->AddChannel(switchPort->GetChannel());

    // Start a constant packet Sender and Receiver Processes
    // First get the node in the other side. 
    Ptr<Channel> channel = switchPort->GetChannel();
    Ptr<NetDevice> otherPort = channel->GetDevice(1);
    if (otherPort == switchPort)
    {
      otherPort = channel->GetDevice(0);
    }

    // Prepare port info
    uint32_t ifIndex = switchPort->GetIfIndex();
    PortInfo& portInfo = m_portsInfo[ifIndex];
    portInfo.portDevice = switchPort;
    portInfo.otherPortDevice = otherPort;

    //Ptr<Node> otherSide = portInfo.otherPortDevice->GetNode();
    //std::string otherSideName = Names::FindName(otherSide);
  }

  void
    P4SwitchNetDevice::SetIfIndex(const uint32_t index)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_ifIndex = index;
  }

  uint32_t
    P4SwitchNetDevice::GetIfIndex(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_ifIndex;
  }

  Ptr<Channel>
    P4SwitchNetDevice::GetChannel(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_channel;
  }

  void
    P4SwitchNetDevice::SetAddress(Address address)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_address = Mac48Address::ConvertFrom(address);
  }

  Address
    P4SwitchNetDevice::GetAddress(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_address;
  }

  bool
    P4SwitchNetDevice::SetMtu(const uint16_t mtu)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_mtu = mtu;
    return true;
  }

  uint16_t
    P4SwitchNetDevice::GetMtu(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_mtu;
  }

  void P4SwitchNetDevice::SetSwitchId(uint8_t id)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_switchId = id;
  }

  uint8_t P4SwitchNetDevice::GetSwitchId(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_switchId;
  }

  bool
    P4SwitchNetDevice::IsLinkUp(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return true;
  }


  void
    P4SwitchNetDevice::AddLinkChangeCallback(Callback<void> callback)
  {}


  bool
    P4SwitchNetDevice::IsBroadcast(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return true;
  }


  Address
    P4SwitchNetDevice::GetBroadcast(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return Mac48Address("ff:ff:ff:ff:ff:ff");
  }

  bool
    P4SwitchNetDevice::IsMulticast(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return true;
  }

  Address
    P4SwitchNetDevice::GetMulticast(Ipv4Address multicastGroup) const
  {
    NS_LOG_FUNCTION(this << multicastGroup);
    Mac48Address multicast = Mac48Address::GetMulticast(multicastGroup);
    return multicast;
  }


  bool
    P4SwitchNetDevice::IsPointToPoint(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return false;
  }

  bool
    P4SwitchNetDevice::IsBridge(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return true;
  }

  // This two functions are used by higher layers... Careful cuz they implement a l2 learning algo

  bool
    P4SwitchNetDevice::Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber)
  {
    NS_LOG_FUNCTION_NOARGS();
    return SendFrom(packet, m_address, dest, protocolNumber);
  }

  bool
    P4SwitchNetDevice::SendFrom(Ptr<Packet> packet, const Address& src, const Address& dest, uint16_t protocolNumber)
  {
    NS_LOG_FUNCTION_NOARGS();
    Mac48Address dst = Mac48Address::ConvertFrom(dest);

    // try to use the learned state if data is unicast
    if (!dst.IsGroup())
    {
      Ptr<NetDevice> outPort = GetLearnedState(dst);
      if (outPort != NULL)
      {
        outPort->SendFrom(packet, src, dest, protocolNumber);
        return true;
      }
    }

    // data was not unicast or no state has been learned for that mac
    // address => flood through all ports.
    Ptr<Packet> pktCopy;
    for (std::vector< Ptr<NetDevice> >::iterator iter = m_ports.begin();
      iter != m_ports.end(); iter++)
    {
      pktCopy = packet->Copy();
      Ptr<NetDevice> port = *iter;
      port->SendFrom(pktCopy, src, dest, protocolNumber);
    }

    return true;
  }


  Ptr<Node>
    P4SwitchNetDevice::GetNode(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return m_node;
  }


  void
    P4SwitchNetDevice::SetNode(Ptr<Node> node)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_node = node;
  }


  bool
    P4SwitchNetDevice::NeedsArp(void) const
  {
    NS_LOG_FUNCTION_NOARGS();
    return true;
  }


  void
    P4SwitchNetDevice::SetReceiveCallback(NetDevice::ReceiveCallback cb)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_rxCallback = cb;
  }

  void
    P4SwitchNetDevice::SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback cb)
  {
    NS_LOG_FUNCTION_NOARGS();
    m_promiscRxCallback = cb;
  }

  bool
    P4SwitchNetDevice::SupportsSendFrom() const
  {
    NS_LOG_FUNCTION_NOARGS();
    return true;
  }

  Address P4SwitchNetDevice::GetMulticast(Ipv6Address addr) const
  {
    NS_LOG_FUNCTION(this << addr);
    return Mac48Address::GetMulticast(addr);
  }

} // namespace ns3
