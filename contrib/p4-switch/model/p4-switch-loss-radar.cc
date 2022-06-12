/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*«
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
#include "p4-switch-loss-radar.h"
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

  NS_LOG_COMPONENT_DEFINE("P4SwitchLossRadar");

  NS_OBJECT_ENSURE_REGISTERED(P4SwitchLossRadar);

#define IPV4 0x0800
#define ARP  0x0806
#define IPV6 0x86DD
#define TCP  6
#define UDP  17

  TypeId
    P4SwitchLossRadar::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::P4SwitchLossRadar")
      .SetParent<P4SwitchNetDevice>()
      .SetGroupName("P4-Switch")
      .AddConstructor<P4SwitchLossRadar>()
      .AddAttribute("NumberBatches", "Number of batches we measure.",
        UintegerValue(2),
        MakeUintegerAccessor(&P4SwitchLossRadar::m_numBatches),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("NumberOfCells", "Number of cells per port and batch",
        UintegerValue(2048),
        MakeUintegerAccessor(&P4SwitchLossRadar::m_numCells),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("NumberHashes", "Number of hash functions per packet",
        UintegerValue(3),
        MakeUintegerAccessor(&P4SwitchLossRadar::m_numHashes),
        MakeUintegerChecker<uint16_t>())
      .AddAttribute("TmDropRate", "Percentage of traffic that gets dropped by the traffic manager.",
        DoubleValue(0),
        MakeDoubleAccessor(&P4SwitchLossRadar::m_tm_drop_rate),
        MakeDoubleChecker<double>(0, 1))
      .AddAttribute("FailDropRate", "Percentage of traffic that gets dropped when a prefix is affected.",
        DoubleValue(0),
        MakeDoubleAccessor(&P4SwitchLossRadar::m_fail_drop_rate),
        MakeDoubleChecker<double>(0, 1))
      .AddAttribute("BatchTime", "Batch frequency update in ms",
        DoubleValue(100),
        MakeDoubleAccessor(&P4SwitchLossRadar::m_batchTimeMs),
        MakeDoubleChecker<double>())
      .AddAttribute("OutFile", "Name of the output file for the data generated.",
        StringValue(""),
        MakeStringAccessor(&P4SwitchLossRadar::m_outFile),
        MakeStringChecker())
      ;
    return tid;
  }

  P4SwitchLossRadar::P4SwitchLossRadar()
    :
    header_stack_reverse(header_stack)
  {
    std::reverse(header_stack_reverse.begin(), header_stack_reverse.end());
  }

  P4SwitchLossRadar::~P4SwitchLossRadar()
  {
    NS_LOG_FUNCTION_NOARGS();
    if (m_hash)
    {
      m_hash->Clean();
    }

    if (m_outFile != "")
    {
      m_simState->SaveInJson(m_outFile);
      // save packet drops
      if (m_drop_times.size() > 0)
      {
        std::string drops_file = m_outFile;
        drops_file.replace(drops_file.find(".json"), 5, ".drops");
        SaveDrops(drops_file);
      }
    }
  }

  LossRadarPortInfo&
    P4SwitchLossRadar::GetPortInfo(uint32_t i)
  {
    return m_portsInfo[i];
  }

  /* Hash functions */

  void
    P4SwitchLossRadar::GetMeterHashIndexes(ip_five_tuple& flow, uint32_t hash_indexes[])
  {
    // build five tuple buffer
    char s[15];
    IpFiveTupleWithIdToBuffer(s, flow);

    for (uint32_t i = 0; i < m_numHashes; i++)
    {
      hash_indexes[i] = m_hash->GetHash(s, 15, i, m_numCells);
    }
  }

  void
    P4SwitchLossRadar::Controller(Ptr<NetDevice> port, uint8_t batch_id)
  {
    NS_LOG_FUNCTION_NOARGS();

    /* Get registers */

    LossRadarPortInfo& portInfo = m_portsInfo[port->GetIfIndex()];

    NS_LOG_DEBUG("Link: " << portInfo.link_name << " batch id: " << uint32_t(batch_id));
    Ptr<Node> otherNode = portInfo.otherPortDevice->GetNode();

    /* Find device id */
    uint32_t device_id = 0;
    for (uint32_t i = 0; i < otherNode->GetNDevices(); i++)
    {
      if (DynamicCast<P4SwitchLossRadar>(otherNode->GetDevice(i)))
      {
        device_id = i;
      }
    }

    LossRadarPortInfo& otherPortInfo = (DynamicCast<P4SwitchLossRadar>(otherNode->GetDevice(device_id)))->GetPortInfo(portInfo.otherPortDevice->GetIfIndex());

    /* Do register difference */
    for (uint32_t i = 0; i < m_numCells; i++)
    {
      portInfo.um_info[batch_id][i].pkt_counter -= otherPortInfo.dm_info[batch_id][i].pkt_counter;
      portInfo.um_info[batch_id][i].src_ip ^= otherPortInfo.dm_info[batch_id][i].src_ip;
      portInfo.um_info[batch_id][i].dst_ip ^= otherPortInfo.dm_info[batch_id][i].dst_ip;
      portInfo.um_info[batch_id][i].id ^= otherPortInfo.dm_info[batch_id][i].id;
      portInfo.um_info[batch_id][i].src_port ^= otherPortInfo.dm_info[batch_id][i].src_port;
      portInfo.um_info[batch_id][i].dst_port ^= otherPortInfo.dm_info[batch_id][i].dst_port;
      portInfo.um_info[batch_id][i].protocol ^= otherPortInfo.dm_info[batch_id][i].protocol;
    }

    /* count total packets in the filter */
    uint32_t total_packets_lost_in_batch = 0;
    for (uint32_t i = 0; i < m_numCells; i++)
    {
      total_packets_lost_in_batch += portInfo.um_info[batch_id][i].pkt_counter;
    }

    /* Compute packet losses */
    std::vector<ip_five_tuple> packets_lost;

    /* Report packet losses */
    bool converged = false;
    bool step = false;
    while (not converged)
    {
      step = false;
      for (uint32_t i = 0; i < m_numCells; i++)
      {
        if (portInfo.um_info[batch_id][i].pkt_counter == 1)
        {
          step = true;

          /* get flow from the pure cell*/
          ip_five_tuple flow;
          flow.src_ip = portInfo.um_info[batch_id][i].src_ip;
          flow.dst_ip = portInfo.um_info[batch_id][i].dst_ip;
          flow.id = portInfo.um_info[batch_id][i].id;
          flow.src_port = portInfo.um_info[batch_id][i].src_port;
          flow.dst_port = portInfo.um_info[batch_id][i].dst_port;
          flow.protocol = portInfo.um_info[batch_id][i].protocol;

          /* Add packet to drops list */
          packets_lost.push_back(flow);

          /* get hashes for the flow in the pure cell*/
          uint32_t meter_hash_indexes[m_numHashes];
          GetMeterHashIndexes(flow, meter_hash_indexes);

          /* Remove from the other cells */
          for (int j = 0; j < m_numHashes; j++)
          {
            // What happens if we go negative here?
            portInfo.um_info[batch_id][meter_hash_indexes[j]].pkt_counter -= 1;
            portInfo.um_info[batch_id][meter_hash_indexes[j]].src_ip ^= flow.src_ip;
            portInfo.um_info[batch_id][meter_hash_indexes[j]].dst_ip ^= flow.dst_ip;
            portInfo.um_info[batch_id][meter_hash_indexes[j]].id ^= flow.id;
            portInfo.um_info[batch_id][meter_hash_indexes[j]].src_port ^= flow.src_port;
            portInfo.um_info[batch_id][meter_hash_indexes[j]].dst_port ^= flow.dst_port;
            portInfo.um_info[batch_id][meter_hash_indexes[j]].protocol ^= flow.protocol;
          }
        }
      }
      if (not step)
      {
        converged = true;
      }


    }

    /* Do register difference */
    NS_LOG_DEBUG("Controller Info: (" << portInfo.link_name << ")");

    /* Count how many non pure cells */
    uint32_t non_pure_cells = 0;
    uint32_t non_detected_packets = 0;
    for (uint32_t i = 0; i < m_numCells; i++)
    {
      if (portInfo.um_info[batch_id][i].pkt_counter > 0)
      {
        non_detected_packets += portInfo.um_info[batch_id][i].pkt_counter;
        non_pure_cells++;
      }
    }


    /* log decoding */
    if (packets_lost.size() > 0 || non_detected_packets > 0)
    {
      /*
      * Non detected packets: the number of packets that have been lost but not decoded
      * Non pure cells: the number of cells that could not be decoded. Meaning 2 or more packets were left.
      */

      NS_LOG_DEBUG("Non pure cells: " << non_pure_cells);
      NS_LOG_DEBUG("Packets lost non decodable: " << non_detected_packets / m_numHashes);
      /* why approximated? Well it can be that if we have multiple packets for the
      same flow (or even with id) then they are not detected*/
      NS_LOG_DEBUG("Packets lost decodable: " << packets_lost.size());

      /* Print lost packets */
      //for (auto it = packets_lost.begin(); it != packets_lost.end(); ++it)
      //{
      //  PrintIpFiveTuple(*it);
      //}

      m_simState->SetFailureEvent(portInfo.link_name, Simulator::Now().GetSeconds(), packets_lost, non_pure_cells, non_detected_packets / m_numHashes, total_packets_lost_in_batch / m_numHashes);
    }

    /* Clear registers */
    std::fill(portInfo.um_info[batch_id].begin(), portInfo.um_info[batch_id].end(), LossRadarPacketInfo());
    std::fill(otherPortInfo.dm_info[batch_id].begin(), otherPortInfo.dm_info[batch_id].end(), LossRadarPacketInfo());
  }


  void
    P4SwitchLossRadar::UpdateBatchId(Ptr<NetDevice> port, const Time& delay)
  {
    NS_LOG_FUNCTION_NOARGS();

    LossRadarPortInfo& portInfo = m_portsInfo[port->GetIfIndex()];

    /* changes the batch ID every some time */
    NS_LOG_DEBUG("UPDATE BATCH ID");
    //NS_LOG_DEBUG("Address of batch ID : " <<  &portInfo);
    uint8_t previous_batch_id = portInfo.current_batch_id;
    NS_LOG_DEBUG("Link: " << portInfo.link_name << " previous batch id: " << uint32_t(previous_batch_id));
    portInfo.current_batch_id = (portInfo.current_batch_id + 1) % m_numBatches;
    NS_LOG_DEBUG("Link: " << portInfo.link_name << " new batch id: " << uint32_t(portInfo.current_batch_id));

    /* Auto reschedule it again */
    Simulator::Schedule(delay, &P4SwitchLossRadar::UpdateBatchId, this, port, delay);

    // Schedule Controller to read the previous batch in delay/2
    // We assume that in that time packets should be for sure at the downstream 
    // TODO: If we start playing with delay we might have to change how this is done
    Simulator::Schedule(delay / 2, &P4SwitchLossRadar::Controller, this, port, previous_batch_id);
  }

  void
    P4SwitchLossRadar::AddSwitchPort(Ptr<NetDevice> switchPort)
  {
    NS_LOG_FUNCTION_NOARGS();

    P4SwitchNetDevice::AddSwitchPort(switchPort);

    uint32_t ifIndex = switchPort->GetIfIndex();
    LossRadarPortInfo& portInfo = m_portsInfo[ifIndex];

    NS_LOG_DEBUG("Address of original port info: " << &portInfo);

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
    P4SwitchLossRadar::SetDebug(bool state)
  {
    m_enableDebug = state;
  }

  void
    P4SwitchLossRadar::Init()
  {
    // Forwarding table is set here
    P4SwitchNetDevice::Init();

    /* allocates the simulation state object */
    m_simState = std::make_unique<LossRadarSimulationState>();

    /* Sets tm drop error model */
    tm_em->SetAttribute("ErrorRate", DoubleValue(m_tm_drop_rate));
    tm_em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    /* Sets tm drop error model */
    fail_em->SetAttribute("ErrorRate", DoubleValue(m_fail_drop_rate));
    fail_em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    /* set the number of hashes we need for the algorithm */
    m_hash = std::make_unique<HashUtils>(m_numHashes);

    /* Allocate port structure memories */
    for (uint32_t i = 0; i < m_ports.size(); i++)
    {
      uint32_t ifIndex = m_ports[i]->GetIfIndex();
      Ptr<NetDevice> switchPort = m_ports[i];
      LossRadarPortInfo& portInfo = m_portsInfo[ifIndex];
      //NS_LOG_DEBUG("Address of port info during init : " <<  &portInfo);

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
      // CAREFUL WITH THIS, if any of my nodes has an s all breaks
      if (otherSideName.find("s") != std::string::npos)
      {
        portInfo.switchPort = true;
        /* Start the batch id update process */
        //UpdateBatchId(switchPort, MilliSeconds(m_batchTimeMs));
        Simulator::Schedule(MilliSeconds(m_batchTimeMs), &P4SwitchLossRadar::UpdateBatchId, this, switchPort, MilliSeconds(m_batchTimeMs));
      }
    }
  }

  void
    P4SwitchLossRadar::InitPortInfo(LossRadarPortInfo& portInfo)
  {

    portInfo.current_batch_id = 0;
    portInfo.um_info = std::vector<std::vector<LossRadarPacketInfo>>(m_numBatches);
    for (int i = 0; i < m_numBatches; i++)
    {
      portInfo.um_info[i] = std::vector<LossRadarPacketInfo>(m_numCells);
    }
    portInfo.dm_info = std::vector<std::vector<LossRadarPacketInfo>>(m_numBatches);
    for (int i = 0; i < m_numBatches; i++)
    {
      portInfo.dm_info[i] = std::vector<LossRadarPacketInfo>(m_numCells);
    }

    //NS_LOG_DEBUG("Address of port info during init port info : " <<  &portInfo);

  }

  void P4SwitchLossRadar::DoParser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
  {
    NS_LOG_FUNCTION_NOARGS();

    switch (protocol)
    {
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

  void P4SwitchLossRadar::ParseArp(Ptr<Packet> packet, pkt_info& meta)
  {
    ArpHeader arp_hdr;
    packet->RemoveHeader(arp_hdr);
    meta.headers["ARP"] = arp_hdr;
  }

  void P4SwitchLossRadar::ParseIpv4(Ptr<Packet> packet, pkt_info& meta)
  {
    Ipv4Header ipv4_hdr;
    packet->RemoveHeader(ipv4_hdr);
    meta.headers["IPV4"] = ipv4_hdr;
    ParseTransport(packet, meta, ipv4_hdr.GetProtocol());
  }

  void P4SwitchLossRadar::ParseTransport(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
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

  void P4SwitchLossRadar::ParseIpv6(Ptr<Packet> packet, pkt_info& meta)
  {

  }

  void
    P4SwitchLossRadar::DoVerifyChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void
    P4SwitchLossRadar::DoIngress(Ptr<const Packet> packet, pkt_info& meta)
  {

    NS_LOG_FUNCTION_NOARGS();
    m_pkt_counter1 = m_pkt_counter1 + 1;

    // Set transport ports such that udp and tcp are unified
    meta.flow = GetFlowFiveTuple(meta);

    uint32_t meter_hash_indexes[m_numHashes];
    GetMeterHashIndexes(meta.flow, meter_hash_indexes);

    // Do Normal forwarding
    if (m_forwardingType == P4SwitchNetDevice::ForwardingType::PORT_FORWARDING)
    {
      PortForwarding(meta);
    }
    else if (m_forwardingType == P4SwitchNetDevice::ForwardingType::L3_SPECIAL_FORWARDING)
    {
      L3SpecialForwarding(meta);
    }

    // Only do the entire thing for IP packets 
    // We do this such that we can use some IP header fields
    // and we do not have to add any new header. 
    // Also this means we only detect IPV4 packets, but its fine
    if (meta.headers.count("IPV4") > 0) {

      LossRadarPortInfo& inPortInfo = m_portsInfo[meta.inPort->GetIfIndex()];

      /* Get previous switch batch id */
      Ipv4Header& ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
      uint8_t tos = ipv4_hdr.GetTos();
      uint8_t previous_batch_id = getTosHi(tos);

      /* Decrease TTL */
      ipv4_hdr.SetTtl(ipv4_hdr.GetTtl() - 1);

      /* Does packet go to a host or switch ? */
      if (meta.outPort != NULL)
      {
        LossRadarPortInfo& outPortInfo = m_portsInfo[meta.outPort->GetIfIndex()];
        if (outPortInfo.switchPort) {
          uint8_t batch_id = outPortInfo.current_batch_id;

          UmMeter(outPortInfo, meta.flow, meter_hash_indexes, batch_id);
          /* Set tos field for the outgoing packet */
          ipv4_hdr.SetTos(setTosHi(tos, batch_id));
        }
      }

      /* Does packet come from a host or switch */
      if (inPortInfo.switchPort) {
        DmMeter(inPortInfo, meta.flow, meter_hash_indexes, previous_batch_id);
      }
    }

  }

  void
    P4SwitchLossRadar::DoDispose()
  {
    NS_LOG_FUNCTION_NOARGS();
    std::cout << Names::FindName(m_node) << " Packets 1 received " << m_pkt_counter1 << std::endl;
    std::cout << Names::FindName(m_node) << " Packets 2 received " << m_pkt_counter2 << std::endl;
    P4SwitchNetDevice::DoDispose();
  }

  void
    P4SwitchLossRadar::DoTrafficManager(Ptr<const Packet> packet, pkt_info& meta)
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
        //ip_five_tuple flow = GetFlowFiveTuple(meta);
        //std::cout << "PACKET DROPPED AT (" << Names::FindName(m_node) << ")" << std::endl;
        //PrintFiveTuple(flow);
        return;
      }
    }

    Egress(packet, meta);
  }


  void
    P4SwitchLossRadar::DoEgress(Ptr<const Packet> packet, pkt_info& meta)
  {

    NS_LOG_FUNCTION_NOARGS();

  }

  void
    P4SwitchLossRadar::DoUpdateChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void
    P4SwitchLossRadar::DoDeparser(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    // Deparse valid headers if there are headers to deparse
    if (meta.headers.size() > 0)
    {
      for (auto hdr_type : header_stack_reverse)
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
    P4SwitchLossRadar::DoEgressTrafficManager(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    // To remove 

    if (meta.headers.count("IPV4") > 0)
    {
      // drop packet at the egress
      Ipv4Header& ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
      /* gets 4 lsb */
      uint8_t lo_tos = getTosLo(ipv4_hdr.GetTos());

      /* Here is where we decide if a packet has to be dropped or not
         as an easy way our sender the TOS field to a number, if that number
         is this switch ID then we drop the packet with "m_fail_drop_rate" probability
      */

      if (lo_tos == m_switchId || meta.gray_drop)
      {
        /* only normal traffic, we do not drop good traffic */
        //Ptr<Packet> pkt = packet->Copy();
        /* drop packet */

        /* I believe the == 1 is to do it faster*/
        if (m_fail_drop_rate == 1 || fail_em->IsCorrupt(packet))
        {
          m_drop_times.push_back(Simulator::Now());
          return;
        }
      }
    }

    // To remove 
    m_pkt_counter2 = m_pkt_counter2 + 1;

    // Before the packet leaves the switch
    // Update Send Timestamp
    LossRadarPortInfo& outPortInfo = m_portsInfo[meta.outPort->GetIfIndex()];
    outPortInfo.last_time_sent = Simulator::Now();
    /* Removed the sense of l2 swtich we had before, now for each packet we modify the mac address, useful to track what happens */
    SendOutFrom(packet, meta.outPort, meta.outPort->GetAddress(), outPortInfo.otherPortDevice->GetAddress(), meta.protocol);
    //SendOutFrom(packet, meta.outPort, meta.src, meta.dst, meta.protocol);
  }

  void
    P4SwitchLossRadar::UmMeter(LossRadarPortInfo& portInfo, ip_five_tuple& flow, uint32_t hash_indexes[], uint8_t batch_id)
  {
    NS_LOG_FUNCTION_NOARGS();

    /* XORs and increments counter */
    for (uint32_t i = 0; i < m_numHashes; i++)
    {
      portInfo.um_info[batch_id][hash_indexes[i]].src_ip ^= flow.src_ip;
      portInfo.um_info[batch_id][hash_indexes[i]].dst_ip ^= flow.dst_ip;
      portInfo.um_info[batch_id][hash_indexes[i]].id ^= flow.id;
      portInfo.um_info[batch_id][hash_indexes[i]].src_port ^= flow.src_port;
      portInfo.um_info[batch_id][hash_indexes[i]].dst_port ^= flow.dst_port;
      portInfo.um_info[batch_id][hash_indexes[i]].protocol ^= flow.protocol;
      portInfo.um_info[batch_id][hash_indexes[i]].pkt_counter++;
    }
  }

  void
    P4SwitchLossRadar::DmMeter(LossRadarPortInfo& portInfo, ip_five_tuple& flow, uint32_t hash_indexes[], uint8_t batch_id)
  {
    NS_LOG_FUNCTION_NOARGS();

    /* XORs and increments counter */
    for (uint32_t i = 0; i < m_numHashes; i++)
    {
      portInfo.dm_info[batch_id][hash_indexes[i]].src_ip ^= flow.src_ip;
      portInfo.dm_info[batch_id][hash_indexes[i]].dst_ip ^= flow.dst_ip;
      portInfo.dm_info[batch_id][hash_indexes[i]].id ^= flow.id;
      portInfo.dm_info[batch_id][hash_indexes[i]].src_port ^= flow.src_port;
      portInfo.dm_info[batch_id][hash_indexes[i]].dst_port ^= flow.dst_port;
      portInfo.dm_info[batch_id][hash_indexes[i]].protocol ^= flow.protocol;
      portInfo.dm_info[batch_id][hash_indexes[i]].pkt_counter++;
    }
  }

} // namespace ns3
