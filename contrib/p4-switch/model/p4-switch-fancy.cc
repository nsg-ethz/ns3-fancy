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
#include "p4-switch-fancy.h"
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
#include "fancy-header.h"
#include "ns3/double.h"
#include "ns3/enum.h"
#include "ns3/utils.h"

using namespace std;
namespace ns3 {

  NS_LOG_COMPONENT_DEFINE("P4SwitchFancy");

  NS_OBJECT_ENSURE_REGISTERED(P4SwitchFancy);

#define IPV4 0x0800
#define ARP  0x0806
#define IPV6 0x86DD
#define FANCY 0x0801
#define TCP  6
#define UDP  17

  // Actions
#define KEEP_ALIVE     0
#define GREY_START     1
#define GREY_STOP      2
#define GREY_COUNTER   4  //Packet contains a counter
#define MULTIPLE_COUNTERS 16
#define COUNTER_MAXIMUMS 8

/* README:
  m_numerTopEntries is the number of top entries reserved by the system. IDs go from 0 to m_numTopEntries - 1.
  Id m_numberTopEntries belongs to the zooming tree.
  Id m_numberTopEntries+1 belongs to a global dedicated counter.
*/

  TypeId
    P4SwitchFancy::GetTypeId(void)
  {
    static TypeId tid = TypeId("ns3::P4SwitchFancy")
      .SetParent<P4SwitchNetDevice>()
      .SetGroupName("P4-Switch")
      .AddConstructor<P4SwitchFancy>()
      .AddAttribute("NumTopEntries", "Where to put the threshold of big and small prefixes",
        UintegerValue(100),
        MakeUintegerAccessor(&P4SwitchFancy::m_numTopEntries),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("TreeEnabled", "Enable or disable the zooming tree",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchFancy::m_treeEnabled),
        MakeBooleanChecker())
      .AddAttribute("SoftDetectionEnabled", "Enable or disable soft detection detections.",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchFancy::m_treeEnableSoftDetections),
        MakeBooleanChecker())
      .AddAttribute("RerouteEnabled", "Enable or disable traffic rerouting",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchFancy::m_rerouteEnabled),
        MakeBooleanChecker())
      .AddAttribute("Pipeline", "Enable or disable the pipeline",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchFancy::m_pipeline),
        MakeBooleanChecker())
      .AddAttribute("PipelineBoost", "Enable or disable the pipeline boost",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchFancy::m_pipelineBoost),
        MakeBooleanChecker())
      .AddAttribute("TreeDepth", "Depth of the zooming data structure.",
        UintegerValue(5),
        MakeUintegerAccessor(&P4SwitchFancy::m_treeDepth),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("LayerSplit", "Amount of childs each layer can expand to.",
        UintegerValue(2),
        MakeUintegerAccessor(&P4SwitchFancy::m_layerSplit),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("CounterWidth", "Number of counters per tree cell.",
        UintegerValue(16),
        MakeUintegerAccessor(&P4SwitchFancy::m_counterWidth),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("CounterBloomFilterWidth", "Width of bloom filters per counter.",
        UintegerValue(16),
        MakeUintegerAccessor(&P4SwitchFancy::m_counterBloomFilterWidth),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("RerouteBloomFilterWidth", "Cells in the reroute bloom filter.",
        UintegerValue(1000),
        MakeUintegerAccessor(&P4SwitchFancy::m_rerouteBloomFilterWidth),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("RerouteBloomFilterNumHashes", "Number of hashes used for the reroute bloom filter.",
        UintegerValue(3),
        MakeUintegerAccessor(&P4SwitchFancy::m_rerouteBloomFilterNumHashes),
        MakeUintegerChecker<uint32_t>())
      .AddAttribute("MaxCounterCollisions", "Number of maximum amout of collisions accepted in a counter field.",
        UintegerValue(1),
        MakeUintegerAccessor(&P4SwitchFancy::m_maxCounterCollisions),
        MakeUintegerChecker<uint16_t>())
      .AddAttribute("UniformLossThreshold", "Number of entries affected at the first layer such that we report the link to experience uniform random loss",
        UintegerValue(0),
        MakeUintegerAccessor(&P4SwitchFancy::m_uniformLossThreshold),
        MakeUintegerChecker<uint16_t>())
      .AddAttribute("CostType", "Cost type used to sort most affected cells.",
        UintegerValue(1),
        MakeUintegerAccessor(&P4SwitchFancy::m_costType),
        MakeUintegerChecker<uint16_t>())
      .AddAttribute("RerouteMinCost", "Min Cost to Reroute Something.",
        DoubleValue(0),
        MakeDoubleAccessor(&P4SwitchFancy::m_rerouteMinCost),
        MakeDoubleChecker<double>())
      .AddAttribute("TmDropRate", "Percentage of traffic that gets dropped by the traffic manager.",
        DoubleValue(0),
        MakeDoubleAccessor(&P4SwitchFancy::m_tm_drop_rate),
        MakeDoubleChecker<double>(0, 1))
      .AddAttribute("FailDropRate", "Percentage of traffic that gets dropped when a prefix is affected.",
        DoubleValue(1),
        MakeDoubleAccessor(&P4SwitchFancy::m_fail_drop_rate),
        MakeDoubleChecker<double>(0, 1))
      .AddAttribute("CheckPortStateMs", "Frequency check for port down.",
        DoubleValue(25),
        MakeDoubleAccessor(&P4SwitchFancy::m_check_port_state_ms),
        MakeDoubleChecker<double>())
      .AddAttribute("CheckPortStateEnable", "Enable or disable checking for port state",
        BooleanValue(true),
        MakeBooleanAccessor(&P4SwitchFancy::m_check_port_state_enable),
        MakeBooleanChecker())
      .AddAttribute("StartSystemSec", "Starting time for the system detection.",
        DoubleValue(2),
        MakeDoubleAccessor(&P4SwitchFancy::m_start_system_sec),
        MakeDoubleChecker<double>())
      .AddAttribute("AckWaitTimeMs", "Time a node waits for ack before retransmissions.",
        DoubleValue(5),
        MakeDoubleAccessor(&P4SwitchFancy::m_ack_wait_time_ms),
        MakeDoubleChecker<double>())
      .AddAttribute("SendCounterWaitMs", "Time a node waits for counters ack before retransmission.",
        DoubleValue(5),
        MakeDoubleAccessor(&P4SwitchFancy::m_send_counter_wait_ms),
        MakeDoubleChecker<double>())
      .AddAttribute("TimeBetweenCampaingMs", "Wait time between Counting campaings, this can be 0.",
        DoubleValue(2),
        MakeDoubleAccessor(&P4SwitchFancy::m_time_between_campaing_ms),
        MakeDoubleChecker<double>())
      .AddAttribute("ProbingTimeZoomingMs", "Amount of time accomulating counters.",
        DoubleValue(200),
        MakeDoubleAccessor(&P4SwitchFancy::m_probing_time_zooming_ms),
        MakeDoubleChecker<double>())
      .AddAttribute("ProbingTimeTopEntriesMs", "Amount of time accomulating counters.",
        DoubleValue(200),
        MakeDoubleAccessor(&P4SwitchFancy::m_probing_time_top_entries_ms),
        MakeDoubleChecker<double>())
      .AddAttribute("PacketHashType", "Selects the header fields used for the hashing.",
        StringValue("FiveTupleHash"),
        MakeStringAccessor(&P4SwitchFancy::m_packet_hash_type),
        MakeStringChecker())
      .AddAttribute("OutFile", "Name of the output file for the data generated.",
        StringValue(""),
        MakeStringAccessor(&P4SwitchFancy::m_outFile),
        MakeStringChecker())
      .AddAttribute("EnableSaveDrops", "Enable saving all the drop timestamps into a file.",
        BooleanValue(false),
        MakeBooleanAccessor(&P4SwitchFancy::m_enableSaveDrops),
        MakeBooleanChecker())
      .AddAttribute("TopFile", "File with all the global top prefixes",
        StringValue(""),
        MakeStringAccessor(&P4SwitchFancy::m_topFile),
        MakeStringChecker())
      .AddAttribute("EarlyStopCounter", "Stops simulation before",
        UintegerValue(0),
        MakeUintegerAccessor(&P4SwitchFancy::m_early_stop_counter),
        MakeUintegerChecker<uint32_t>())
      ;
    return tid;
  }

  P4SwitchFancy::P4SwitchFancy()
    :
    header_stack_reverse(header_stack)
  {
    std::reverse(header_stack_reverse.begin(), header_stack_reverse.end());
  }

  P4SwitchFancy::~P4SwitchFancy()
  {
    NS_LOG_FUNCTION_NOARGS();
    if (m_hash)
    {
      m_hash->Clean();
    }

    if (m_outFile != "")
    {
      // save detections
      m_simState->SaveInJson(m_outFile);

      // save packet drops
      if (m_drop_times.size() > 0)
      {
        if (m_enableSaveDrops)
        {
          std::string drops_file = m_outFile;
          drops_file.replace(drops_file.find(".json"), 5, ".drops");
          SaveDrops(drops_file);
        }
      }
    }
  }

  /* Hash functions */

  uint32_t
    P4SwitchFancy::GetFlowHash(ip_five_tuple& five_tuple, int hash_index, int modulo)
  {
    char s[13];
    IpFiveTupleToBuffer(s, five_tuple);
    return m_hash->GetHash(s, 13, hash_index, modulo);
  }

  uint32_t
    P4SwitchFancy::GetDstPrefixHash(ip_five_tuple& five_tuple, int hash_index, int modulo)
  {
    char s[4];
    /* assume /24 */
    DstPrefixToBuffer(s, five_tuple);
    return m_hash->GetHash(s, 4, hash_index, modulo);
  }

  /* Bloom filter hashes */
  void
    P4SwitchFancy::GetBloomFilterHashes(ip_five_tuple& ip_five_tuple, uint32_t hash_indexes[], int unit_modulo, int bloom_filter_modulo)
  {
    char s[m_treeDepth];
    char* ptr = s;
    for (uint32_t i = 0; i < m_treeDepth; i++)
    {
      uint8_t h = uint8_t((this->*m_packet_hash)(ip_five_tuple, i, unit_modulo));
      std::memcpy(ptr + (i), &h, 1);
    }
    for (uint32_t i = 0; i < m_rerouteBloomFilterNumHashes; i++)
    {
      hash_indexes[i] = m_hash->GetHash(s, m_treeDepth, i, bloom_filter_modulo);
    }
  }

  bool
    P4SwitchFancy::IsBloomFilterSet(FancyPortInfo& portInfo, const uint32_t hash_indexes[])
  {
    bool res = true;
    for (uint32_t i = 0; i < m_rerouteBloomFilterNumHashes; i++)
    {
      res &= portInfo.reroute[hash_indexes[i]].set;
    }
    return res;
  }

  void
    P4SwitchFancy::SetBloomFilter(FancyPortInfo& portInfo, char hash_path[], uint32_t bloom_filter_hashes[])
  {
    for (uint32_t bf_index = 0; bf_index < m_rerouteBloomFilterNumHashes; bf_index++)
    {
      uint32_t path_hash = m_hash->GetHash(hash_path, m_treeDepth, bf_index, m_rerouteBloomFilterWidth);
      bloom_filter_hashes[bf_index] = path_hash;
      portInfo.reroute[path_hash].set = true;
    }
  }

  /* End hash functions */

  void
    P4SwitchFancy::AddSwitchPort(Ptr<NetDevice> switchPort)
  {
    NS_LOG_FUNCTION_NOARGS();

    P4SwitchNetDevice::AddSwitchPort(switchPort);

    uint32_t ifIndex = switchPort->GetIfIndex();
    FancyPortInfo& portInfo = m_portsInfo[ifIndex];
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
    P4SwitchFancy::SetDebug(bool state)
  {
    m_enableDebug = state;
    m_simState->SetDetail(m_enableDebug);
  }

  void
    P4SwitchFancy::Init()
  {
    P4SwitchNetDevice::Init();

    /* allocates the simulation state object */
    m_simState = std::make_unique<FancySimulationState>(m_treeDepth, m_rerouteBloomFilterNumHashes);

    /* Set debugging details */
    m_simState->SetDetail(m_enableDebug);

    /* Sets tm drop error model */
    tm_em->SetAttribute("ErrorRate", DoubleValue(m_tm_drop_rate));
    tm_em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    /* Sets tm drop error model */
    fail_em->SetAttribute("ErrorRate", DoubleValue(m_fail_drop_rate));
    fail_em->SetAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET));

    /* pointer to our packet hash function*/
    if (m_packet_hash_type == "FiveTupleHash")
    {
      m_packet_hash = &P4SwitchFancy::GetFlowHash;
      m_packet_str_funct = &IpFiveTupleToString;
    }
    else if (m_packet_hash_type == "DstPrefixHash")
    {
      m_packet_hash = &P4SwitchFancy::GetDstPrefixHash;
      m_packet_str_funct = &DstPrefixToString;
    }
    else
    {
      m_packet_hash = &P4SwitchFancy::GetFlowHash;
      m_packet_str_funct = &IpFiveTupleToString;
    }

    /* Compute the num nodes and timeout time */
    if (m_treeEnabled)
    {
      /* Set hashes array */
      m_hash = std::make_unique<HashUtils>(m_treeDepth * 2);
      if (m_layerSplit > 1)
      {
        m_nodesInTree = uint32_t((pow(m_layerSplit, m_treeDepth) - 1) / (m_layerSplit - 1));
      }
      else if (m_layerSplit == 1)
      {
        m_nodesInTree = m_treeDepth;
      }
      std::cout << "Nodes in the tree: " << m_nodesInTree << std::endl;
    }

    m_send_port_state_ms = m_check_port_state_ms / 2;

    /* Load K Top Entries Tables,
    NOTE: right now we just have a global one, this
    has to be changed at a per-egress*/
    /* Its actually fine */
    //LoadTopEntriesTable("control_plane/s" + std::to_string(m_switchId) + "-topEntries.txt");
    //LoadTopEntriesTable("control_plane/top-entries.txt");
    if (m_packet_hash_type == "FiveTupleHash")
    {
      LoadTopFlowEntriesTable(m_topFile, m_numTopEntries);
    }
    else if (m_packet_hash_type == "DstPrefixHash")
    {
      LoadTopDstPrefixEntriesTable(m_topFile, m_numTopEntries);
    }
    else
    {
      LoadTopFlowEntriesTable(m_topFile, m_numTopEntries);
    }

    /* Allocate port structure memories */
    for (uint32_t i = 0; i < m_ports.size(); i++)
    {
      uint32_t ifIndex = m_ports[i]->GetIfIndex();
      Ptr<NetDevice> switchPort = m_ports[i];
      FancyPortInfo& portInfo = m_portsInfo[ifIndex];
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
        Ptr<Packet> packet = Create<Packet>();
        FancyHeader fancy_hdr;
        // We use grey type since it does not matter
        fancy_hdr.SetAction(KEEP_ALIVE);

        pkt_info meta(switchPort->GetAddress(), portInfo.otherPortDevice->GetAddress(), FANCY);
        meta.headers["FANCY"] = fancy_hdr;
        meta.outPort = switchPort;

        if (m_check_port_state_enable)
        {
          std::cout << "TIME TESTS " << m_send_port_state_ms << " " << m_check_port_state_ms << std::endl;
          // Total link failure events to be scheduled
          SendProbe(MilliSeconds(m_send_port_state_ms), packet, meta);
          Simulator::Schedule(Seconds(m_start_system_sec), &P4SwitchFancy::CheckPortState, this, MilliSeconds(m_check_port_state_ms), switchPort);
        }
        else
        {
          std::cout << "PORT CHECK DISABLED" << std::endl;
        }

        Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable>();
        Time start_at = Seconds(m_start_system_sec);
        // Start the top k entries machine
        for (uint32_t id = 0; id < m_numTopEntries; id++)
        {
          start_at = Seconds(std::max(0.0, m_start_system_sec - random_generator->GetValue(0, 0.15)));
          portInfo.greySend.currentEvent[id] = Simulator::Schedule(start_at, &P4SwitchFancy::SendGreyAction,
            this, switchPort, (GREY_START), id);
        }

        // Start the zooming machine
        if (m_treeEnabled)
        {
          // start_at = Seconds(std::max(0.0, m_start_system_sec - random_generator->GetValue(0, 0.05)));
          portInfo.greySend.currentEvent[m_numTopEntries] = Simulator::Schedule(Seconds(m_start_system_sec), &P4SwitchFancy::SendGreyAction,
            this, switchPort, (GREY_START | COUNTER_MAXIMUMS), m_numTopEntries);
        }

        // Special global dedicated counter
        /* This special state machine ignores the counting flag, which is technically not needed */
        /* Counting is performed as long as the state is in counting (since it is perfectly synched) */
        if (m_treeEnableSoftDetections)
        {
          start_at = Seconds(std::max(0.0, m_start_system_sec - random_generator->GetValue(0, 0.15)));
          portInfo.greySend.currentEvent[m_numTopEntries + 1] = Simulator::Schedule(start_at, &P4SwitchFancy::SendGreyAction,
            this, switchPort, (GREY_START), m_numTopEntries + 1);
        }
      }
    }
  }

  void
    P4SwitchFancy::DisableTopEntries()
  {

    /* Allocate port structure memories */
    for (uint32_t i = 0; i < m_ports.size(); i++)
    {
      uint32_t ifIndex = m_ports[i]->GetIfIndex();
      Ptr<NetDevice> switchPort = m_ports[i];
      FancyPortInfo& portInfo = m_portsInfo[ifIndex];

      Ptr<Node> otherSide = portInfo.otherPortDevice->GetNode();
      std::string otherSideName = Names::FindName(otherSide);

      // The state machine is lunched only if the other side is a switch
      if (otherSideName.find("s") != std::string::npos)
      {
        // Start the top k entries machine
        for (uint32_t id = 0; id < m_numTopEntries; id++)
        {
          portInfo.greySend.currentEvent[id].Cancel();
          portInfo.greyRecv.currentEvent[id].Cancel();
        }
      }
    }
  }

  void
    P4SwitchFancy::DisableAllFSM()
  {
    /* Allocate port structure memories */
    for (uint32_t i = 0; i < m_ports.size(); i++)
    {
      uint32_t ifIndex = m_ports[i]->GetIfIndex();
      Ptr<NetDevice> switchPort = m_ports[i];
      FancyPortInfo& portInfo = m_portsInfo[ifIndex];

      Ptr<Node> otherSide = portInfo.otherPortDevice->GetNode();
      std::string otherSideName = Names::FindName(otherSide);

      // The state machine is lunched only if the other side is a switch
      if (otherSideName.find("s") != std::string::npos)
      {
        // Start the top k entries machine
        for (uint32_t id = 0; id <= m_numTopEntries + 1; id++)
        {
          portInfo.greySend.currentEvent[id].Cancel();
        }
      }
    }
  }

  void
    P4SwitchFancy::InitPortInfo(FancyPortInfo& portInfo)
  {

    portInfo.reroute = std::vector<RerouteBloomFilter>(m_rerouteBloomFilterWidth);
    portInfo.reroute_top = std::vector<RerouteTopEntries>(m_numTopEntries + 2);

    /* Prepare all the state machines */
    portInfo.greyRecv.localCounter = std::vector<uint64_t>(m_numTopEntries + 2, 0);
    portInfo.greyRecv.currentSEQ = std::vector<uint32_t>(m_numTopEntries + 2, 0);
    portInfo.greyRecv.currentEvent = std::vector<EventId>(m_numTopEntries + 2);
    portInfo.greyRecv.greyState = std::vector<GreyState>(m_numTopEntries + 2, GreyState::IDLE);
    /* Debug thing to remove */
    portInfo.greyRecv.last_packet_seq = std::vector<uint32_t>(m_numTopEntries + 2);

    /* initialize Zooming data structure for the receiver  Recv*/
    if (m_treeEnabled)
    {
      portInfo.greyRecv.counter_tree = std::vector<HashCounter>(m_nodesInTree);
      for (uint32_t i = 0; i < portInfo.greyRecv.counter_tree.size(); i++)
      {
        portInfo.greyRecv.counter_tree[i].counters = std::vector<uint32_t>(m_counterWidth);
        portInfo.greyRecv.counter_tree[i].last_flow = std::vector<ip_five_tuple>(m_counterWidth);

        portInfo.greyRecv.counter_tree[i].hashed_flows = std::vector<std::unordered_map<std::string, ip_five_tuple>>(m_counterWidth);
        portInfo.greyRecv.counter_tree[i].bloom_filter = std::vector<boost::dynamic_bitset<>>(m_counterWidth);

        for (uint32_t j = 0; j < m_counterWidth; j++)
        {
          portInfo.greyRecv.counter_tree[i].bloom_filter[j] = boost::dynamic_bitset<>(m_counterBloomFilterWidth);
        }
        portInfo.greyRecv.counter_tree[i].max_cells = std::vector<std::vector<uint8_t>>(m_treeDepth);
        for (uint32_t j = 0; j < m_treeDepth; j++)
        {
          portInfo.greyRecv.counter_tree[i].max_cells[j] = std::vector<uint8_t>(m_layerSplit);
        }
      }
    }

    /* Prepare all the state machines */
    portInfo.greySend.localCounter = std::vector<uint64_t>(m_numTopEntries + 2, 0);
    portInfo.greySend.currentSEQ = std::vector<uint32_t>(m_numTopEntries + 2, 0);
    portInfo.greySend.currentEvent = std::vector<EventId>(m_numTopEntries + 2);
    portInfo.greySend.greyState = std::vector<GreyState>(m_numTopEntries + 2, GreyState::IDLE);
    /* Debug thing to remove */
    portInfo.greySend.last_packet_seq = std::vector<uint32_t>(m_numTopEntries + 2);

    /* Initialize Zooming data structure for the sender side*/
    if (m_treeEnabled)
    {
      portInfo.greySend.counter_tree = std::vector<HashCounter>(m_nodesInTree);
      for (uint32_t i = 0; i < portInfo.greySend.counter_tree.size(); i++)
      {
        portInfo.greySend.counter_tree[i].counters = std::vector<uint32_t>(m_counterWidth);
        portInfo.greySend.counter_tree[i].last_flow = std::vector<ip_five_tuple>(m_counterWidth);
        portInfo.greySend.counter_tree[i].hashed_flows = std::vector<std::unordered_map<std::string, ip_five_tuple>>(m_counterWidth);
        portInfo.greySend.counter_tree[i].bloom_filter = std::vector<boost::dynamic_bitset<>>(m_counterWidth);

        for (uint32_t j = 0; j < m_counterWidth; j++)
        {
          portInfo.greySend.counter_tree[i].bloom_filter[j] = boost::dynamic_bitset<>(m_counterBloomFilterWidth);
        }
        portInfo.greySend.counter_tree[i].max_cells = std::vector<std::vector<uint8_t>>(m_treeDepth);
        for (uint32_t j = 0; j < m_treeDepth; j++)
        {
          portInfo.greySend.counter_tree[i].max_cells[j] = std::vector<uint8_t>(m_layerSplit);
        }
      }
    }
  }


  void P4SwitchFancy::DoParser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
  {
    NS_LOG_FUNCTION_NOARGS();

    switch (protocol)
    {
    case FANCY:
    {
      ParseFancy(packet, meta);
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

  void P4SwitchFancy::ParseFancy(Ptr<Packet> packet, pkt_info& meta)
  {
    FancyHeader fancy_hdr;
    packet->RemoveHeader(fancy_hdr);
    meta.headers["FANCY"] = fancy_hdr;
    Parser(packet, meta, fancy_hdr.GetNextHeader());
  }

  void P4SwitchFancy::ParseArp(Ptr<Packet> packet, pkt_info& meta)
  {
    ArpHeader arp_hdr;
    packet->RemoveHeader(arp_hdr);
    meta.headers["ARP"] = arp_hdr;
  }

  void P4SwitchFancy::ParseIpv4(Ptr<Packet> packet, pkt_info& meta)
  {
    Ipv4Header ipv4_hdr;
    packet->RemoveHeader(ipv4_hdr);
    meta.headers["IPV4"] = ipv4_hdr;
    ParseTransport(packet, meta, ipv4_hdr.GetProtocol());
  }

  void P4SwitchFancy::ParseTransport(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol)
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

  void P4SwitchFancy::ParseIpv6(Ptr<Packet> packet, pkt_info& meta)
  {

  }

  void
    P4SwitchFancy::DoVerifyChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void
    P4SwitchFancy::DoIngress(Ptr<const Packet> packet, pkt_info& meta)
  {

    NS_LOG_FUNCTION_NOARGS();
    //L2Learning(packet, meta);
    //L2Forwarding(meta);

    //PortForwarding(meta);

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

    // remove 1ttl just for testing
    //if (meta.headers.count("IPV4") > 0)
    //{
    //  Ipv4Header & ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
    //  ipv4_hdr.SetTtl(ipv4_hdr.GetTtl()-1);
    //}

    // Execute counter box
    IngressCounterBox(packet, meta);

  }

  void
    P4SwitchFancy::DoTrafficManager(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    /* Lets emulate congestion drops */

    /* We just used this to show that
       congestion does not affect us*/

    if (m_tm_drop_rate > 0)
    {
      /* Get action */
      uint8_t action = 0;
      if (meta.headers.count("FANCY") > 0)
      {
        FancyHeader& fancy_hdr = std::any_cast<FancyHeader&>(meta.headers["FANCY"]);
        action = fancy_hdr.GetAction();

      }

      /* only normal traffic, we do not drop good traffic */
      if (action == 0)
      {
        // Not sure why this copy was here in first place
        Ptr<Packet> pkt = packet->Copy();
        /* drop packet */
        if (tm_em->IsCorrupt(pkt))
        {
          return;
        }
      }
    }

    Egress(packet, meta);
  }


  void
    P4SwitchFancy::DoEgress(Ptr<const Packet> packet, pkt_info& meta)
  {

    NS_LOG_FUNCTION_NOARGS();
    EgressCounterBox(packet, meta);

  }

  void
    P4SwitchFancy::DoUpdateChecksums(Ptr<const Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();
  }

  void
    P4SwitchFancy::DoDeparser(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    // Deparse valid headers if there are headers to deparse
    if (meta.headers.size() > 0)
    {
      for (auto hdr_type : header_stack_reverse)
      {
        if (meta.headers.count(hdr_type) > 0)
        {
          if (hdr_type == "FANCY")
          {
            packet->AddHeader(std::any_cast<FancyHeader>(meta.headers[hdr_type]));
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
    P4SwitchFancy::DoEgressTrafficManager(Ptr<Packet> packet, pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    if (meta.headers.count("IPV4") > 0)
    {
      // drop packet at the egress
      Ipv4Header& ipv4_hdr = std::any_cast<Ipv4Header&>(meta.headers["IPV4"]);
      uint8_t tos = ipv4_hdr.GetTos();

      /* Here is where we decide if a packet has to be dropped or not
         as an easy way our sender the TOS field to a number, if that number
         is this switch ID then we drop the packet with "m_fail_drop_rate" probability
      */

      /* Gray drop comes from the forwarding table, it is a flag that gets set there */

      if (tos == m_switchId || meta.gray_drop)
      {
        /* only normal traffic, we do not drop good traffic */
        //Ptr<Packet> pkt = packet->Copy();
        /* drop packet */
        if (m_fail_drop_rate == 1 || fail_em->IsCorrupt(packet))
        {
          // TODO add a flag here from a param to enable disable
          m_drop_times.push_back(Simulator::Now());
          // TODO REMOVE
          // std::cout << "DROP PACKET: " << ipv4_hdr.GetDestination() << std::endl;
          return;
        }
      }
    }

    // Before the packet leaves the switch
    // Update Send Timestamp
    FancyPortInfo& outPortInfo = m_portsInfo[meta.outPort->GetIfIndex()];
    outPortInfo.last_time_sent = Simulator::Now();
    /* Removed the sense of l2 swtich we had before, now for each packet we modify the mac address, useful to track what happens */
    SendOutFrom(packet, meta.outPort, meta.outPort->GetAddress(), outPortInfo.otherPortDevice->GetAddress(), meta.protocol);
    //SendOutFrom(packet, meta.outPort, meta.src, meta.dst, meta.protocol);
  }

  /*  Ingress Heavy hitter table

      This is a temporal solution since we should do this at a per-egress and not
      like we will do here as a global table
  */

  /* Loads top flow entries and fixes some flow parameters, this is only useful
  when using our static trace-based generator
  */
  void P4SwitchFancy::LoadTopFlowEntriesTable(std::string top_entries_file, int k)
  {
    std::vector<std::string> top_entries = LoadTopPrefixes(top_entries_file);
    NS_ASSERT(top_entries.size() >= m_numTopEntries);
    uint32_t index = 0;

    for (int i = 0; i < k; i++)
    {
      /* Create flow */
      ip_five_tuple flow;

      uint32_t src_num = Ipv4Address("102.102.102.102").Get();
      uint32_t dst_num = Ipv4Address(top_entries[i].c_str()).Get();

      flow.src_ip = src_num;
      flow.dst_ip = dst_num;
      flow.protocol = uint8_t(6);
      flow.src_port = uint16_t(5555);
      flow.dst_port = uint16_t(7777);

      std::string flow_str = IpFiveTupleToString(flow);

      m_topEntries[flow_str] = index;
      index++;
    }

    // This index will be used to find the zooming data structure
    //m_numTopEntries = index;
    std::cout << "Top entries Loaded: " << m_numTopEntries << std::endl;
  }

  void P4SwitchFancy::LoadTopDstPrefixEntriesTable(std::string top_entries_file, int k)
  {
    std::vector<std::string> top_entries = LoadTopPrefixes(top_entries_file);
    NS_ASSERT(top_entries.size() >= m_numTopEntries);
    uint32_t index = 0;

    for (int i = 0; i < k; i++)
    {
      /* Create flow */
      uint32_t dst_prefix = Ipv4Address(top_entries[i].c_str()).Get();
      ip_five_tuple flow;
      flow.dst_ip = dst_prefix;

      std::string dst_prefix_str = DstPrefixToString(flow);
      m_topEntries[dst_prefix_str] = index;
      index++;
    }

    // This index will be used to find the zooming data structure
    std::cout << "Top entries Loaded: " << m_numTopEntries << std::endl;
  }

  uint32_t P4SwitchFancy::MatchTopEntriesTable(pkt_info& meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    std::unordered_map<std::string, uint32_t>::iterator iter = m_topEntries.find(meta.str_flow);
    if (iter != m_topEntries.end())
    {
      return iter->second;
    }
    else
    {
      /* id of the tree */
      return m_numTopEntries;
    }
  }

  void P4SwitchFancy::ShowTopEntriesTable(void)
  {
    std::cout << "TOP ENTRIES TABLE" << std::endl;
    std::cout << "=================" << std::endl;
    for (auto it = m_topEntries.begin(); it != m_topEntries.end(); it++)
    {
      std::cout << it->first << " -> " << it->second << std::endl;
    }
  }


  // Loss detection event; This one does not require any special ID info.
  void
    P4SwitchFancy::SendProbe(const Time& delay, Ptr<const Packet> packet, pkt_info meta)
  {
    NS_LOG_FUNCTION_NOARGS();

    // check if we really need to send 
    FancyPortInfo& portInfo = m_portsInfo[meta.outPort->GetIfIndex()];
    Simulator::Schedule(delay, &P4SwitchFancy::SendProbe, this, delay, packet, meta);

    // Only send if there is no traffic 
    if (Simulator::Now() - portInfo.last_time_sent >= delay)
    {
      meta.internalPacket = true;
      TrafficManager(packet, meta);
    }
  }

  void P4SwitchFancy::CheckPortState(const Time& delay, Ptr<NetDevice> port)
  {
    NS_LOG_FUNCTION_NOARGS();

    FancyPortInfo& portInfo = m_portsInfo[port->GetIfIndex()];

    // Nothing received 
    if (Simulator::Now() - portInfo.last_time_received > delay)
    {
      if (portInfo.linkState == true)
      {
        _NS_LOG_DEBUG("\033[1;31mEvent: Detected link failure at node: " << Names::FindName(port->GetNode()) << " at time: " << Simulator::Now().GetSeconds() << "\033[0m" << std::endl, m_enableDebug);
        portInfo.linkState = false;
      }
    }

    if (portInfo.linkState == false && ((Simulator::Now() - portInfo.last_time_received) < delay))
    {
      _NS_LOG_DEBUG("\033[1;32mEvent: Detected a link recover at node: " << Names::FindName(port->GetNode()) << " at time: " << Simulator::Now().GetSeconds() << "\033[0m" << std::endl, m_enableDebug);
      portInfo.linkState = true;
    }

    Simulator::Schedule(delay, &P4SwitchFancy::CheckPortState, this, delay, port);
  }

  void
    P4SwitchFancy::SetGreyState(FancyPortInfo& portInfo, GreyState newState, uint32_t id, bool in)
  {

    GreyState oldState;

    std::string direction;

    if (in)
    {
      oldState = portInfo.greyRecv.greyState[id];
      portInfo.greyRecv.greyState[id] = newState;
      direction = "recv";
    }
    else
    {
      oldState = portInfo.greySend.greyState[id];
      portInfo.greySend.greyState[id] = newState;
      direction = "send";
    }

    NS_LOG_WARN(Simulator::Now().GetSeconds() << ": State Change (id: " << id << "| " << m_name << ", " << portInfo.link_name << ", " << direction << "): " << GreyStateToString[oldState] << "->" << GreyStateToString[newState]);

  }

  void P4SwitchFancy::SendGreyAction(Ptr<NetDevice> outPort, uint8_t action, uint32_t id)
  {
    NS_LOG_FUNCTION_NOARGS();

    uint32_t ifIndex = outPort->GetIfIndex();
    FancyPortInfo& portInfo = m_portsInfo[ifIndex];

    Ptr<Packet> packet = Create<Packet>();
    FancyHeader fancy_hdr;
    fancy_hdr.SetAction(action);
    fancy_hdr.SetFSMFlag(0);
    fancy_hdr.SetSeq(portInfo.greySend.currentSEQ[id]);
    fancy_hdr.SetId(id);

    // Schedule action again
    // if we want multiple trees this needs to be changed. 
    if (action == (GREY_START | COUNTER_MAXIMUMS) && id == m_numTopEntries)
    {
      portInfo.greySend.localCounter[id] = 0;
      SetGreyState(portInfo, GreyState::START_ACK, id, false);
      fancy_hdr.SetDataWidth(m_layerSplit);
      for (uint16_t index = 0; index < m_nodesInTree; index++)
      {
        // We get the first level in the tree
        //uint8_t * ptr = portInfo.greySend.counter_tree[index].max_cells[0];
        fancy_hdr.SetDataMaximums(portInfo.greySend.counter_tree[index].max_cells[0], index);
      }

      // Schedule this to be sent again until the state machine cancells it
      portInfo.greySend.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_ack_wait_time_ms), &P4SwitchFancy::SendGreyAction, this, outPort, action, id);
    }
    else if (action == GREY_START)
    {

      /* WARNING: this is a path for the wildcard dedicated entry as an attempt to skip the START ACK transition */
      /* This is special for the baseline non other state machine should do this!!! */
      if (m_treeEnableSoftDetections & (id == (m_numTopEntries + 1)))
      {
        SetGreyState(portInfo, GreyState::COUNTING, id, false);
        Simulator::Cancel(portInfo.greySend.currentEvent[id]);

        //Schedule the stop event
        double probing_time;
        probing_time = m_probing_time_top_entries_ms;
        portInfo.greySend.currentEvent[id] = Simulator::Schedule(MilliSeconds(probing_time), &P4SwitchFancy::SendGreyAction, this, outPort, GREY_STOP, id);
      }
      /* For all the normal dedicated entries we do the normal transition and wait for the ACK */
      else {
        portInfo.greySend.localCounter[id] = 0;
        SetGreyState(portInfo, GreyState::START_ACK, id, false);
        // Schedule this to be sent again until the state machine cancells it
        portInfo.greySend.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_ack_wait_time_ms), &P4SwitchFancy::SendGreyAction, this, outPort, action, id);
      }
    }
    else if (action == GREY_STOP)
    {
      // Schedule this to be sent again until the state machine cancells it
      SetGreyState(portInfo, GreyState::WAIT_COUNTER_RECEIVE, id, false);
      portInfo.greySend.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_ack_wait_time_ms), &P4SwitchFancy::SendGreyAction, this, outPort, action, id);
      //std::cout << "DEBUG UPSTREAM STOPS COUNTING" << std::endl;
    }

    pkt_info meta(outPort->GetAddress(), portInfo.otherPortDevice->GetAddress(), FANCY);
    meta.headers["FANCY"] = fancy_hdr;
    meta.outPort = outPort;

    //Send packet to traffic manager 
    meta.internalPacket = true;
    TrafficManager(packet, meta);
  }

  void P4SwitchFancy::SendGreyCounter(Ptr<NetDevice> outPort, uint32_t id)
  {
    NS_LOG_FUNCTION_NOARGS();

    uint32_t ifIndex = outPort->GetIfIndex();
    FancyPortInfo& portInfo =
      m_portsInfo[ifIndex];

    Ptr<Packet> packet = Create<Packet>();
    FancyHeader fancy_hdr;

    fancy_hdr.SetAction(GREY_COUNTER);
    fancy_hdr.SetId(id);

    /* Only add when we are sending the zooming structure */
    if (id == m_numTopEntries)
    {
      fancy_hdr.SetAction(GREY_COUNTER | MULTIPLE_COUNTERS);

      fancy_hdr.SetDataWidth(m_counterWidth);
      for (uint16_t index = 0; index < m_nodesInTree; index++)
      {
        //uint32_t * ptr = portInfo.greyRecv.counter_tree[index].counters;
        fancy_hdr.SetDataCounter(portInfo.greyRecv.counter_tree[index].counters, index);
      }
    }

    fancy_hdr.SetSeq(portInfo.greyRecv.currentSEQ[id]);
    fancy_hdr.SetFSMFlag(true); // We want the senders fsm to get the counter
    fancy_hdr.SetCounter(portInfo.greyRecv.localCounter[id]);
    SetGreyState(portInfo, GreyState::COUNTER_ACK, id, true);

    pkt_info meta(outPort->GetAddress(), portInfo.otherPortDevice->GetAddress(), FANCY);
    meta.headers["FANCY"] = fancy_hdr;
    meta.outPort = outPort;

    //Schedule again 
    portInfo.greyRecv.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_ack_wait_time_ms), &P4SwitchFancy::SendGreyCounter, this, outPort, id);

    // Send packet to the traffic manager 
    meta.internalPacket = true;
    TrafficManager(packet, meta);
  }

  void P4SwitchFancy::PacketCounting(GreyInfo& greyPortInfo, uint32_t id, pkt_info& meta, bool sender)
  {
    // Increase counter 
    greyPortInfo.localCounter[id]++;

    /* Non Pipelined More Basic Zooming */

    // indexes obtained from the hash function
    uint32_t counter_index, bloom_filter_index;
    // index that points the counter in the flat tree array (not the depth)
    uint32_t counter_tree_index = 0;

    if (sender && meta.str_flow == "")
    {
      meta.str_flow = (*m_packet_str_funct)(meta.flow);
    }

    // Get zoom phase
    uint32_t zoom_phase = greyPortInfo.currentSEQ[id] % m_treeDepth;

    /* If we have to do something at the root of the tree */
    if (zoom_phase == 0)
    {
      // Root algorithm
      counter_index = (this->*m_packet_hash)(meta.flow, 0, m_counterWidth);
      // Update counter
      greyPortInfo.counter_tree[counter_tree_index].counters[counter_index]++;
      if (sender)
      {
        greyPortInfo.counter_tree[counter_tree_index].last_flow[counter_index] = meta.flow;
        if (greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index].count(meta.str_flow) == 0)
        {
          greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index][meta.str_flow] = meta.flow;
        }
      }
      /* Set bloom filter & count hit*/
      bloom_filter_index = (this->*m_packet_hash)(meta.flow, (0 + m_treeDepth), m_counterBloomFilterWidth);
      greyPortInfo.counter_tree[counter_tree_index].bloom_filter[counter_index].set(bloom_filter_index);
    }
    else /* other layers, we get the address if sequence of hashes, or pass otherwise */
    {
      // Go to lower layers when not pipelined, here we find the index of the node to update
      counter_tree_index = 0;
      for (uint8_t tree_level = 0; tree_level < zoom_phase; tree_level++)
      {
        // Check if the hash is any of the max if so get the address
        counter_index = (this->*m_packet_hash)(meta.flow, tree_level, m_counterWidth);
        bool zoom = false;
        for (uint32_t ii = 0; ii < greyPortInfo.counter_tree[counter_tree_index].max_cells[0].size(); ii++)
        {
          if (greyPortInfo.counter_tree[counter_tree_index].max_cells[0][ii] == counter_index)
          {
            counter_tree_index = (counter_tree_index * m_layerSplit) + (ii + 1);
            zoom = true;
            break;
          }
        }
        // Stop zooming due to the fact that this flow is not being tracked
        if (!zoom)
        {
          return;
        }
      }

      /*If we are here it means we found a hash path until the zoom phase. If so we update the counter*/
      counter_index = (this->*m_packet_hash)(meta.flow, zoom_phase, m_counterWidth);
      greyPortInfo.counter_tree[counter_tree_index].counters[counter_index]++;
      if (sender)
      {
        greyPortInfo.counter_tree[counter_tree_index].last_flow[counter_index] = meta.flow;
        if (greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index].count(meta.str_flow) == 0)
        {
          greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index][meta.str_flow] = meta.flow;
        }
      }
      // Set bloom filter & count hit
      bloom_filter_index = (this->*m_packet_hash)(meta.flow, ((zoom_phase)+m_treeDepth), m_counterBloomFilterWidth);
      greyPortInfo.counter_tree[counter_tree_index].bloom_filter[counter_index].set(bloom_filter_index);
    }
  }

  void P4SwitchFancy::PipelinedPacketCounting(GreyInfo& greyPortInfo, uint32_t id, pkt_info& meta, bool sender)
  {
    // Increase counter
    greyPortInfo.localCounter[id]++;

    // Add new algorithm 
    /////*
    // Run the tree GreyState::COUNTING algorithm

    // indexes obtained from the hash function
    uint32_t counter_index, bloom_filter_index;
    // index that points the counter in the flat tree array (not the depth)
    uint32_t counter_tree_index = 0;

    if (sender && meta.str_flow == "")
    {
      meta.str_flow = (*m_packet_str_funct)(meta.flow);
    }

    // Root algorithm
    counter_index = (this->*m_packet_hash)(meta.flow, 0, m_counterWidth);
    // Update counter
    greyPortInfo.counter_tree[counter_tree_index].counters[counter_index]++;
    if (sender)
    {
      greyPortInfo.counter_tree[counter_tree_index].last_flow[counter_index] = meta.flow;

      if (greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index].count(meta.str_flow) == 0)
      {
        greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index][meta.str_flow] = meta.flow;
      }
    }

    // Set bloom filter & count hit
    bloom_filter_index = (this->*m_packet_hash)(meta.flow, (0 + m_treeDepth), m_counterBloomFilterWidth);
    greyPortInfo.counter_tree[counter_tree_index].bloom_filter[counter_index].set(bloom_filter_index);

    // Lower layers and pipelining 
    for (uint8_t history_level = 0; history_level < m_treeDepth - 1; history_level++)
    {
      // Root
      counter_tree_index = 0;

      // Go down through the tree
      for (uint8_t tree_level = 0; tree_level <= history_level; tree_level++)
      {
        // Check if the hash is any of the max if so get the address
        counter_index = (this->*m_packet_hash)(meta.flow, tree_level, m_counterWidth);
        bool zoom = false;
        for (uint32_t ii = 0; ii < greyPortInfo.counter_tree[counter_tree_index].max_cells[history_level - tree_level].size(); ii++)
        {
          if (greyPortInfo.counter_tree[counter_tree_index].max_cells[history_level - tree_level][ii] == counter_index)
          {
            counter_tree_index = (counter_tree_index * m_layerSplit) + (ii + 1);
            zoom = true;
            break;
          }
        }

        // Stop zooming
        if (!zoom)
        {
          break;
        }

        // If next level is the last one
        if (tree_level == history_level)
        {
          // Update counter
          counter_index = (this->*m_packet_hash)(meta.flow, tree_level + 1, m_counterWidth);
          greyPortInfo.counter_tree[counter_tree_index].counters[counter_index]++;
          if (sender)
          {
            greyPortInfo.counter_tree[counter_tree_index].last_flow[counter_index] = meta.flow;

            if (greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index].count(meta.str_flow) == 0)
            {
              greyPortInfo.counter_tree[counter_tree_index].hashed_flows[counter_index][meta.str_flow] = meta.flow;
            }
          }
          // Set bloom filter & count hit
          bloom_filter_index = (this->*m_packet_hash)(meta.flow, ((tree_level + 1) + m_treeDepth), m_counterBloomFilterWidth);
          greyPortInfo.counter_tree[counter_tree_index].bloom_filter[counter_index].set(bloom_filter_index);
        }
      }
    }
  }

  void P4SwitchFancy::InitReceiverStep(FancyPortInfo& portInfo, uint32_t id, uint16_t length, Buffer::Iterator& start, bool shift_history)
  {

    // Get zoom phase
    uint32_t zoom_phase = portInfo.greyRecv.currentSEQ[id] % m_treeDepth;

    /* Update all the maximum. Note: In theory we should only
       do it for nodes at zoom_phase-1
    */
    uint8_t max_counter;
    uint16_t i;
    /* we only shift the max history the first time we receive the Init signal */
    if (shift_history)
    {
      for (uint16_t t = 0; t < length; t++)
      {
        /* Update maxes for this node */
        i = start.ReadU16();
        for (uint8_t j = 0; j < m_layerSplit; j++)
        {
          max_counter = start.ReadU8();
          portInfo.greyRecv.counter_tree[i].max_cells[0][j] = max_counter;
        }
      }
    }

    // Reset State for this level
    if (zoom_phase == 0)
    {
      for (uint32_t i = 0; i < m_nodesInTree; i++)
      {
        /* When we start at the tree root we erase everything*/
        std::fill(portInfo.greyRecv.counter_tree[i].counters.begin(), portInfo.greyRecv.counter_tree[i].counters.end(), 0);
        for (uint32_t j = 0; j < m_counterWidth; j++)
        {
          portInfo.greyRecv.counter_tree[i].bloom_filter[j].reset();
        }
      }
    }
  }

  void P4SwitchFancy::PipelinedInitReceiverStep(FancyPortInfo& portInfo, uint32_t id, uint16_t length, Buffer::Iterator& start, bool shift_history)
  {

    /* we only shift the max history the first time we receive the Init signal */
    if (shift_history)
    {
      // Copyes counters max and shifts the history
      uint8_t max_counter;
      uint16_t i;
      for (uint16_t t = 0; t < length; t++)
      {
        // Tree counter index
        i = start.ReadU16();
        // Get max counters and shift register
        for (int index = m_treeDepth - 2; index >= 0; index--)
        {
          // Right shift
          for (uint8_t j = 0; j < m_layerSplit; j++)
          {
            portInfo.greyRecv.counter_tree[i].max_cells[index + 1][j] = portInfo.greyRecv.counter_tree[i].max_cells[index][j];
          }
        }
        // Set the last history
        for (uint8_t j = 0; j < m_layerSplit; j++)
        {
          max_counter = start.ReadU8();
          portInfo.greyRecv.counter_tree[i].max_cells[0][j] = max_counter;
        }
      }
    }

    // Reset all the counter state
    for (uint32_t i = 0; i < m_nodesInTree; i++)
    {
      std::fill(portInfo.greyRecv.counter_tree[i].counters.begin(), portInfo.greyRecv.counter_tree[i].counters.end(), 0);
      for (uint32_t j = 0; j < m_counterWidth; j++)
      {
        portInfo.greyRecv.counter_tree[i].bloom_filter[j].reset();
      }
    }
  }

  void P4SwitchFancy::PipelinedCounterExchangeAlgorithm(FancyPortInfo& inPortInfo, uint32_t id, FancyHeader& fancy_hdr)
  {
    /* Get initial data*/
    uint16_t seq = fancy_hdr.GetSeq();
    uint64_t counter = fancy_hdr.GetCounter();

    _NS_LOG_DEBUG("Debugging Info: sw->" << m_name << " step->" << int(seq) << std::endl, m_enableDebug);
    // Report the link's packet loss
    uint64_t local_counter = inPortInfo.greySend.localCounter[id];

    _NS_LOG_DEBUG("Link packet loss is: " << (1 - float(counter) / float(local_counter)) * 100
      << "% (" << counter << "/" << local_counter << ")" << std::endl, m_enableDebug);

    // Reset counters
    inPortInfo.greySend.localCounter[id] = 0;
    SetGreyState(inPortInfo, GreyState::IDLE, id, false);
    inPortInfo.greySend.currentSEQ[id]++;

    /* Set current step*/
    m_simState->SetSimulationStep(Simulator::Now().GetSeconds(), seq, local_counter, local_counter - counter);

    /*
    *
    COUNTER EXCHANGE ALGORITHM
    *
    */
    //std::cout << Simulator::Now().GetSeconds() << " " << "stupid test" << m_name << std::endl;
    // Get all the tree counters, compute maximums, and shift
    uint16_t length = fancy_hdr.GetDataSize();
    Buffer::Iterator start = fancy_hdr.GetData();
    // Copyes counters max and shifts the history
    uint32_t remote_counter;
    uint32_t cell_local_counter;
    uint16_t i;
    uint32_t counter_diff;
    double packet_loss;
    uint32_t flow_counter = 0;
    uint32_t loss_counter = 0;
    int last_layer_index = 0;

    if (m_layerSplit == 1) /* code to support 1 cell zoomings */
    {
      last_layer_index = m_treeDepth - 1;
    }
    else
    {
      last_layer_index = (pow(m_layerSplit, m_treeDepth - 1) - 1) / (m_layerSplit - 1);
    }

    for (uint16_t t = 0; t < length; t++)
    {
      // Tree counter index
      i = start.ReadU16();
      //std::cout << "TREE INDEX " << int(i) << std::endl;
      // Shift max counter history
      for (int index = m_treeDepth - 2; index >= 0; index--)
      {
        // Right shift
        for (uint8_t j = 0; j < m_layerSplit; j++)
        {
          inPortInfo.greySend.counter_tree[i].max_cells[index + 1][j] = inPortInfo.greySend.counter_tree[i].max_cells[index][j];
        }
      }
      /* Clean first level */
      std::fill(inPortInfo.greySend.counter_tree[i].max_cells[0].begin(), inPortInfo.greySend.counter_tree[i].max_cells[0].end(), 0);
      //std::vector<uint32_t> current_max (m_layerSplit);
      std::vector<std::pair<uint32_t, double>> current_max;

      /* Add Tree Node & and sets index*/
      m_simState->SetSimulationTreeNode(i);

      flow_counter = 0;
      loss_counter = 0;
      // Computes new maximums for this tree node
      _NS_LOG_DEBUG("Tree: " << int(i) << " " << KArryTreeDepth(m_layerSplit, i) << std::endl, m_enableDebug);

      /* We count the number of faulty cells*/
      uint32_t faulty_cells = 0;

      // for counter in all buckets
      for (uint8_t j = 0; j < m_counterWidth; j++)
      {
        remote_counter = start.ReadU32();
        cell_local_counter = inPortInfo.greySend.counter_tree[i].counters[j];
        counter_diff = cell_local_counter - remote_counter;
        if (counter_diff != 0)
        {
          faulty_cells++;
          packet_loss = double(counter_diff) / cell_local_counter;
        }
        else
        {
          packet_loss = 0;
        }

        /* Cost of the system: right now either absolute or relative lost*/
        double cost;
        switch (CostTypes(m_costType))
        {
        case CostTypes::ABSOLUTE:
          cost = double(counter_diff);
          break;
        case CostTypes::RELATIVE:
          cost = packet_loss;
          break;
        }

        loss_counter += counter_diff;
        // Check if there is loss, and 1 in the hit marker, if so we add an entry to the blooom filter
        // only check if we are in the last layer of the tree
        // before hit_counter[j]

        /* Save Tree node main state */
        m_simState->SetCounterValues(cell_local_counter, remote_counter, inPortInfo.greySend.counter_tree[i].bloom_filter[j].count(),
          inPortInfo.greySend.counter_tree[i].hashed_flows[j].size(), inPortInfo.greySend.counter_tree[i].bloom_filter[j]);

        /* If we are at the last layer... we start doing the magic */
        if (i >= last_layer_index && cost > m_rerouteMinCost && inPortInfo.greySend.counter_tree[i].bloom_filter[j].count() <= m_maxCounterCollisions)
        {
          uint16_t child_address = i;
          char hash_path[m_treeDepth];
          hash_path[m_treeDepth - 1] = j;
          // we go through the tree going up
          // the algorithm we use to get the addresses can be found here:
          // https://en.wikipedia.org/wiki/M-ary_tree
          for (uint32_t t = 0; t < m_treeDepth - 1; t++)
          {
            uint16_t parent_address = (child_address - 1) / m_layerSplit;
            uint16_t child_shift = (child_address - (m_layerSplit * parent_address)) - 1;
            uint16_t hash_index = inPortInfo.greySend.counter_tree[parent_address].max_cells[t + 1][child_shift];
            hash_path[m_treeDepth - (t + 2)] = hash_index;
            child_address = parent_address;
          }


          std::cout << "\n\033[1;34m# Failure Detected(" << inPortInfo.link_name << ")" << std::endl;
          std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
          std::cout << "Path: ";
          for (uint32_t a = 0; a < m_treeDepth; a++)
          {
            std::cout << int(hash_path[a]) << " ";
          }
          std::cout << std::endl;

          /* Set element in the main bloom filter */
          uint32_t bloom_filter_indexes[m_rerouteBloomFilterNumHashes];
          SetBloomFilter(inPortInfo, hash_path, bloom_filter_indexes);

          std::cout << ("BF Indexes: ");
          for (uint32_t bf_index = 0; bf_index < m_rerouteBloomFilterNumHashes; bf_index++)
          {
            std::cout << bloom_filter_indexes[bf_index] << " ";
          }
          std::cout << std::endl;

          std::cout << "Drops: " << counter_diff << std::endl;
          std::cout << "Loss: " << packet_loss << std::endl;
          std::cout << "BF Collisions: " << inPortInfo.greySend.counter_tree[i].bloom_filter[j].count() << std::endl;
          std::cout << "Real Collisions: " << inPortInfo.greySend.counter_tree[i].hashed_flows[j].size() << std::endl;
          if (m_packet_hash_type == "FiveTupleHash")
          {
            std::cout << "Flows:" << std::endl;
            for (auto it = inPortInfo.greySend.counter_tree[i].hashed_flows[j].begin(); it != inPortInfo.greySend.counter_tree[i].hashed_flows[j].end(); ++it)
            {
              PrintIpFiveTuple(it->second);
            }
          }
          else if (m_packet_hash_type == "DstPrefixHash")
          {
            std::cout << "Prefixes:" << std::endl;
            for (auto it = inPortInfo.greySend.counter_tree[i].hashed_flows[j].begin(); it != inPortInfo.greySend.counter_tree[i].hashed_flows[j].end(); ++it)
            {
              PrintDstPrefix("", it->second);
            }
          }

          inPortInfo.failures_count++;
          std::cout << "Failure number: " << inPortInfo.failures_count << std::endl;
          std::cout << "# End Failure Detected\033[0m" << std::endl << std::endl;

          /* Add failure detection event to the sim data*/
          m_simState->SetFailureEvent(Simulator::Now().GetSeconds(), hash_path, bloom_filter_indexes, inPortInfo.greySend.counter_tree[i].hashed_flows[j],
            inPortInfo.greySend.counter_tree[i].bloom_filter[j].count(), cell_local_counter, remote_counter, id, inPortInfo.failures_count);

          /* TODO probably remove (there are 3 more)
          /* Early stop simulation */
          if (m_early_stop_counter > 0 && inPortInfo.failures_count == m_early_stop_counter)
          {
            std::cout << "EARLY STOP SIMULATION (All failure detected pipelined)" << std::endl;
            Simulator::Stop(MilliSeconds(m_early_stop_delay_ms));
          }

        }
        //collision
        else if (i >= last_layer_index && cost > m_rerouteMinCost && inPortInfo.greySend.counter_tree[i].bloom_filter[j].count() > m_maxCounterCollisions)
        {
          /* We indicate that in this cell there is more "entries" than maxCounterCollisions*/
          m_simState->SetCollisionEvent(seq, i, j, inPortInfo.greySend.counter_tree[i].bloom_filter[j].count());
          //std::cout << "# Too many flows hashed at the last layer: node=" << int(i) << " depth=" << KArryTreeDepth(m_layerSplit, i) 
          //<< " cell=" << int(j) << " collisions=" <<  << j << std::endl;
        }
        // Here we log partial tree failures 
        else if (i < last_layer_index && cost > m_rerouteMinCost)
        {
          /* adds extra info for early detections */
          if (m_treeEnableSoftDetections)
          {
            // Here we will do some special loging for events that are partial. 
            uint16_t child_address = i;
            uint32_t current_depth = uint32_t(KArryTreeDepth(m_layerSplit, i)) + 1;
            char hash_path[current_depth];
            hash_path[m_treeDepth] = j;
            // we go through the tree going up
            // the algorithm we use to get the addresses can be found here:
            // https://en.wikipedia.org/wiki/M-ary_tree
            for (uint32_t t = 0; t < current_depth - 1; t++)
            {
              uint16_t parent_address = (child_address - 1) / m_layerSplit;
              uint16_t child_shift = (child_address - (m_layerSplit * parent_address)) - 1;
              uint16_t hash_index = inPortInfo.greySend.counter_tree[parent_address].max_cells[t + 1][child_shift];
              hash_path[m_treeDepth - (t + 2)] = hash_index;
              child_address = parent_address;
            }

            //std::cout << "\n\033[1;34m# Soft Failure Detected(" << inPortInfo.link_name << ")" << std::endl;
            //std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
            //std::cout << "Path: ";
            //std::cout << "Drops: " << counter_diff << std::endl;
            //std::cout << "Loss: " << packet_loss << std::endl;
            //std::cout << "BF Collisions: " << inPortInfo.greySend.counter_tree[i].bloom_filter[j].count() << std::endl;
            //std::cout << "Real Collisions: " << inPortInfo.greySend.counter_tree[i].hashed_flows[j].size() << std::endl
            //std::cout << "# End Soft Failure Detected\033[0m" << std::endl << std::endl;

            /* Add failure detection event to the sim data*/
            m_simState->SetSoftFailureEvent(Simulator::Now().GetSeconds(), 1, hash_path, inPortInfo.greySend.counter_tree[i].hashed_flows[j],
              inPortInfo.greySend.counter_tree[i].bloom_filter[j].count(), cell_local_counter, remote_counter, id, current_depth);
          }
        }

        if (j != 0 && j % 8 == 0)
        {
          _NS_LOG_DEBUG(std::endl, m_enableDebug);
        }
        if (cost > 0)
        {
          _NS_LOG_DEBUG("\033[1;31m|" << std::setw(2) << int(j) << ":" << std::setw(6) << std::setprecision(4) << cost << ":" << std::setw(2) << int(inPortInfo.greySend.counter_tree[i].bloom_filter[j].count()) << "|\033[0m ", m_enableDebug);
        }
        else
        {
          _NS_LOG_DEBUG("|" << std::setw(2) << int(j) << ":" << std::setw(6) << std::setprecision(4) << cost << ":" << std::setw(2) << int(inPortInfo.greySend.counter_tree[i].bloom_filter[j].count()) << "| ", m_enableDebug);
        }
        flow_counter += inPortInfo.greySend.counter_tree[i].bloom_filter[j].count();

        /* New max method: we do it with std:: due to a lack of time */
        current_max.push_back(std::make_pair(j, cost));
      }

      /* check if a big number of entries are experiencing problems */
      /* We only do it for the first layer of the tree */
      //if (i == 0)
      //{
      //  std::cout << "Faulty entries " << i << " " << faulty_cells << " " << m_uniformLossThreshold << std::endl;
      //}
      // check if we are at the first layer and faulty cells is > threshold
      //if (m_uniformLossThreshold > 0 && i == 0 )
      //{
      //  std::cout << "Faulty cells " << faulty_cells << std::endl;
      //}
      if (m_uniformLossThreshold > 0 && i == 0 && faulty_cells >= m_uniformLossThreshold)
      {
        std::cout << "\n\033[1;34m# Uniform Failure Detected!!!(" << inPortInfo.link_name << ")" << std::endl;
        std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
        std::cout << "Faulty cells: " << faulty_cells << std::endl;
        std::cout << "# End Uniform Failure Detected\033[0m" << std::endl << std::endl;

        /* Set event such that we can parse it from files */
        m_simState->SetUniformFailureEvent(Simulator::Now().GetSeconds(), seq, faulty_cells);
        std::cout << "STOP SIMULATION" << std::endl;
        // IS THIS A GOOD IDEA?
        Simulator::Stop();
      }

      ///* Set new maximums */
      if (!m_pipelineBoost)
      {
        std::sort(current_max.begin(), current_max.end(), sortbysec);
        for (uint32_t jj = 0; jj < m_layerSplit; jj++)
        {
          inPortInfo.greySend.counter_tree[i].max_cells[0][jj] = current_max[jj].first;
        }
      }
      else
      {
        /* Set new maximums: The way it is implemented right now is no P4 friendly at all*/
        std::sort(current_max.begin(), current_max.end(), sortbysec);
        std::vector<uint8_t> backup;
        uint8_t max_found = 0;
        for (uint32_t jj = 0; jj < current_max.size(); jj++)
        {
          /* Checks if the index was not in the past 2 histories */
          if ((current_max[jj].second > 0) &&
            (std::find(inPortInfo.greySend.counter_tree[i].max_cells[1].begin(), inPortInfo.greySend.counter_tree[i].max_cells[1].end(), current_max[jj].first) == inPortInfo.greySend.counter_tree[i].max_cells[1].end()) &&
            (std::find(inPortInfo.greySend.counter_tree[i].max_cells[2].begin(), inPortInfo.greySend.counter_tree[i].max_cells[2].end(), current_max[jj].first) == inPortInfo.greySend.counter_tree[i].max_cells[2].end()))
          {
            inPortInfo.greySend.counter_tree[i].max_cells[0][max_found] = current_max[jj].first;
            max_found++;
            if (max_found == m_layerSplit)
            {
              break;
            }
          }
          else
          {
            backup.push_back(current_max[jj].first);
          }

        }
        //std::cout << int(max_found) << " " << backup.size() << " " << current_max.size() << std::endl;
        for (uint32_t jj = 0; max_found < m_layerSplit; jj++, max_found++)
        {
          inPortInfo.greySend.counter_tree[i].max_cells[0][max_found] = backup[jj];
        }
        /* end max*/
      }

      /* new maximums */
      _NS_LOG_DEBUG(std::endl, m_enableDebug);

      /* Some nice info */
      // Counter of all number of flows
      _NS_LOG_DEBUG("Total Flows Found: " << flow_counter << std::endl, m_enableDebug);
      // Counter of dropped packets
      _NS_LOG_DEBUG("Total Packets Dropped: " << loss_counter << std::endl, m_enableDebug);
      // Sets the new maximums
      _NS_LOG_DEBUG("New max indexes: ", m_enableDebug);
      for (uint8_t jj = 0; jj < m_layerSplit; jj++)
      {
        _NS_LOG_DEBUG(int(inPortInfo.greySend.counter_tree[i].max_cells[0][jj]) << " ", m_enableDebug);
        //inPortInfo.greySend.counter_tree[i].max_cells[0][jj] = inPortInfo.greySend.counter_tree[i].max_cells[0][jj];
      }

      m_simState->SetMaxHistory(inPortInfo.greySend.counter_tree[i].max_cells);

      /* DEBUG*/
      _NS_LOG_DEBUG(std::endl, m_enableDebug);
      // print local history
      _NS_LOG_DEBUG("History:", m_enableDebug);
      for (uint8_t index = 0; index < m_treeDepth; index++)
      {
        _NS_LOG_DEBUG("(", m_enableDebug);
        for (uint8_t j = 0; j < m_layerSplit; j++)
        {
          // defined macro to aboid std::endl                      
          _NS_LOG_DEBUG(int(inPortInfo.greySend.counter_tree[i].max_cells[index][j]) << " ", m_enableDebug);
        }
        _NS_LOG_DEBUG("\b)", m_enableDebug);
      }
      _NS_LOG_DEBUG("\n" << std::endl, m_enableDebug);
    }

    // Reset all the counter state
    for (uint32_t i = 0; i < m_nodesInTree; i++)
    {
      std::fill(inPortInfo.greySend.counter_tree[i].counters.begin(), inPortInfo.greySend.counter_tree[i].counters.end(), 0);

      for (uint32_t j = 0; j < m_counterWidth; j++)
      {
        inPortInfo.greySend.counter_tree[i].bloom_filter[j].reset();
        inPortInfo.greySend.counter_tree[i].hashed_flows[j].clear();
      }
    }
  }

  void P4SwitchFancy::CounterExchangeAlgorithm(FancyPortInfo& inPortInfo, uint32_t id, FancyHeader& fancy_hdr)
  {

    /* Get initial data*/
    uint16_t seq = fancy_hdr.GetSeq();
    uint8_t zoom_phase = seq % m_treeDepth;
    uint8_t next_zoom_phase = (seq + 1) % m_treeDepth;

    uint64_t counter = fancy_hdr.GetCounter();

    _NS_LOG_DEBUG("Debugging Info: sw->" << m_name << " step->" << int(seq) << std::endl, m_enableDebug);
    // Report the link's packet loss
    uint64_t local_counter = inPortInfo.greySend.localCounter[id];

    _NS_LOG_DEBUG("Link packet loss is: " << (1 - float(counter) / float(local_counter)) * 100
      << "% (" << counter << "/" << local_counter << ")" << std::endl, m_enableDebug);

    // Reset counters
    inPortInfo.greySend.localCounter[id] = 0;
    SetGreyState(inPortInfo, GreyState::IDLE, id, false);
    inPortInfo.greySend.currentSEQ[id]++;

    /* Set current step*/
    m_simState->SetSimulationStep(Simulator::Now().GetSeconds(), seq, local_counter, local_counter - counter);

    /*
    *
    COUNTER EXCHANGE ALGORITHM
    *
    */
    //std::cout << Simulator::Now().GetSeconds() << " " << "stupid test" << m_name << std::endl;
    // Get all the tree counters, compute maximums, and shift
    uint16_t length = fancy_hdr.GetDataSize();
    Buffer::Iterator start = fancy_hdr.GetData();
    // Copyes counters max and shifts the history
    uint32_t remote_counter;
    uint32_t cell_local_counter;
    uint16_t i;
    uint32_t counter_diff;
    double packet_loss;
    uint32_t flow_counter = 0;
    uint32_t loss_counter = 0;
    int last_layer_index = 0;


    if (m_layerSplit == 1) /* code to support 1 cell zoomings */
    {
      last_layer_index = m_treeDepth - 1;
    }
    else
    {
      last_layer_index = (pow(m_layerSplit, m_treeDepth - 1) - 1) / (m_layerSplit - 1);
    }

    for (uint16_t t = 0; t < length; t++)
    {
      // Tree counter index
      i = start.ReadU16();

      /* Clean first level */
      std::fill(inPortInfo.greySend.counter_tree[i].max_cells[0].begin(), inPortInfo.greySend.counter_tree[i].max_cells[0].end(), 0);
      //std::vector<uint32_t> current_max (m_layerSplit);
      std::vector<std::pair<uint32_t, double>> current_max;

      /* Add Tree Node & and sets index*/
      m_simState->SetSimulationTreeNode(i);

      flow_counter = 0;
      loss_counter = 0;
      // Computes new maximums for this tree node
      _NS_LOG_DEBUG("Tree: " << int(i) << " " << KArryTreeDepth(m_layerSplit, i) << std::endl, m_enableDebug);
      for (uint8_t j = 0; j < m_counterWidth; j++)
      {
        remote_counter = start.ReadU32();
        cell_local_counter = inPortInfo.greySend.counter_tree[i].counters[j];
        counter_diff = cell_local_counter - remote_counter;
        if (counter_diff != 0)
        {
          packet_loss = double(counter_diff) / cell_local_counter;
        }
        else
        {
          packet_loss = 0;
        }

        /* Cost of the system: right now either absolute or relative lost*/
        double cost;
        switch (CostTypes(m_costType))
        {
        case CostTypes::ABSOLUTE:
          cost = double(counter_diff);
          break;
        case CostTypes::RELATIVE:
          cost = packet_loss;
          break;
        }

        loss_counter += counter_diff;
        // Check if there is loss, and 1 in the hit marker, if so we add an entry to the blooom filter
        // only check if we are in the last layer of the tree
        // before hit_counter[j]

        /* Save Tree node main state */
        m_simState->SetCounterValues(cell_local_counter, remote_counter, inPortInfo.greySend.counter_tree[i].bloom_filter[j].count(),
          inPortInfo.greySend.counter_tree[i].hashed_flows[j].size(), inPortInfo.greySend.counter_tree[i].bloom_filter[j]);

        if (i >= last_layer_index && cost > m_rerouteMinCost && inPortInfo.greySend.counter_tree[i].bloom_filter[j].count() <= m_maxCounterCollisions)
        {
          uint16_t child_address = i;
          char hash_path[m_treeDepth];
          hash_path[m_treeDepth - 1] = j;
          // we go through the tree going up
          // the algorithm we use to get the addresses can be found here:
          // https://en.wikipedia.org/wiki/M-ary_tree
          for (uint32_t t = 0; t < m_treeDepth - 1; t++)
          {
            uint16_t parent_address = (child_address - 1) / m_layerSplit;
            uint16_t child_shift = (child_address - (m_layerSplit * parent_address)) - 1;
            uint16_t hash_index = inPortInfo.greySend.counter_tree[parent_address].max_cells[0][child_shift];
            hash_path[m_treeDepth - (t + 2)] = hash_index;
            child_address = parent_address;
          }

          std::cout << "\n\033[1;34m# Failure Detected(" << inPortInfo.link_name << ")" << std::endl;
          std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
          std::cout << "Path: ";
          for (uint32_t a = 0; a < m_treeDepth; a++)
          {
            std::cout << int(hash_path[a]) << " ";

          }
          std::cout << std::endl;

          /* Set element in the main bloom filter */
          uint32_t bloom_filter_indexes[m_rerouteBloomFilterNumHashes];
          SetBloomFilter(inPortInfo, hash_path, bloom_filter_indexes);

          std::cout << ("BF Indexes: ");
          for (uint32_t bf_index = 0; bf_index < m_rerouteBloomFilterNumHashes; bf_index++)
          {
            std::cout << bloom_filter_indexes[bf_index] << " ";
          }
          std::cout << std::endl;

          std::cout << "Drops: " << counter_diff << std::endl;
          std::cout << "Loss: " << packet_loss << std::endl;
          std::cout << "BF Collisions: " << inPortInfo.greySend.counter_tree[i].bloom_filter[j].count() << std::endl;
          std::cout << "Real Collisions: " << inPortInfo.greySend.counter_tree[i].hashed_flows[j].size() << std::endl;
          if (m_packet_hash_type == "FiveTupleHash")
          {
            std::cout << "Flows:" << std::endl;
            for (auto it = inPortInfo.greySend.counter_tree[i].hashed_flows[j].begin(); it != inPortInfo.greySend.counter_tree[i].hashed_flows[j].end(); ++it)
            {
              PrintIpFiveTuple(it->second);
            }
          }
          else if (m_packet_hash_type == "DstPrefixHash")
          {
            std::cout << "Prefixes:" << std::endl;
            for (auto it = inPortInfo.greySend.counter_tree[i].hashed_flows[j].begin(); it != inPortInfo.greySend.counter_tree[i].hashed_flows[j].end(); ++it)
            {
              PrintDstPrefix("", it->second);
            }
          }
          inPortInfo.failures_count++;
          std::cout << "Failure number: " << inPortInfo.failures_count << std::endl;
          std::cout << "# End Failure Detected\033[0m" << std::endl << std::endl;

          /* Add failure detection event to the sim data*/
          m_simState->SetFailureEvent(Simulator::Now().GetSeconds(), hash_path, bloom_filter_indexes, inPortInfo.greySend.counter_tree[i].hashed_flows[j],
            inPortInfo.greySend.counter_tree[i].bloom_filter[j].count(), cell_local_counter, remote_counter, id, inPortInfo.failures_count);
          /* TODO probably remove (there are 3 more)
          /* Early stop simulation */
          if (m_early_stop_counter > 0 && inPortInfo.failures_count == m_early_stop_counter)
          {
            std::cout << "EARLY STOP SIMULATION (all failure detected)" << std::endl;
            Simulator::Stop(MilliSeconds(m_early_stop_delay_ms));
          }


        }
        else if (i >= last_layer_index && cost > m_rerouteMinCost && inPortInfo.greySend.counter_tree[i].bloom_filter[j].count() > m_maxCounterCollisions)
        {

          m_simState->SetCollisionEvent(seq, i, j, inPortInfo.greySend.counter_tree[i].bloom_filter[j].count());
          //std::cout << "# Too many flows hashed at the last layer: node=" << int(i) << " depth=" << KArryTreeDepth(m_layerSplit, i) 
          //<< " cell=" << int(j) << " collisions=" <<  << j << std::endl;
        }

        if (j != 0 && j % 8 == 0)
        {
          _NS_LOG_DEBUG(std::endl, m_enableDebug);
        }
        if (cost > 0)
        {
          _NS_LOG_DEBUG("\033[1;31m|" << std::setw(2) << int(j) << ":" << std::setw(6) << std::setprecision(4) << cost << ":" << std::setw(2) << int(inPortInfo.greySend.counter_tree[i].bloom_filter[j].count()) << "|\033[0m ", m_enableDebug);
        }
        else
        {
          _NS_LOG_DEBUG("|" << std::setw(2) << int(j) << ":" << std::setw(6) << std::setprecision(4) << cost << ":" << std::setw(2) << int(inPortInfo.greySend.counter_tree[i].bloom_filter[j].count()) << "| ", m_enableDebug);
        }
        flow_counter += inPortInfo.greySend.counter_tree[i].bloom_filter[j].count();

        // checks if the difference is bigger than any of the current maxes if so sets it 
        //for (uint8_t jj=0 ; jj < m_layerSplit; jj++)
        //{
        //  if (counter_diff > current_max[jj])
        //  {
        //    //std::cout << "debug new max: " << counter_diff << " " << current_max[jj] << " " << int(jj) << std::endl;
        //    // Before adding the new maximum we have to shift 
        //    // all the maximums from the index we enter
        //    // This ensures that the list is kept sorted from bigger to smaller values
        //    // this gurantees that we will always have the 3 biggest values
        //    for (int shift_index = m_layerSplit-2; shift_index >= jj; shift_index--)
        //    {
        //      current_max[shift_index+1] = current_max[shift_index];
        //      inPortInfo.greySend.counter_tree[i].max_cells[0][shift_index+1] = inPortInfo.greySend.counter_tree[i].max_cells[0][shift_index];
        //    }
        //    current_max[jj] = counter_diff;
        //    inPortInfo.greySend.counter_tree[i].max_cells[0][jj] = j;
        //    break;
        //  }
        //}

        /* New max method: we do it with std:: due to a lack of time */
        current_max.push_back(std::make_pair(j, cost));
      }

      /* Set new maximums */
      std::sort(current_max.begin(), current_max.end(), sortbysec);
      for (uint32_t jj = 0; jj < m_layerSplit; jj++)
      {
        inPortInfo.greySend.counter_tree[i].max_cells[0][jj] = current_max[jj].first;
      }

      _NS_LOG_DEBUG(std::endl, m_enableDebug);

      /* Some nice info */
      // Counter of all number of flows
      _NS_LOG_DEBUG("Total Flows Found: " << flow_counter << std::endl, m_enableDebug);
      // Counter of dropped packets
      _NS_LOG_DEBUG("Total Packets Dropped: " << loss_counter << std::endl, m_enableDebug);
      // Sets the new maximums
      _NS_LOG_DEBUG("New max indexes: ", m_enableDebug);
      for (uint8_t jj = 0; jj < m_layerSplit; jj++)
      {
        _NS_LOG_DEBUG(int(inPortInfo.greySend.counter_tree[i].max_cells[0][jj]) << " ", m_enableDebug);
        inPortInfo.greySend.counter_tree[i].max_cells[0][jj] = inPortInfo.greySend.counter_tree[i].max_cells[0][jj];
      }

      m_simState->SetMaxHistory(inPortInfo.greySend.counter_tree[i].max_cells);

      /* DEBUG*/
      _NS_LOG_DEBUG(std::endl, m_enableDebug);
      // print local history
      _NS_LOG_DEBUG("History:", m_enableDebug);
      for (uint8_t index = 0; index < m_treeDepth; index++)
      {
        _NS_LOG_DEBUG("(", m_enableDebug);
        for (uint8_t j = 0; j < m_layerSplit; j++)
        {
          // defined macro to aboid std::endl                      
          _NS_LOG_DEBUG(int(inPortInfo.greySend.counter_tree[i].max_cells[index][j]) << " ", m_enableDebug);
        }
        _NS_LOG_DEBUG("\b)", m_enableDebug);
      }
      _NS_LOG_DEBUG("\n" << std::endl, m_enableDebug);
    }

    // Reset all the counter state when we are at the bottom of the tree
    if ((zoom_phase + 1) == m_treeDepth)
    {
      for (uint32_t i = 0; i < m_nodesInTree; i++)
      {
        std::fill(inPortInfo.greySend.counter_tree[i].counters.begin(), inPortInfo.greySend.counter_tree[i].counters.end(), 0);

        for (uint32_t j = 0; j < m_counterWidth; j++)
        {
          inPortInfo.greySend.counter_tree[i].bloom_filter[j].reset();
          inPortInfo.greySend.counter_tree[i].hashed_flows[j].clear();
        }
      }
    }
  }

  void P4SwitchFancy::IngressCounterBox(Ptr<const Packet> packet, pkt_info& meta)
  {

    // Update the receiving port time 
    FancyPortInfo& portInfo = m_portsInfo[meta.inPort->GetIfIndex()];
    portInfo.last_time_received = Simulator::Now();

    /* Wild card counting */
    /*
      For whilecard counting (dedicated counter entry for all traffic) we do
      not look at the count flag. Thus, as long as the state machine is at
      counting state, downstream ingress will count. This is not a problem,
      since the counting flag is just used as a double check but is not
      strictly needed to ensure our state machine correctness
    */

    /* This counts packets for everything, maybe we should only check IPV4 ? */
    if (m_treeEnableSoftDetections)
    {
      if (portInfo.greyRecv.greyState[m_numTopEntries + 1] == GreyState::COUNTING and meta.headers.count("IPV4") > 0)
      {
        portInfo.greyRecv.localCounter[m_numTopEntries + 1]++;
      }
    }

    // If FANCY header is valid we run the state machine
    if (meta.headers.count("FANCY") > 0)
    {
      FancyHeader& fancy_hdr = std::any_cast<FancyHeader&>(meta.headers["FANCY"]);

      /* Receivers State machine */
      uint32_t id = fancy_hdr.GetId();
      bool count_flag = fancy_hdr.GetCountFlag();
      /* Reset the count flag just in case */
      fancy_hdr.SetCountFlag(0);
      bool fsm = fancy_hdr.GetFSMFlag();
      uint16_t seq = fancy_hdr.GetSeq();
      uint8_t action = fancy_hdr.GetAction();
      bool ack_flag = fancy_hdr.GetAckFlag();

      /* we use it to check if its a packet to be dropped */
      uint16_t next_header = fancy_hdr.GetNextHeader();

      /* Simple keep alive packets are dropped here */
      if (action == KEEP_ALIVE && next_header == 0)
      {
        meta.drop_flag;
        meta.outPort = NULL;
        return;
      }


      /* Direct State Machines */
      /* IMPORTANT: we added a special dedicated state machine for all traffic at m_numTopEntries +1 */
      if (id < m_numTopEntries or id == (m_numTopEntries + 1))
      {
        // Counting algorithm */
        if (portInfo.greyRecv.greyState[id] == GreyState::COUNTING and count_flag)
        {
          if (seq == portInfo.greyRecv.currentSEQ[id])
          {
            portInfo.greyRecv.localCounter[id]++;
          }
        }

        // Receivers State Machine
        // Packet needs to be sent to the egress pipeline
        // thus we directly sent it to the input port egress
        if (fsm)
        {
          meta.outPort = meta.inPort;
        }
        else if (action != KEEP_ALIVE && !fsm)
        {
          //Once the action is read we reset it
          fancy_hdr.ResetActionField();

          switch (portInfo.greyRecv.greyState[id])
          {
          case GreyState::IDLE:
            if (action == (GREY_START))
            {
              portInfo.greyRecv.localCounter[id] = 0;
              portInfo.greyRecv.currentSEQ[id] = seq;
              SetGreyState(portInfo, GreyState::COUNTING, id, true);

              // We send an acknowledge back and move to GreyState::COUNTING
              fancy_hdr.SetAction(GREY_START);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(true);

              meta.internalPacket = true;
              meta.outPort = meta.inPort;
            }
            else
            {
              // Drop control packet
              meta.drop_flag = true;
            }
            break;

          case GreyState::COUNTING:
            if (action == (GREY_START)) // the ack was lost
            {
              portInfo.greyRecv.localCounter[id] = 0;
              portInfo.greyRecv.currentSEQ[id] = seq;
              SetGreyState(portInfo, GreyState::COUNTING, id, true);

              // We send an acknowledge back and move to GreyState::COUNTING
              fancy_hdr.SetAction(GREY_START);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(true);

              meta.internalPacket = true;
              meta.outPort = meta.inPort;
            }
            else if (action == GREY_STOP and portInfo.greyRecv.currentSEQ[id] == seq)
            {
              SetGreyState(portInfo, GreyState::WAIT_COUNTER_SEND, id, true);

              // Schedule counter send
              portInfo.greyRecv.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_send_counter_wait_ms), &P4SwitchFancy::SendGreyCounter, this, meta.inPort, id);

              // Drop control packet
              meta.drop_flag = true;
            }
            else
            {
              // Drop control packet
              meta.drop_flag = true;
            }
            break;

          case GreyState::WAIT_COUNTER_SEND:
            // does not listen to anything, if we are here it 
            // means that the packet was not send yet
            // Drop control packet
            meta.drop_flag = true;
            break;

          case GreyState::COUNTER_ACK:

            // Waits for acknolwedgement 2 from the initiators side
            if (action == GREY_COUNTER and ack_flag and seq == portInfo.greyRecv.currentSEQ[id])
            {
              // all good
              Simulator::Cancel(portInfo.greyRecv.currentEvent[id]);
              SetGreyState(portInfo, GreyState::IDLE, id, true);
              meta.drop_flag = true;

            }
            else if (action == (GREY_START))
            {
              portInfo.greyRecv.localCounter[id] = 0;
              portInfo.greyRecv.currentSEQ[id] = seq;
              SetGreyState(portInfo, GreyState::COUNTING, id, true);

              // We send an acknowledge back and move to GreyState::COUNTING
              fancy_hdr.SetAction(GREY_START);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(true);

              // So we do not add the counters when deparsing
              meta.internalPacket = true;
              meta.outPort = meta.inPort;
            }
            else
            {
              // Drop control packet
              meta.drop_flag = true;
            }
            break;
          }
        }
      }
      /* Zooming state machines */
      else if (id == m_numTopEntries)
      {
        // Counting algorithm */
        if (portInfo.greyRecv.greyState[id] == GreyState::COUNTING and count_flag)
        {
          if (seq == portInfo.greyRecv.currentSEQ[id])
          {
            /* Run Main algorithm */
            if (m_pipeline)
            {
              PipelinedPacketCounting(portInfo.greyRecv, id, meta, false);
            }
            else
            {
              PacketCounting(portInfo.greyRecv, id, meta, false);
            }
          }
        }

        // Receivers State Machine

        // Packet needs to be sent to the egress pipeline
        // thus we directly sent it to the input port egress
        if (fsm)
        {
          meta.outPort = meta.inPort;
        }
        else if (action != KEEP_ALIVE && !fsm)
        {
          //Once the action is read we reset it
          fancy_hdr.ResetActionField();

          switch (portInfo.greyRecv.greyState[id])
          {
          case GreyState::IDLE:
            if (action == (GREY_START | COUNTER_MAXIMUMS))
            {
              portInfo.greyRecv.localCounter[id] = 0;
              portInfo.greyRecv.currentSEQ[id] = seq;
              SetGreyState(portInfo, GreyState::COUNTING, id, true);

              /* Init state and update max indexes */
              uint16_t length = fancy_hdr.GetDataSize();
              Buffer::Iterator start = fancy_hdr.GetData();

              if (m_pipeline)
              {
                PipelinedInitReceiverStep(portInfo, id, length, start, true);
              }
              else
              {
                InitReceiverStep(portInfo, id, length, start, true);
              }

              // We send an acknowledge back and move to GreyState::COUNTING
              fancy_hdr.SetAction(GREY_START);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(true);

              // So we do not add the counters when deparsing
              fancy_hdr.SetDataSize(0);
              meta.internalPacket = true;
              meta.outPort = meta.inPort;
            }
            else
            {
              // Drop control packet
              meta.drop_flag = true;
            }
            break;

          case GreyState::COUNTING:
            if (action == (GREY_START | COUNTER_MAXIMUMS)) // the ack was lost
            {
              portInfo.greyRecv.localCounter[id] = 0;
              portInfo.greyRecv.currentSEQ[id] = seq;
              SetGreyState(portInfo, GreyState::COUNTING, id, true);

              // TODO What do we do here?????
              /* Init state and update max indexes */
              uint16_t length = fancy_hdr.GetDataSize();
              Buffer::Iterator start = fancy_hdr.GetData();
              if (m_pipeline)
              {
                PipelinedInitReceiverStep(portInfo, id, length, start, false);
              }
              else
              {
                InitReceiverStep(portInfo, id, length, start, false);
              }

              // We send an acknowledge back and move to GreyState::COUNTING
              fancy_hdr.SetAction(GREY_START);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(true);

              // So we do not add the counters when deparsing
              fancy_hdr.SetDataSize(0);
              meta.internalPacket = true;
              meta.outPort = meta.inPort;
            }
            else if (action == GREY_STOP and portInfo.greyRecv.currentSEQ[id] == seq)
            {
              SetGreyState(portInfo, GreyState::WAIT_COUNTER_SEND, id, true);

              // Schedule counter send
              portInfo.greyRecv.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_send_counter_wait_ms), &P4SwitchFancy::SendGreyCounter, this, meta.inPort, id);

              // Drop control packet
              meta.drop_flag = true;
            }
            else
            {
              // Drop control packet
              meta.drop_flag = true;
            }
            break;

          case GreyState::WAIT_COUNTER_SEND:
            // does not listen to anything, if we are here it 
            // means that the packet was not send yet
            // Drop control packet
            meta.drop_flag = true;
            break;

          case GreyState::COUNTER_ACK:

            // Waits for acknolwedgement 2 from the initiators side
            if (action == GREY_COUNTER and ack_flag and seq == portInfo.greyRecv.currentSEQ[id])
            {
              // all good
              Simulator::Cancel(portInfo.greyRecv.currentEvent[id]);
              SetGreyState(portInfo, GreyState::IDLE, id, true);
              meta.drop_flag = true;

            }
            else if (action == (GREY_START | COUNTER_MAXIMUMS))
            {
              portInfo.greyRecv.localCounter[id] = 0;
              portInfo.greyRecv.currentSEQ[id] = seq;
              SetGreyState(portInfo, GreyState::COUNTING, id, true);

              /* Init state and update max indexes */
              uint16_t length = fancy_hdr.GetDataSize();
              Buffer::Iterator start = fancy_hdr.GetData();
              if (m_pipeline)
              {
                PipelinedInitReceiverStep(portInfo, id, length, start, true);
              }
              else
              {
                InitReceiverStep(portInfo, id, length, start, true);
              }

              // We send an acknowledge back and move to GreyState::COUNTING
              fancy_hdr.SetAction(GREY_START);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(true);

              // So we do not add the counters when deparsing
              fancy_hdr.SetDataSize(0);
              meta.internalPacket = true;
              meta.outPort = meta.inPort;
            }
            else
            {
              // Drop control packet
              meta.drop_flag = true;
            }
            break;
          }
        }
      }
    }

    /* End of ingress logic, here we might reroute, or
     -> ! just match at the top entries table */
    if (meta.outPort != NULL)
    {
      FancyPortInfo& outPortInfo = m_portsInfo[meta.outPort->GetIfIndex()];

      if (meta.str_flow == "")
      {
        meta.str_flow = (m_packet_str_funct)(meta.flow);
      }

      /* Get State machine index */
      meta.id = MatchTopEntriesTable(meta);

      //std::cout << str_flow << meta.id << Simulator::Now().GetSeconds() << std::endl;
      /* Reroute logic for top entries */
      if (m_rerouteEnabled)
      {
        if (meta.id < m_numTopEntries)
        {
          if (outPortInfo.reroute_top[meta.id].set)
          {
            /* Drop packet so it does not go to the egress */
            meta.drop_flag = true;

            /* If not rerouted yet */
            if (!outPortInfo.reroute_top[meta.id].already_rerouted)
            {
              outPortInfo.reroute_top[meta.id].already_rerouted = true;

              /* Set the event info so its recorded */

              std::cout << "\033[1;32m# Reroute Event (top entry)" << std::endl;
              std::cout << "State Machine Index: " << meta.id << std::endl;
              std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
              outPortInfo.reroute_count++;
              std::cout << "Reroute number: " << outPortInfo.reroute_count << std::endl;
              /* Flow 5 tuple */
              if (m_packet_hash_type == "FiveTupleHash")
              {
                PrintIpFiveTuple("Flow: ", meta.flow);
              }
              else if (m_packet_hash_type == "DstPrefixHash")
              {
                PrintDstPrefix("Prefix: ", meta.flow);
              }

              std::cout << "# End Reroute Event\033[0m" << std::endl << std::endl;

              /* Set event */
              m_simState->SetRerouteEvent(Simulator::Now().GetSeconds(), outPortInfo.reroute_count, meta.flow, meta.id);

            }
          }
        }

        /* Reroute logic for zooming data structure */
        else
        {
          if (m_treeEnabled)
          {
            uint32_t bloom_filter_indexes[m_rerouteBloomFilterNumHashes];
            GetBloomFilterHashes(meta.flow, bloom_filter_indexes, m_counterWidth, m_rerouteBloomFilterWidth);

            if (IsBloomFilterSet(outPortInfo, bloom_filter_indexes))
            {
              /* Basically drop packet */
              meta.drop_flag = true;

              /* Not the best way to check if they have been already rerouted */
              /* We could simplify this a bit, but for debugging maybe having the 3 positions is useful */
              if (outPortInfo.reroute[bloom_filter_indexes[0]].already_rerouted.count(meta.str_flow) == 0)
              {

                std::cout << "\033[1;32m# Reroute Event" << std::endl;
                std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;

                _NS_LOG_DEBUG("Indexes: ", m_enableDebug);

                for (uint32_t i = 0; i < m_rerouteBloomFilterNumHashes; i++)
                {
                  _NS_LOG_DEBUG(bloom_filter_indexes[i] << " ", m_enableDebug);
                  outPortInfo.reroute[bloom_filter_indexes[i]].already_rerouted.insert(meta.str_flow);
                }
                outPortInfo.reroute_count++;
                std::cout << "Reroute number: " << outPortInfo.reroute_count << std::endl;
                /* Flow 5 tuple */
                if (m_packet_hash_type == "FiveTupleHash")
                {
                  PrintIpFiveTuple("Flow: ", meta.flow);
                }
                else if (m_packet_hash_type == "DstPrefixHash")
                {
                  PrintDstPrefix("Prefix: ", meta.flow);
                }
                std::cout << "# End Reroute Event\033[0m" << std::endl << std::endl;

                /* Set event */
                m_simState->SetRerouteEvent(Simulator::Now().GetSeconds(), bloom_filter_indexes, outPortInfo.reroute_count, meta.flow, meta.id);

              }
            }
          }
        }
      }
    }
  }

  void P4SwitchFancy::EgressCounterBox(Ptr<const Packet> packet, pkt_info& meta)
  {

    /*
    NOTE: Here when we have to send packets to a new port we are setting
    the outport directly this should be done throughout cloning the packet.
    This in the tofino implementation is different.
    */

    // Egress State machine 
    if (!meta.internalPacket && meta.headers.count("FANCY") > 0)
    {
      FancyHeader& fancy_hdr = std::any_cast<FancyHeader&>(meta.headers["FANCY"]);
      bool fsm = fancy_hdr.GetFSMFlag();

      /* Only if its for our state machine, meaning the other side sent it*/
      if (fsm)
      {
        uint8_t action = fancy_hdr.GetAction();
        bool ack_flag = fancy_hdr.GetAckFlag();
        uint16_t seq = fancy_hdr.GetSeq();
        uint16_t  nextHeader = fancy_hdr.GetNextHeader();
        uint32_t id = fancy_hdr.GetId();

        // Get Input port info: which is output port at the same time right?
        // in out is the same! 
        FancyPortInfo& inPortInfo = m_portsInfo[meta.inPort->GetIfIndex()];

        //Once the action is read we reset it
        fancy_hdr.ResetActionField();

        switch (inPortInfo.greySend.greyState[id])
        {
        case GreyState::IDLE:
          // this means that the ack for the last counter was lost 
          if (action == GREY_COUNTER and inPortInfo.greySend.currentSEQ[id] >= seq)
          {
            fancy_hdr.SetAction(GREY_COUNTER);
            fancy_hdr.SetAckFlag(true);
            fancy_hdr.SetFSMFlag(false);
            fancy_hdr.SetDataSize(0);
            meta.internalPacket = true;
            meta.outPort = meta.inPort;
          }
          else
          {
            // Drop packet otherwise it gets forwarded
            meta.drop_flag = true;
          }
          break;

        case GreyState::START_ACK:
          // The automatic event will put us here
          if (action == GREY_START and ack_flag and inPortInfo.greySend.currentSEQ[id] == seq)
          {

            SetGreyState(inPortInfo, GreyState::COUNTING, id, false);
            //std::cout << "DEBUG UPSTREAM STARTS COUNTING" << std::endl;
            Simulator::Cancel(inPortInfo.greySend.currentEvent[id]);

            //Schedule the stop event
            double probing_time;
            if (id < m_numTopEntries or id == (m_numTopEntries + 1))
            {
              probing_time = m_probing_time_top_entries_ms;
            }
            else
            {
              probing_time = m_probing_time_zooming_ms;
            }

            inPortInfo.greySend.currentEvent[id] = Simulator::Schedule(MilliSeconds(probing_time), &P4SwitchFancy::SendGreyAction, this, meta.inPort, GREY_STOP, id);

            // Drop packet 
            //meta.drop_flag = true;
          }
          // Drop packet 
          meta.drop_flag = true;
          break;

        case GreyState::COUNTING:
          // We do nothing, however a timer has been scheduled to trigger the state change
          // Should not Ignore all the start acks? 
          // Drop packet otherwise it gets forwarded
          meta.drop_flag = true;
          break;

        case GreyState::WAIT_COUNTER_RECEIVE:
          // The transition triggered a stop event.
          // If we receive the counter we move to idle 
          // otherwise we send stop again

          /* Top Entries */
          if (id < m_numTopEntries or id == (m_numTopEntries + 1))
          {
            if (action == (GREY_COUNTER) and inPortInfo.greySend.currentSEQ[id] == seq)
            {
              // stops sending Acknolwedgements all the time
              Simulator::Cancel(inPortInfo.greySend.currentEvent[id]);

              /* Logic to trigger the failure detection */
              uint64_t remote_counter = fancy_hdr.GetCounter();
              uint64_t local_counter = inPortInfo.greySend.localCounter[id];
              uint64_t counter_diff = local_counter - remote_counter;
              double packet_loss;
              if (counter_diff != 0)
              {
                packet_loss = double(counter_diff) / local_counter;
              }
              else
              {
                packet_loss = 0;
              }

              /* Cost of the system: right now either absolute or relative lost*/
              double cost;
              switch (CostTypes(m_costType))
              {
              case CostTypes::ABSOLUTE:
                cost = double(counter_diff);
                break;
              case CostTypes::RELATIVE:
                cost = packet_loss;
                break;
              }

              //if ((id == m_numTopEntries + 1) && m_treeEnableSoftDetections)
              //{
              //  std::cout << "SOFT DEBUG " << remote_counter << " " << local_counter << std::endl;
              //}
              //else {
              //  if (local_counter > 0)
              //    std::cout << "Normal Machine " << remote_counter << " " << local_counter << std::endl;
              //}

              /* Activate rerouting for this prefix */
              if (cost > m_rerouteMinCost)
              {
                if ((id == m_numTopEntries + 1) && m_treeEnableSoftDetections)
                {
                  // TODO REMOVE DEBUG
                  if (inPortInfo.reroute_top[id].set == false)
                  {
                    inPortInfo.reroute_top[id].set = true;
                    std::cout << "\n\033[1;34m# Failure Detected(" << inPortInfo.link_name << ")" << std::endl;
                    std::cout << "State Machine Index: " << id << std::endl;
                    std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
                    std::cout << "Drops: " << counter_diff << std::endl;
                    std::cout << "Loss: " << packet_loss << std::endl;
                    std::cout << "Failure number: " << inPortInfo.failures_count << std::endl;
                    std::cout << "# End Failure Detected\033[0m" << std::endl << std::endl;
                    m_simState->SetSoftFailureEvent(Simulator::Now().GetSeconds(), 2, id, local_counter, remote_counter);
                  }
                }
                else // normal dedicated counter entry
                {
                  // sets reroute flag for this port and index
                  inPortInfo.reroute_top[id].set = true;

                  std::cout << "\n\033[1;34m# Failure Detected(" << inPortInfo.link_name << ")" << std::endl;
                  std::cout << "State Machine Index: " << id << std::endl;
                  std::cout << "Time: " << Simulator::Now().GetSeconds() << std::endl;
                  std::cout << "Drops: " << counter_diff << std::endl;
                  std::cout << "Loss: " << packet_loss << std::endl;
                  inPortInfo.failures_count++;
                  std::cout << "Failure number: " << inPortInfo.failures_count << std::endl;
                  /* TO remove */
                  //std::cout << "Debug SEQ Number: " << inPortInfo.greySend.last_packet_seq[id] << std::endl;
                  std::cout << "# End Failure Detected\033[0m" << std::endl << std::endl;
                  //std::cout << "SUP(" << m_name << "): " <<  std::endl;
                  //std::string a;
                  //std::cin >> a;
                  /* Add failure detection event to the sim data*/
                  m_simState->SetFailureEvent(Simulator::Now().GetSeconds(), id, local_counter, remote_counter, inPortInfo.failures_count);

                  /* TODO probably remove (there are 3 more)
                  /* Early stop simulation */
                  if (m_early_stop_counter > 0 && inPortInfo.failures_count == m_early_stop_counter)
                  {
                    {
                      std::cout << "EARLY STOP SIMULATION (dedicated entries)" << std::endl;
                      Simulator::Stop(MilliSeconds(m_early_stop_delay_ms));
                    }
                  }
                }

              }

              // Reset counters
              inPortInfo.greySend.localCounter[id] = 0;
              SetGreyState(inPortInfo, GreyState::IDLE, id, false);
              inPortInfo.greySend.currentSEQ[id]++;

              // send ack
              fancy_hdr.SetAction(GREY_COUNTER);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(false);
              meta.internalPacket = true;
              meta.outPort = meta.inPort;

              // Start an event to trigger the next start
              inPortInfo.greySend.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_time_between_campaing_ms),
                &P4SwitchFancy::SendGreyAction, this, meta.inPort,
                (GREY_START), id);
            }
            else {
              // Drop packet otherwise it gets forwarded
              meta.drop_flag = true;
            }
          }
          /* zooming */
          else
          {
            if (action == (GREY_COUNTER | MULTIPLE_COUNTERS) and inPortInfo.greySend.currentSEQ[id] == seq)
            {
              //std::cout << "Receive counter from downstream " << std::endl;
              // stops sending Acknolwedgements all the time
              Simulator::Cancel(inPortInfo.greySend.currentEvent[id]);

              /* Counter Exchange Main Algorithm */
              if (m_pipeline)
              {
                PipelinedCounterExchangeAlgorithm(inPortInfo, id, fancy_hdr);
              }
              else
              {
                CounterExchangeAlgorithm(inPortInfo, id, fancy_hdr);
              }

              // send ack
              fancy_hdr.SetAction(GREY_COUNTER);
              fancy_hdr.SetAckFlag(true);
              fancy_hdr.SetFSMFlag(false);
              meta.internalPacket = true;
              meta.outPort = meta.inPort;

              inPortInfo.greySend.currentEvent[id] = Simulator::Schedule(MilliSeconds(m_time_between_campaing_ms),
                &P4SwitchFancy::SendGreyAction, this, meta.inPort,
                (GREY_START | COUNTER_MAXIMUMS), id);
            }
            else {
              // Drop packet otherwise it gets forwarded
              meta.drop_flag = true;
            }
          }
          break;
        }
      }
    }

    FancyPortInfo& outPortInfo = m_portsInfo[meta.outPort->GetIfIndex()];

    /* Get the state machine index where to count from here */
    uint32_t id = meta.id;

    /* Wildcard state machine always counts */
    if (outPortInfo.greySend.greyState[m_numTopEntries + 1] == GreyState::COUNTING && outPortInfo.switchPort && (meta.headers.count("IPV4") > 0))
    {
      outPortInfo.greySend.localCounter[m_numTopEntries + 1]++;
    }

    // if the other side is a switch and we are GreyState::COUNTING
    if (outPortInfo.greySend.greyState[id] == GreyState::COUNTING && outPortInfo.switchPort && (meta.headers.count("IPV4") > 0))
    {

      /* Count Output Packets for the zooming data structure */
      if (id == m_numTopEntries)
      {
        if (m_pipeline)
        {
          PipelinedPacketCounting(outPortInfo.greySend, id, meta, true);
        }
        else
        {
          PacketCounting(outPortInfo.greySend, id, meta, true);
        }
      }
      /* Normal packet count */
      else
      {
        outPortInfo.greySend.localCounter[id]++;
      }

      /* we modify so we count at the other side in case the header is already here */
      if (meta.headers.count("FANCY") > 0)
      {
        // Change it so the other side counts
        FancyHeader& fancy_hdr = std::any_cast<FancyHeader&>(meta.headers["FANCY"]);
        //set count bit
        fancy_hdr.SetCountFlag(true);
        fancy_hdr.SetId(id);
        if (fancy_hdr.GetAction() == KEEP_ALIVE)
        {
          fancy_hdr.SetSeq(outPortInfo.greySend.currentSEQ[id]);
        }
      }
      else // we need to add the header to meta
      {
        FancyHeader fancy_hdr;
        fancy_hdr.SetCountFlag(true);
        fancy_hdr.SetId(id);
        fancy_hdr.SetNextHeader(meta.protocol);
        fancy_hdr.SetSeq(outPortInfo.greySend.currentSEQ[id]);
        meta.protocol = FANCY;
        meta.headers["FANCY"] = fancy_hdr;
      }
    }
    // If the other side is a host
    else if (!outPortInfo.switchPort && meta.headers.count("FANCY") > 0)
    {
      // remove fancy header and set the ethrnet protocol of the next header that was
      // stored in the fancy header
      meta.protocol = std::any_cast<FancyHeader>(meta.headers["FANCY"]).GetNextHeader();

      // Just in case the packet made it here we have to drop it
      if (meta.protocol == 0)
      {
        meta.drop_flag = true;
      }
      meta.headers.erase("FANCY");
    }

  }


} // namespace ns3
