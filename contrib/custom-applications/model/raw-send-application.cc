/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "raw-send-application.h"
#include "ns3/utils-module.h"
#include "ns3/p4-switch-utils.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("raw-send-app");

NS_OBJECT_ENSURE_REGISTERED (RawSendApplication);

TypeId
RawSendApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RawSendApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications")
    .AddConstructor<RawSendApplication> ()
    .AddAttribute ("PacketSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&RawSendApplication::m_packetSize),
                   MakeUintegerChecker<uint32_t> ())
  .AddAttribute ("SendRate", "The amount of bites per second to send.",
                    DataRateValue (DataRate("900Mbps")),
                    MakeDataRateAccessor (&RawSendApplication::m_sendRate),
                    MakeDataRateChecker())                 
  .AddAttribute("Duration",
									"Time to keep the traffic rate",
									DoubleValue(2),
									MakeDoubleAccessor(&RawSendApplication::m_duration),                    
                  MakeDoubleChecker<double> ())
  .AddAttribute ("DstAddr", "The address of the destination",
                 AddressValue (AddressValue(Mac48Address("00:00:00:00:00:ff"))),
                 MakeAddressAccessor (&RawSendApplication::m_dstAddr),
                 MakeAddressChecker ())
  .AddAttribute ("NumFlows", "The amount of flows to send each time.",
                  UintegerValue (1),
                  MakeUintegerAccessor (&RawSendApplication::m_nFlows),
                  MakeUintegerChecker<uint32_t> ())
  .AddAttribute ("NumDrops", "The amount of flows to drop.",
                  UintegerValue (1),
                  MakeUintegerAccessor (&RawSendApplication::m_dropFlows),
                  MakeUintegerChecker<uint32_t> ())
  .AddAttribute ("OutFile", "Saves the folows that were sent",
                  StringValue (""),
                  MakeStringAccessor(&RawSendApplication::m_outFile),
                  MakeStringChecker())
  ;
  return tid;
}

Ptr<Packet> RawSendApplication::CreateTcpSynPacket(uint32_t dstIp, uint16_t srcPort, uint16_t dstPort, uint8_t tos)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  TcpHeader tcp_header;
  Ipv4Header ipv4_header;

  // Set IPV4 fields
  ipv4_header.SetProtocol(6);
  ipv4_header.SetTtl(64);
  ipv4_header.SetIdentification(m_nPackets & 0xffff);
  ipv4_header.SetTos(tos);
  ipv4_header.SetPayloadSize(44);
  ipv4_header.SetDestination(Ipv4Address(dstIp));

  // Set TCP fields
  tcp_header.SetSourcePort(srcPort);
  tcp_header.SetDestinationPort(dstPort);
  tcp_header.SetFlags(TcpHeader::Flags_t::SYN);
  
  packet->AddHeader(tcp_header);
  ipv4_header.SetPayloadSize(packet->GetSize ());
  packet->AddHeader(ipv4_header);

  return packet;
}

RawSendApplication::RawSendApplication ()
  :
  m_packetIndex (0),
  m_device (0),
  m_packets (NULL)
{
  NS_LOG_FUNCTION (this);
}

RawSendApplication::~RawSendApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
RawSendApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  CleanPackets ();
  m_device = 0;

  // chain up
  Application::DoDispose ();
}

void RawSendApplication::CleanPackets ()
{
  if (m_packets != NULL)
  {
    delete[] m_packets; // no idea why
    m_packets = NULL;
  }  
  //m_packets.clear ();
}
void RawSendApplication::AllocatePackets ()
{
  CleanPackets ();
  m_packets = new Ptr<Packet>[m_nFlows];
  //NS_LOG_UNCOND(m_packets);
}

void RawSendApplication::SaveFlows (){
  if (m_outFile != "")
  {
    if (m_packets != NULL)
    {
      uint32_t tree_depth = 3;
      uint32_t num_hashes = 3;
      uint32_t bloom_hash_size = 10000;
      HashUtils h = HashUtils(tree_depth*2);
      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> file = asciiTraceHelper.CreateFileStream(m_outFile);
      for (int index_tos = 0; index_tos < 100; index_tos++)
      {
        bool found = false;
        for (int flow_id = 0; flow_id < m_nFlows; flow_id++)
        {
          Ptr<Packet> pkt = (*(m_packets+flow_id))->Copy();

          Ipv4Header ipv4_header;
          pkt->RemoveHeader(ipv4_header);

          Ipv4Address src_ip = ipv4_header.GetSource();
          Ipv4Address dst_ip = ipv4_header.GetDestination();
          uint8_t protocol = ipv4_header.GetProtocol();
          uint16_t src_port = 0;
          uint16_t dst_port =0;
          uint8_t tos = ipv4_header.GetTos();
          /* We just save the flows with that specific tos so we can split them*/
          if (tos != index_tos)
          {
            continue;
          }
          if (protocol == 6)
          {
            TcpHeader tcp_header;
            pkt->RemoveHeader(tcp_header);
            src_port = tcp_header.GetSourcePort();
            dst_port = tcp_header.GetDestinationPort();
          }
          else if (protocol == 17)
          {
            UdpHeader udp_header;
            pkt->RemoveHeader(udp_header);
            src_port = udp_header.GetSourcePort();
            dst_port = udp_header.GetDestinationPort();
          }
          if (!found)
          {
            *(file->GetStream()) << "# TOS = "<< int(index_tos) << "\n";
            found=true;
          }

          // Get hash values
          char s[13];
          char *ptr = s;
          ip_five_tuple five_tuple;
          five_tuple.src_ip = src_ip.Get();
          five_tuple.dst_ip = dst_ip.Get();
          five_tuple.src_port = src_port;
          five_tuple.dst_port = dst_port;
          five_tuple.protocol = protocol;
          IpFiveTupleToBuffer(ptr, five_tuple);

          char t[tree_depth];
          char * ptr1 = t;

          *(file->GetStream()) << "Flow: " << src_ip<< " " << dst_ip << " " << src_port << " " << dst_port << " " << int(protocol) << " Path: ";

          for (int i=0; i < tree_depth; i++)
          {
            uint8_t out = h.GetHash(s, 13, i, 16);
            *(file->GetStream()) << int(out) << " ";
            std::memcpy(ptr1+(i), &out, 1);
          }
          
          *(file->GetStream()) << " Path Hash: ";
          for (int i= 0; i < num_hashes; i++)
          {
             *(file->GetStream()) << h.GetHash(t, tree_depth, i, bloom_hash_size) << " ";
          }
          *(file->GetStream()) << "\n";
        }
        //*(file->GetStream()) << "\n";
      }
      *(file->GetStream()) << "\n";
      file->GetStream()->flush();
    }
  }
}

void RawSendApplication::RandomInitializePackets (bool randomize_dst_ip)
{
  NS_ASSERT(m_nFlows >= m_dropFlows);
  Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable>();
  random_generator->SetStream(1);
  uint32_t dropped = 0;
  uint8_t tos = 0;

  uint32_t default_dst = Ipv4Address("102.102.102.102").Get();

  for (int n = 0; n < m_nFlows; n++)
  {
    tos = 0;
    if (dropped < m_dropFlows)
    {
      tos = 1;
      dropped++;
    }

    uint32_t dst = default_dst;
    if (randomize_dst_ip)
    {
      // Random prefix 
      dst = (random_generator->GetInteger(1, 16777215) << 8) + 1;
    }    

    *(m_packets+n) = CreateTcpSynPacket(dst, random_generator->GetInteger(0, 65000), random_generator->GetInteger(0, 65000), tos);
  }

  /* Shuffle */
  std::random_device rd;
  std::mt19937 g(rd());
  g.seed(1);
  std::shuffle(m_packets, m_packets+(m_nFlows-1), g);
}

void RawSendApplication::StartApplication (void) 
{
  NS_LOG_FUNCTION (this);

  if (m_nFlows == 0)
  {
    StopApplication();
    return;
  }

  /* Set local net device */
  m_device = GetNodeNetDevice (GetNode ());

  /* Computes the amount of packets to send */
  m_nPackets = (m_sendRate.GetBitRate ()/ (8*m_packetSize)) * m_duration;
  NS_LOG_UNCOND("Packets to be sent: " << m_nPackets);
  NS_LOG_UNCOND("Packets per second and flow: " << m_nPackets/(m_duration*m_nFlows));

  /* Create packets to be sent */
  AllocatePackets ();

  /* Initialize the packet vector */
  RandomInitializePackets (true);
  SaveFlows();
  m_nextTx = m_sendRate.CalculateBytesTxTime(m_packets[m_packetIndex]->GetSize ());
  SendPackets ();
}

void RawSendApplication::SendPackets(void)
{
  m_device->SendFrom((*(m_packets+m_packetIndex))->Copy(), m_device->GetAddress(), m_dstAddr, 0x0800);
  m_nPackets--;
  if (m_nPackets == 0)
  {
    return;
  }
  
  m_packetIndex = (m_packetIndex + 1) % m_nFlows;
  m_sendEvent = Simulator::Schedule(m_nextTx, &RawSendApplication::SendPackets, this);

  /* reset packets for new Ids*/
  if (m_packetIndex == 0)
  {
    RandomInitializePackets (true);
  }

}

void RawSendApplication::StopApplication (void) 
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel(m_sendEvent);
  DoDispose ();
}


} // Namespace ns3
