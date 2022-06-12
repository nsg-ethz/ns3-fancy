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
#include "trace-send-application.h"
#include "ns3/utils-module.h"
#include "ns3/p4-switch-utils.h"
#include "ns3/ipv4-address.h"

#include <iostream>
#include <fstream>

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("trace-send-app");

NS_OBJECT_ENSURE_REGISTERED (TraceSendApplication);

TypeId
TraceSendApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::TraceSendApplication")
  .SetParent<Application> ()
  .SetGroupName("Applications")
  .AddConstructor<TraceSendApplication> ()
  .AddAttribute ("AvgPacketSize", "The amount of data to send each time.",
                  UintegerValue (512),
                  MakeUintegerAccessor (&TraceSendApplication::m_packetSize),
                  MakeUintegerChecker<uint32_t> ())              
  .AddAttribute("Duration",
									"Used to stop the generartion earlier",
									DoubleValue(2),
									MakeDoubleAccessor(&TraceSendApplication::m_duration),                    
                  MakeDoubleChecker<double> ())
  .AddAttribute ("DstAddr", "The address of the destination",
                 AddressValue (AddressValue(Mac48Address("00:00:00:00:00:ff"))),
                 MakeAddressAccessor (&TraceSendApplication::m_dstAddr),
                 MakeAddressChecker ())
  .AddAttribute ("NumTopEntries", "Where to put the threshold of big and small prefixes",
                  UintegerValue (0),
                  MakeUintegerAccessor (&TraceSendApplication::m_numTopEntries),
                  MakeUintegerChecker<uint32_t> ())
  .AddAttribute ("TopFailType", "Type of drop: total, random, top-down.",
                  StringValue ("TopDown"),
                  MakeStringAccessor(&TraceSendApplication::m_topFailType),
                  MakeStringChecker())
  .AddAttribute ("NumTopFails", "The amount of top flows to drop.",
                  UintegerValue (0),
                  MakeUintegerAccessor (&TraceSendApplication::m_topDrops),
                  MakeUintegerChecker<uint32_t> ())        
  .AddAttribute ("BottomFailType", "Type of drop: total, random, top-down.",
                  StringValue ("TopDown"),
                  MakeStringAccessor(&TraceSendApplication::m_bottomFailType),
                  MakeStringChecker())
  .AddAttribute ("NumBottomFails", "The amount of top flows to drop.",
                  UintegerValue (0),
                  MakeUintegerAccessor (&TraceSendApplication::m_bottomDrops),
                  MakeUintegerChecker<uint32_t> ())                              
  .AddAttribute("Scaling",
									"Bandwidth Scaling you can devide to send faster or multiple to send slower ",
									StringValue("x1"),
									MakeStringAccessor(&TraceSendApplication::m_scaling),                    
                  MakeStringChecker ())                
  .AddAttribute ("InFile", "Binary file with the packets to send",
                  StringValue (""),
                  MakeStringAccessor(&TraceSendApplication::m_inFile),
                  MakeStringChecker())    
  .AddAttribute ("TopFile", "File with all the slice top prefixes",
                StringValue (""),
                MakeStringAccessor(&TraceSendApplication::m_topFile),
                MakeStringChecker())
  .AddAttribute ("AllowedToFail", "File with a list of all the prefixes that are allowed to be failed",
                StringValue (""),
                MakeStringAccessor(&TraceSendApplication::m_allowedToFailFile),
                MakeStringChecker())                                 
  .AddAttribute ("OutFile", "Saves the folows that were sent",
                  StringValue (""),
                  MakeStringAccessor(&TraceSendApplication::m_outFile),
                  MakeStringChecker())
  ;
  return tid;
}

Ptr<Packet> TraceSendApplication::CreateTcpSynPacket(uint16_t srcPort, uint16_t dstPort, 
                                                     uint16_t packet_size, uint32_t dst_addr, uint8_t tos)
{
  
  Ptr<Packet> packet = Create<Packet>(std::max(0, packet_size-58));
  TcpHeader tcp_header;
  Ipv4Header ipv4_header;

  // Set IPV4 fields
  ipv4_header.SetProtocol(6);
  ipv4_header.SetTtl(64);
  ipv4_header.SetIdentification(m_sentPackets & 0xFFFF); // in theory same than % 65536
  ipv4_header.SetTos(tos);
  ipv4_header.SetPayloadSize(44);
  ipv4_header.SetDestination(Ipv4Address(dst_addr));

  // Set TCP fields
  tcp_header.SetSourcePort(srcPort);
  tcp_header.SetDestinationPort(dstPort);
  tcp_header.SetFlags(TcpHeader::Flags_t::SYN);
  /* To remove */
  //tcp_header.SetSequenceNumber(ns3::SequenceNumber32(m_sentPackets));
  
  packet->AddHeader(tcp_header);
  ipv4_header.SetPayloadSize(packet->GetSize ());
  packet->AddHeader(ipv4_header);

  return packet;
}

TraceSendApplication::TraceSendApplication ()
  :
  m_packetIndex (0),
  m_device (0)
{
  NS_LOG_FUNCTION (this);
}

TraceSendApplication::~TraceSendApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
TraceSendApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  CleanPackets ();
  m_device = 0;

  // chain up
  Application::DoDispose ();
}

void TraceSendApplication::CleanPackets ()
{
   /* void */
}

void TraceSendApplication::SaveFailedPrefixes ()
{
 if (m_outFile != "")
 {
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> file = asciiTraceHelper.CreateFileStream(m_outFile);

    /* Done like this to keep order */
    for (int i = 0; i < top_prefixes.size(); i++)
    {
      if (prefixes_to_fail.count(top_prefixes[i]) > 0)
      {
        Ipv4Address address(top_prefixes[i]);
        bool top = i < m_numTopEntries;
        uint32_t top_down;
        if (top)
        {
          top_down = 1;
        }
        else
        {
          top_down = 0;
        }
        *(file->GetStream()) <<  address << " "  << top_down << "\n";
      }
    }
  file->GetStream()->flush();
 }
}

fail_types TraceSendApplication::GetFailType(std::string const& str_type) {
  if (str_type == "TopDown") return TopDown;
  if (str_type == "Random") return Random;
  if (str_type == "All") return All;

  return Unknown;
}

void TraceSendApplication::StartApplication (void) 
{
  NS_LOG_FUNCTION (this);

  /* Set local net device */
  m_device = GetNodeNetDevice (GetNode ());

  double parsed_scaling = 1;
  /* Load Time Scaling */
  if (m_scaling.rfind("x", 0) == 0)
  {
    m_scaling.erase(std::remove(m_scaling.begin(), m_scaling.end(), 'x'), m_scaling.end());
    parsed_scaling = 1/double(std::stoi(m_scaling));
  }
  else if (m_scaling.rfind("/", 0) == 0)
  {
    m_scaling.erase(std::remove(m_scaling.begin(), m_scaling.end(), '/'), m_scaling.end());
    parsed_scaling = double(std::stoi(m_scaling));
  }

  m_parsed_scaling = parsed_scaling;

  /* Loads all packets and set of prefixes in the bin file */
  LoadBinaryFile();
  uint32_t prefixes_added = 0;

  /* When we only fail allowed prefixes prefixes, and we consider them all */
 
  /* Load top prefixes file */
  TraceSendApplication::LoadTopPrefixes(m_topFile);

  /* Failable prefixes */
  failable_prefixes = top_prefixes;

  /* Here in the case of having a detectable prefixes file,
    we will even a bit more the failable_prefixes*/

  /* When we only fail allowed prefixes prefixes, and we consider them all */
  std::cout << m_allowedToFailFile << "\n";
  if (m_allowedToFailFile != "")
  {
    LoadAllowedPrefixes(m_allowedToFailFile);  
    /* SO our failable list of prefixes is this, thus if we fail top 10k, we are
       actually failing the top 10k of allowed to fail (usually because they are "detectable") */
    failable_prefixes = allowed_prefixes;
  } 

   /* Set prefixes to fail */
  uint32_t failable_size = failable_prefixes.size();

  //for (int i = 0; i < failable_size; i++)
  //{
  //  if (prefixes_in_trace.count(failable_size[i]) == 0)
  //  {
  //    std::cout << "problem" << std::endl;
  //    break;
  //  }
  //}

  if (m_numTopEntries > failable_size)
  {
    m_numTopEntries = failable_size;
  }
  
  std::cout << "Failable Prefixes: " << failable_size << " Top prefixes size: " << top_prefixes.size() << " prefixes in trace: " << prefixes_in_trace.size() << "\n";
  std::cout << "num top entries " << m_numTopEntries << " num top drops " << m_topDrops << " num bottom drops " << m_bottomDrops << "\n";
 
  /* If num_top_entries < m_topDrops we ajust it */
  if (m_numTopEntries < m_topDrops)
  {
    std::cout << "NumTopFails ajusted to match NumTopEntries " << m_topDrops << " -> " << m_numTopEntries << std::endl;
    m_topDrops = m_numTopEntries;
    /* we have to fail all of them anyways */
    m_topFailType = "All";
  }

  /* if total prefixes - top prefixes < num bottom we adjust too */
  if ((failable_size - m_numTopEntries) < m_bottomDrops)
  {
    std::cout << "numbottomDrops decreased to match remaining prefixes " << m_bottomDrops << " -> " << (failable_size - m_numTopEntries) << std::endl;
    m_bottomDrops = (failable_size - m_numTopEntries);
    /* we have to fail all of them anyways */
    m_bottomFailType = "All";
  }

  std::cout << "num top entries " << m_numTopEntries << " num top drops " << m_topDrops << " num bottom drops " << m_bottomDrops << "\n";

  /* Deciding what to fail */
  switch (GetFailType(m_topFailType))
  {
    case TopDown:
      prefixes_added +=AddToFailTopBottom(0, m_numTopEntries, m_topDrops);
      break;
    case Random:
      prefixes_added +=AddToFailRandom(0, m_numTopEntries, m_topDrops);
      break;
    case All:
      prefixes_added +=AddToFailAll(0, m_numTopEntries);
      break;
    case Unknown:
      NS_LOG_UNCOND("Unkown Fail type");
      break;
  } 

  std::cout << "TOP Prefixes to fail: " << prefixes_added << "\n";

  switch (GetFailType(m_bottomFailType))
  {
    case TopDown:
      prefixes_added +=AddToFailTopBottom(m_numTopEntries, failable_size, m_bottomDrops);
      break;
    case Random:
      prefixes_added +=AddToFailRandom(m_numTopEntries, failable_size, m_bottomDrops);
      break;
    case All:
      prefixes_added +=AddToFailAll(m_numTopEntries, failable_size);
      break;
    case Unknown:
      NS_LOG_UNCOND("Unkown Fail type");
      break;      
  }
  
  
  std::cout << "Total Prefixes to fail: " << prefixes_added << "\n";

  /* Save the prefixes we will fail */

  SaveFailedPrefixes ();
  std::cout << "Traffic Loaded" << std::endl;
  std::cout << "Starts sending packets" << std::endl;
  SendPackets ();
}

void TraceSendApplication::LoadTopPrefixes(std::string top_prefixes_file)
{
  std::vector<std::string> _top_prefixes = ns3::LoadTopPrefixes(top_prefixes_file);
  for (int i=0; i < _top_prefixes.size() ; i++)
  {
    uint32_t prefix = (Ipv4Address(_top_prefixes[i].c_str()).Get()) & 0xffffff00;

    /* Only add them if the prefix appears in the trace we send */
    /* This was more useful when we loaded a global top file, now we are using a sliced one*/
    /* however, just in case we can keep this check */
    if (prefixes_in_trace.count(prefix) > 0)
    {
      top_prefixes.push_back(prefix);
    }
  }
}

void TraceSendApplication::LoadAllowedPrefixes(std::string allowed_prefixes_file)
{

  /* Loads the allowed prefixes and keeps top prefix order */

  std::vector<std::string> _allowed_prefixes = ns3::LoadPrefixList(allowed_prefixes_file);
  std::unordered_set<uint32_t> _allowed_prefixes_set;

  /* build int set for fast check*/
  for (int i = 0; i < _allowed_prefixes.size(); i++)
  {
    uint32_t prefix = (Ipv4Address(_allowed_prefixes[i].c_str()).Get()) & 0xffffff00;
    _allowed_prefixes_set.insert(prefix);
  }

  /* We add them like this so we can keep the ORDER */
  for (int i = 0; i < top_prefixes.size(); i++)
  {
    uint32_t prefix = top_prefixes[i];

    if (_allowed_prefixes_set.count(prefix))
    {
      allowed_prefixes.push_back(prefix);
    }
  }
}

int TraceSendApplication::AddToFailAll(int min_limit, int max_limit)
{ 
  std::cout << min_limit << " " << max_limit << "\n";

  NS_ASSERT(min_limit <= failable_prefixes.size());
  NS_ASSERT(max_limit <= failable_prefixes.size());

  int initial_prefixes_dropped = prefixes_to_fail.size();
  for (int i = min_limit; i < max_limit; i++)
  {
    prefixes_to_fail.insert(failable_prefixes[i]);
  }
  return prefixes_to_fail.size() - initial_prefixes_dropped;
}

int TraceSendApplication::AddToFailTopBottom(int min_limit, int max_limit, int number_to_add)
{
  NS_ASSERT(min_limit <= failable_prefixes.size());
  NS_ASSERT(max_limit <= failable_prefixes.size());
  NS_ASSERT(number_to_add <= (max_limit - min_limit));

  int initial_prefixes_dropped = prefixes_to_fail.size();
  int added = 0;
  for (int i  = min_limit; (i < max_limit) && (added != number_to_add); i++)
  { 
    uint32_t prefix = failable_prefixes[i];
    if (prefixes_to_fail.count(prefix) == 0)
    {
      prefixes_to_fail.insert(prefix);
      added++;
    } 
  }
  return prefixes_to_fail.size() - initial_prefixes_dropped;
}

int TraceSendApplication::AddToFailRandom(int min_limit, int max_limit, int number_to_add)
{

  NS_ASSERT(min_limit <= failable_prefixes.size());
  NS_ASSERT(max_limit <= failable_prefixes.size());
  NS_ASSERT(number_to_add <= (max_limit - min_limit));

  int initial_prefixes_dropped = prefixes_to_fail.size();
  int added = 0;
  int count = 0;
  Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable>();

  while ((added != number_to_add) and (prefixes_to_fail.size() != failable_prefixes.size()))
  {
    count++;
    uint32_t prefix = failable_prefixes[random_generator->GetInteger(min_limit, max_limit-1)];
    if (prefixes_to_fail.count(prefix) == 0)
    {
      prefixes_to_fail.insert(prefix);
      added++;
    }

    // In case this never finishes
    if (count > 2000000)
    { 
      NS_LOG_UNCOND("AddToFailRandom could not find " << number_to_add << " prefixes to add. Instead found: " << (prefixes_to_fail.size() - initial_prefixes_dropped));
      return prefixes_to_fail.size() - initial_prefixes_dropped;
    }

  }
  return prefixes_to_fail.size() - initial_prefixes_dropped;
}

void TraceSendApplication::SendPackets(void)
{
  packet data = m_binary_packets[m_sentPackets];

  data.dst = (data.dst & 0xffffff00);
  uint8_t tos = 0;
  if (prefixes_to_fail.count(data.dst) > 0)
  {
    tos = 1;
  }
  Ptr<Packet> p = CreateTcpSynPacket(5555, 7777, data.size, data.dst, tos);
  m_device->SendFrom(p, m_device->GetAddress(), m_dstAddr, 0x0800);
  m_sentPackets++;
  if (m_sentPackets == m_binary_packets.size())
  {
    return;
  }

  packet next = m_binary_packets[m_sentPackets];
  
  /* Schedule the event for the next packet to be sent respecting the scaling factor */
  /* We use picoseconds so we can keep sub nanosecond precission */
  m_sendEvent = Simulator::Schedule(PicoSeconds((next.ts - data.ts)* 1000 * m_parsed_scaling), &TraceSendApplication::SendPackets, this);
}

void TraceSendApplication::LoadBinaryFile (void)
{
  /* NOTE BE CAREFUL WITH THE IP ENDIANESS */
  /*/ Read binary file */
  std::ifstream file(m_inFile, std::ios::in | std::ios::binary);
  if(!file) {
    std::cout << "Cannot open file!" << std::endl;
    return;
  }

  file.seekg (0, file.end);
  int length = file.tellg();
  file.seekg(0, file.beg);

  int read_bytes = 0;

  /* Read packets one by one */
  while (read_bytes < length)
  {
    packet p;
    file.read((char*)&p, 14);
    /* add packet to packet stream */
    m_binary_packets.push_back(p);
    /* add dst to set of destinations */
    prefixes_in_trace.insert(p.dst & 0xffffff00);
    read_bytes +=14;
  }
  file.close();
}

void TraceSendApplication::StopApplication (void) 
{
  NS_LOG_FUNCTION (this);
  Simulator::Cancel(m_sendEvent);
  DoDispose ();
}

} // Namespace ns3
