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
 */

 // Routing resources
 // https://www.nsnam.org/docs/models/html/routing-overview.html#unicast-routing
 // https://www.nsnam.org/docs/models/html/internet-models.html
 // https://www.nsnam.org/docs/models/html/internet-stack.html
 // https://www.nsnam.org/docs/models/html/ipv4.html
 // https://gitlab.com/nsnam/ns-3-dev/-/blob/master/examples/routing/static-routing-slash32.cc
 // https://groups.google.com/g/ns-3-users/c/82lYCe9_zk4
 // https://www.nsnam.org/doxygen/classns3_1_1_ipv4_static_routing.html

 // Example:
 // Ptr<OutputStreamWrapper> routingStream = Create<OutputStreamWrapper> ("output/routes.txt", std::ios::out);
 //
 // /* try to set a default route*/
 // /* prints routing table*/
 // Ptr<Ipv4> ipv4 = Names::Find<Node>("h_0_0")->GetObject<Ipv4> ();
 // Ipv4StaticRoutingHelper helper;
 //
 // Ptr<Ipv4StaticRouting> Ipv4stat = helper.GetStaticRouting(ipv4);
 // Ipv4stat->SetDefaultRoute(Ipv4Address("10.0.25.25"), 1);
 // Ipv4stat->AddHostRouteTo(Ipv4Address("10.0.1.1"), Ipv4Address("10.1.1.6"), 1);
 // Ipv4stat->AddNetworkRouteTo("20.0.0.0", "255.0.0.0", 1);
 //
 // if (ipv4)
 // {
 //   Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol ();
 //   NS_ASSERT (rp);
 //   rp->PrintRoutingTable (routingStream, Time::S);
 // }

 // Network topology
 //

#include <iostream>
#include <fstream>
#include <iomanip>
#include <bitset>
#include <memory>
#include <filesystem>
#include <ctime>
#include <random>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/p4-switch-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/utils.h"
#include "ns3/trace-sinks.h"
#include "ns3/simple-send.h"
#include "ns3/raw-send-application.h"
#include "ns3/trace-send-application.h"
#include "ns3/hash-utils.h"
#include "ns3/bloom-filter-test.h"
#include "ns3/fancy-utils.h"
#include "ns3/custom-utils.h"
#include "ns3/traffic-control-module.h"
#include "ns3/traffic-app-install-helpers.h"
#include "ns3/traffic-scheduler.h"

using namespace ns3;


NS_LOG_COMPONENT_DEFINE("Main-Test");

/**
 *  \brief Saves simulation metadata
 *
 *  \param out_file Output file
 *  \param sim_metadata Unordered map with all the simulation information to save
 *
 *
 */
void
SaveSimulationMetadata(std::string out_file,
  std::unordered_map<std::string, std::string> sim_metadata)
{
  if (out_file != "")
  {
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> file = asciiTraceHelper.CreateFileStream(out_file);

    for (auto& it : sim_metadata)
    {
      *(file->GetStream()) << it.first << "=" << it.second << "\n";
    }
    file->GetStream()->flush();
  }
}

/**
* Define all the command line parameters as gloal variables
* this will allow as to access them from inside functions
* the goal of this is to be able to run the same exact topology
* with just different switch types
*/

//
// Allow the user to override any of the defaults and the above Bind() at
// run-time, via command-line arguments
//

/* DEFAULT PARAMETERS */

CommandLine cmd;
/* Switch we use to run the simulation */
std::string switch_type = "Fancy";

/* General Simulation Settings */
double bw_print_interval = 0.1;
double print_interval = bw_print_interval;

/* Adds a nat switch in the topology */
/* This has some important implications such that we can send packets to any IP
and the nat will handle the address translation even if the IP does not exist,
however, we need to be careful because I did some tricks with packet forwarding
at S1 and S2 */
bool enable_nat = false;

/* Enable debugs and pcaps */
bool debug_flag = false;
bool pcap_enabled = false;

uint32_t sim_seed = 1;
std::string out_dir_base = "./output/";
/* Directory to input files used for experiment description*/
std::string in_dir_base = "./inputs/";

/* Topology Parametets*/
std::string network_bandwidth = "100Gbps";
// one direction, thus, RTT is increased two times this
uint32_t switch_to_switch_delay = 0; // In microseconds
// here we have the entire round trip for that link
double synthetic_sender_rtt = 0.01; // 10ms
uint32_t num_senders_per_rtt = 1;
uint32_t num_receivers = 20;

/* Basic Traffic Generation Params */

/* Tcp Parameters */

// RTT and RTO values in milliseconds
uint16_t min_rto = 200;
uint16_t connection_rto = 1000;

/* type of traffic we will send, this is used to decide which app we start
 *
 * Available traffic types
 * -----------------------
 *
 * StatefulTraceTraffic: uses stats and prefix info from a trace and
 * generates stateful flows.
 *
 * StatefulSyntheticTraffic: stateful traffic but generated from parameters.
 *
 * PcapReplayTraffic: reply pcap files from a binary file format we made.
 *
 * RawSyntheticTraffic: generate some synthetic raw flows by just
 * injecting packets to an interface.
 *
 * TestTraffic: sends some TCP flows.
 *
*/
std::string traffic_type = "TestTraffic";
double traffic_start = 1;
double send_duration = 5;
double sim_duration = 10;

double fail_time = 2;

uint32_t flows_per_sec = 10;

/* Synthetic traces */
std::string send_rate = "100Mbps";
uint32_t num_flows = 10;
uint32_t num_drops = 0;
std::string main_dst = "h3";

/* Synthetic one shot */
uint32_t num_elephant_flows = 0;
double elephant_share = 0;

/* Synthetic traces stateful params */
/* this is only used for synthetic generators */
uint32_t synthetic_num_prefixes = 10;

/* Caida Traces*/
std::string scaling_speed = "x1";
uint32_t num_top_entries_traffic = 1000;
uint32_t num_top_drops = 0;
uint32_t num_bottom_drops = 0;
std::string bottom_drop_type = "TopDown";
std::string top_drop_type = "TopDown";
std::string allowed_to_fail_file = "";
uint32_t fail_specific_top_index = 0;

uint32_t trace_slice = 0;

/* Switch generics */
/* drop rate at the traffic manager for tagged packets */
double m_tm_drop_rate = 0;
/* drop rate at the end of the switch for tagged packets */
double m_fail_drop_rate = 1;

/* Fancy defaults */

/* Switch Configurtation */
std::string m_packet_hash_type = "FiveTupleHash"; //DstPrefixHash
bool m_treeEnabled = true;
bool m_rerouteEnabled = true;
uint32_t m_treeDepth = 3;
uint32_t m_layerSplit = 2;
uint32_t m_counterWidth = 16;
uint32_t m_counterBloomFilterWidth = 16;
uint32_t m_rerouteBloomFilterWidth = 100000;
uint32_t m_rerouteBloomFilterNumHashes = 4;
uint16_t m_maxCounterCollisions = 1;
uint16_t m_costType = 1;
uint32_t num_top_entries_system = 100;

/* constant attributes that drive the state */

// not used for our evals thus, we set it to 1s 
// should also depend on the link delay
// TODO: careful wih this
double m_check_port_state_ms = 25;  //25ms normally increased to remove noise
bool   m_check_port_state_enable = true; // set false so we do not add noise when not wanted
double m_start_system_sec = 1;

// we only use this value or the one that comes from the input if switch to switch 
// delay is 0, otherwise we set it 2 times to that. 
double m_ack_wait_time_ms = 5; // min switch delay
double m_send_counter_wait_ms = 0;
double m_time_between_campaing_ms = 0;
double m_probing_time_zooming_ms = 200;
double m_probing_time_top_entries_ms = 200;
bool m_pipeline = true;
bool m_pipelineBoost = true;
uint16_t m_uniformLossThreshold = 0;

bool m_enableSaveDrops = false;//EnableSaveDrops
bool m_treeEnableSoftDetections = true;

/* LossRadar defaults */
uint32_t m_numBatches = 2;
uint32_t m_numHashes = 3;
double m_batchTimeMs = 100;

uint32_t m_numCells = 2048; /* also used by net seer as ring buffer size */

/* Net Seer defaults */
uint32_t m_eventCacheSize = 256;
uint32_t m_eventCounter = 128;

//bool fix_ack_time = true;

void
GetGeneralSimulationCmds()
{
  // TODO REMOVE
  //cmd.AddValue("FixAckTime", "", fix_ack_time);

  cmd.AddValue("SwitchType", "Type of switch", switch_type);
  cmd.AddValue("EnableNat", "Flag to enable a NAT switch with the advanced topo", enable_nat);
  cmd.AddValue("DebugFlag", "If enabled debugging messages will be printed", debug_flag);
  cmd.AddValue("PcapEnabled", "If enabled interfaces traffic will be captured", pcap_enabled);
  cmd.AddValue("Seed", "Random seed", sim_seed);
  cmd.AddValue("OutDirBase", "Root of where to put output files", out_dir_base);
  cmd.AddValue("InDirBase", "Input directory base where to find input files", in_dir_base);
  cmd.AddValue("TrafficType", "Sender we use to send traffic: synthetic or trace based",
    traffic_type);
  cmd.AddValue("TrafficStart", "Start time of main events", traffic_start);
  cmd.AddValue("FlowsPerSec", "Starting flows per sec", flows_per_sec);
  cmd.AddValue("SendDuration", "Duration in seconds to keep the rate", send_duration);
  /* Overwriten by start + duration */
  //cmd.AddValue ("SimDuration", "Simulation duration", sim_duration);
  cmd.AddValue("FailTime", "Time at witch some failure are scheduled", fail_time);

  cmd.AddValue("SendRate", "Datarate to send", send_rate);
  cmd.AddValue("SyntheticNumPrefixes", "Number of prefixes to use in our synthetic generation",
    synthetic_num_prefixes);
  /* although this is called num flows, this was used to create fancy entries, when we did per flow detection */
  cmd.AddValue("NumFlows", "Uniform amout of different flows", num_flows);
  cmd.AddValue("DstAddr", "Destination node", main_dst);
  cmd.AddValue("NumDrops", "Number of flows being blackholed", num_drops);

  cmd.AddValue("NumElephantFlows", "Number of elephant flows", num_elephant_flows);
  cmd.AddValue("ElephantShare", "Byte share of elephant flows", elephant_share);

  cmd.AddValue("Scaling", "Scaling speed of the pcap trace", scaling_speed);
  cmd.AddValue("TraceSlice", "Trace slice to simulate", trace_slice);
  cmd.AddValue("AllowedToFail", "List of prefixes that can be failed", allowed_to_fail_file);
  //cmd.AddValue("TraceBin", "File with the binary trace to inject", trace_bin);
  //cmd.AddValue("TopPrefixesFile", "file with top prefixes parsed from trace", trace_top_prefixes);

  cmd.AddValue("TopFailType", "Way of selecting prefixes in the top region", top_drop_type);
  cmd.AddValue("BottomFailType", "Way of selecting prefixes in the down region", bottom_drop_type);
  cmd.AddValue("NumTopEntriesTraffic", "Number of prefixes considered top",
    num_top_entries_traffic);
  cmd.AddValue("NumTopFails", "prefixes to fail from the top", num_top_drops);
  cmd.AddValue("FailSpecificTopIndex", "If set we only fail this top index",
    fail_specific_top_index);
  cmd.AddValue("NumBottomFails", "prefixes to fail from the bottom", num_bottom_drops);

  /* TCP values */
  cmd.AddValue("minRTO", "Min RTO for tcp flows", min_rto);
  cmd.AddValue("connRTO", "Min connection RTO for tcp flows", connection_rto);

  /* Topology parameters */
  cmd.AddValue("SwitchDelay", "Delay in the interface between switches in microseconds (us)",
    switch_to_switch_delay);
  cmd.AddValue("NetworkBandwidth", "The bandwidth of the links in the netwrok ",
    network_bandwidth);
  cmd.AddValue("NumSendersPerRtt", "Number of hosts per RTT to loadbalance from",
    num_senders_per_rtt);
  cmd.AddValue("NumReceivers", "Number of receiving hosts", num_receivers);

  cmd.AddValue("TmDropRate", "Percentage of traffic that gets dropped by the traffic manager.",
    m_tm_drop_rate);
  cmd.AddValue("FailDropRate", "Percentage of traffic that gets dropped when there is a failure.",
    m_fail_drop_rate);
  cmd.AddValue("UniformLossThreshold",
    "Number of entries at the lowest layer of the tree to report a uniform random drop",
    m_uniformLossThreshold);
}

/* Cmd line parameters definitions for each switch */
void
GetFancyCmds()
{
  cmd.AddValue("TreeEnabled", "Enables/Disables the zooming tree.", m_treeEnabled);
  cmd.AddValue("RerouteEnabled", "Enables/Disables traffic rerouting", m_rerouteEnabled);
  cmd.AddValue("Pipeline", "Enables/Disables the pipeline algorithm.", m_pipeline);
  cmd.AddValue("PipelineBoost", "Enables/Disables the pipeline max allocation boost.",
    m_pipelineBoost);
  cmd.AddValue("TreeDepth", "Depth of the zooming data structure.", m_treeDepth);
  cmd.AddValue("LayerSplit", "Amount of childs each layer can expand to.", m_layerSplit);
  cmd.AddValue("CounterWidth", "Numsber of counters per tree cell.", m_counterWidth);
  cmd.AddValue("CounterBloomFilterWidth", "Width of bloom filters per counter.",
    m_counterBloomFilterWidth);
  cmd.AddValue("RerouteBloomFilterWidth", "Cells in the reroute bloom filter.",
    m_rerouteBloomFilterWidth);
  cmd.AddValue("RerouteBloomFilterNumHashes",
    "Number of hashes used for the reroute bloom filter.",
    m_rerouteBloomFilterNumHashes);
  cmd.AddValue("MaxCounterCollisions",
    "Number of maximum amout of collisions accepted in a counter field.",
    m_maxCounterCollisions);
  cmd.AddValue("CostType", "0 = absolute, 1 = relative.", m_costType);
  cmd.AddValue("NumTopEntriesSystem", "Number of prefixes with dedicated memory",
    num_top_entries_system);

  cmd.AddValue("CheckPortStateMs", "Frequency check for port down.", m_check_port_state_ms);
  cmd.AddValue("CheckPortStateEnable", "Enable check for port down", m_check_port_state_enable);

  cmd.AddValue("StartSystemSec", "Starting time for the system detection.", m_start_system_sec);
  cmd.AddValue("AckWaitTimeMs", "Time a node waits for ack before retransmissions.",
    m_ack_wait_time_ms);
  cmd.AddValue("SendCounterWaitMs", "Time a node waits for counters ack before retransmission.",
    m_send_counter_wait_ms);
  cmd.AddValue("TimeBetweenCampaingMs", "Wait time between Counting campaings, this can be 0.",
    m_time_between_campaing_ms);
  cmd.AddValue("ProbingTimeZoomingMs", "Amount of time accomulating counters.",
    m_probing_time_zooming_ms);
  cmd.AddValue("ProbingTimeTopEntriesMs", "Amount of time accomulating counters.",
    m_probing_time_top_entries_ms);
  cmd.AddValue("PacketHashType", "function used to hash packets", m_packet_hash_type);
  cmd.AddValue("EnableSaveDrops", "enables saving drops into a file at the output dir", m_enableSaveDrops);
  cmd.AddValue("SoftDetectionEnabled", "Enables saving base line information", m_treeEnableSoftDetections);

}

void
GetLossRadarCmds()
{
  cmd.AddValue("NumberBatches", "Number of batches we measure.", m_numBatches);
  cmd.AddValue("NumberOfCells", "Number of cells per port and batch", m_numCells);
  cmd.AddValue("NumberHashes", "Number of hash functions per packet", m_numHashes);
  cmd.AddValue("BatchTime", "Batch frequency update in ms", m_batchTimeMs);
}

void
GetNetSeerCmds()
{
  cmd.AddValue("NumberOfCells", "Number of cells per port and batch", m_numCells);
  cmd.AddValue("EventCacheSize", "Number of cells in the event cache", m_eventCacheSize);
  cmd.AddValue("EventCounter", "Treshold before reporting an event multiple times",
    m_eventCounter);
}

/* Configuring defaults */
void
SetGeneralSimulationDefaults()
{
  Config::SetDefault("ns3::P4SwitchNetDevice::EnableDebug", BooleanValue(debug_flag));

  //Set globals defaults
  Config::SetDefault("ns3::CsmaChannel::FullDuplex",
    BooleanValue(true)); //same than DupAckThreshold

  /* TCP defaults */
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1446)); //MTU 1446
}

void
SetTcpDefaults()
{
  //Tcp Socket (general socket conf)
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(10000000));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(10000000));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1446)); //MTU 1446
  Config::SetDefault("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(4294967295));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));

  //Can be much slower than my rtt because packet size of syn is 60bytes
  Config::SetDefault(
    "ns3::TcpSocket::ConnTimeout",
    TimeValue(MilliSeconds(connection_rto))); // connection retransmission timeout
  Config::SetDefault("ns3::TcpSocket::ConnCount",
    UintegerValue(5)); //retrnamissions during connection
  Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(10)); //retranmissions
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(min_rto / 50)));
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(0)));
  // 	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue(2));
  Config::SetDefault("ns3::TcpSocket::TcpNoDelay",
    BooleanValue(true)); //disable nagle's algorithm
  Config::SetDefault(
    "ns3::TcpSocket::PersistTimeout",
    TimeValue(NanoSeconds(6000000000))); //persist timeout to porbe for rx window

  //Tcp Socket Base: provides connection orientation, sliding window, flow control; congestion control is delegated to the subclasses (i.e new reno)

  Config::SetDefault("ns3::TcpSocketBase::MaxSegLifetime", DoubleValue(10));
  Config::SetDefault("ns3::TcpSocketBase::MinRto",
    TimeValue(MilliSeconds(min_rto))); //min RTO value that can be set
  Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MicroSeconds(1)));
  Config::SetDefault("ns3::TcpSocketBase::ReTxThreshold",
    UintegerValue(3)); //same than DupAckThreshold

  Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true)); //enable sack
  Config::SetDefault("ns3::TcpSocketBase::LimitedTransmit", BooleanValue(true)); //enable sack

  //TCP L4
  //TcpNewReno
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  //Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpVeno::GetTypeId()));
  Config::SetDefault("ns3::RttEstimator::InitialEstimation",
    TimeValue(MicroSeconds(connection_rto))); //defautlt 1sec
}

void
SetFancyDefaults()
{
  Config::SetDefault("ns3::P4SwitchFancy::PacketHashType", StringValue(m_packet_hash_type));
  Config::SetDefault("ns3::P4SwitchFancy::TreeEnabled", BooleanValue(m_treeEnabled));
  Config::SetDefault("ns3::P4SwitchFancy::RerouteEnabled", BooleanValue(m_rerouteEnabled));
  Config::SetDefault("ns3::P4SwitchFancy::Pipeline", BooleanValue(m_pipeline));
  Config::SetDefault("ns3::P4SwitchFancy::PipelineBoost", BooleanValue(m_pipelineBoost));
  Config::SetDefault("ns3::P4SwitchFancy::TreeDepth", UintegerValue(m_treeDepth));
  Config::SetDefault("ns3::P4SwitchFancy::LayerSplit", UintegerValue(m_layerSplit));
  Config::SetDefault("ns3::P4SwitchFancy::CounterWidth", UintegerValue(m_counterWidth));
  Config::SetDefault("ns3::P4SwitchFancy::CounterBloomFilterWidth",
    UintegerValue(m_counterBloomFilterWidth));
  Config::SetDefault("ns3::P4SwitchFancy::RerouteBloomFilterWidth",
    UintegerValue(m_rerouteBloomFilterWidth));
  Config::SetDefault("ns3::P4SwitchFancy::RerouteBloomFilterNumHashes",
    UintegerValue(m_rerouteBloomFilterNumHashes));
  Config::SetDefault("ns3::P4SwitchFancy::MaxCounterCollisions",
    UintegerValue(m_maxCounterCollisions));
  Config::SetDefault("ns3::P4SwitchFancy::CostType", UintegerValue(m_costType));
  Config::SetDefault("ns3::P4SwitchFancy::NumTopEntries", UintegerValue(num_top_entries_system));
  /* swtich drop rates*/
  Config::SetDefault("ns3::P4SwitchFancy::TmDropRate", DoubleValue(m_tm_drop_rate));
  Config::SetDefault("ns3::P4SwitchFancy::FailDropRate", DoubleValue(m_fail_drop_rate));
  /* unifrom packet drops */
  Config::SetDefault("ns3::P4SwitchFancy::UniformLossThreshold",
    UintegerValue(m_uniformLossThreshold));
  Config::SetDefault("ns3::P4SwitchFancy::CheckPortStateMs", DoubleValue(m_check_port_state_ms));
  Config::SetDefault("ns3::P4SwitchFancy::CheckPortStateEnable", BooleanValue(m_check_port_state_enable));
  Config::SetDefault("ns3::P4SwitchFancy::StartSystemSec", DoubleValue(m_start_system_sec));

  /* Use switch to switch delay in order to adjust this timeout. There seems to
  be a bug if the system is retransmissing all the time.

  When the interswitch delay is bigger than this ack, state machines will
  retransmit when they should not, leading to some weird corner cases.
  */
  if (switch_to_switch_delay > 0) // from microseconds to ms
  {
    double new_ack_wait_time_ms = (switch_to_switch_delay / 1000) * 4; // 2 times the rount trip
    std::cout << "The ack wait time had to be adapted from " << m_ack_wait_time_ms << " to " << new_ack_wait_time_ms << std::endl;
    m_ack_wait_time_ms = new_ack_wait_time_ms;
  }


  Config::SetDefault("ns3::P4SwitchFancy::AckWaitTimeMs", DoubleValue(m_ack_wait_time_ms));
  Config::SetDefault("ns3::P4SwitchFancy::SendCounterWaitMs",
    DoubleValue(m_send_counter_wait_ms));
  Config::SetDefault("ns3::P4SwitchFancy::TimeBetweenCampaingMs",
    DoubleValue(m_time_between_campaing_ms));
  Config::SetDefault("ns3::P4SwitchFancy::ProbingTimeZoomingMs",
    DoubleValue(m_probing_time_zooming_ms));
  Config::SetDefault("ns3::P4SwitchFancy::ProbingTimeTopEntriesMs",
    DoubleValue(m_probing_time_top_entries_ms));

  // Main top file without slice
  Config::SetDefault("ns3::P4SwitchFancy::TopFile", StringValue(in_dir_base + ".top"));

  // Save drops enabled
  Config::SetDefault("ns3::P4SwitchFancy::EnableSaveDrops", BooleanValue(m_enableSaveDrops));
  Config::SetDefault("ns3::P4SwitchFancy::SoftDetectionEnabled", BooleanValue(m_treeEnableSoftDetections));

}

void
SetLossRadarDefaults()
{

  Config::SetDefault("ns3::P4SwitchLossRadar::NumberBatches", UintegerValue(m_numBatches));
  Config::SetDefault("ns3::P4SwitchLossRadar::NumberOfCells", UintegerValue(m_numCells));
  Config::SetDefault("ns3::P4SwitchLossRadar::NumberHashes", UintegerValue(m_numHashes));
  Config::SetDefault("ns3::P4SwitchLossRadar::BatchTime", DoubleValue(m_batchTimeMs));

  /* switch drop rates */
  Config::SetDefault("ns3::P4SwitchLossRadar::TmDropRate", DoubleValue(m_tm_drop_rate));
  Config::SetDefault("ns3::P4SwitchLossRadar::FailDropRate", DoubleValue(m_fail_drop_rate));
}

void
SetNetSeerDefaults()
{
  Config::SetDefault("ns3::P4SwitchNetSeer::NumberOfCells", UintegerValue(m_numCells));
  /* switch drop rates */
  Config::SetDefault("ns3::P4SwitchNetSeer::TmDropRate", DoubleValue(m_tm_drop_rate));
  Config::SetDefault("ns3::P4SwitchNetSeer::FailDropRate", DoubleValue(m_fail_drop_rate));
}

void
SetFancyMetadata(std::unordered_map<std::string, std::string>& sim_metadata)
{
  /* Fancy switch info */
  sim_metadata["NumTopEntriesSystem"] = std::to_string(num_top_entries_system);
  sim_metadata["Pipeline"] = std::to_string(m_pipeline);
  sim_metadata["PipelineBoost"] = std::to_string(m_pipelineBoost); // what was this?
  sim_metadata["TreeDepth"] = std::to_string(m_treeDepth);
  sim_metadata["LayerSplit"] = std::to_string(m_layerSplit);
  sim_metadata["CounterWidth"] = std::to_string(m_counterWidth);
  sim_metadata["CostType"] = std::to_string(m_costType);
  sim_metadata["ProbingTimeZoomingMs"] = std::to_string(m_probing_time_zooming_ms);
  sim_metadata["ProbingTimeTopEntriesMs"] = std::to_string(m_probing_time_top_entries_ms);
  sim_metadata["TreeEnabled"] = std::to_string(m_treeEnabled);
  sim_metadata["RerouteEnabled"] = std::to_string(m_rerouteEnabled);
  sim_metadata["PacketHashType"] = m_packet_hash_type;
  sim_metadata["RerouteBloomFilterWidth"] = std::to_string(m_rerouteBloomFilterWidth);
  sim_metadata["AckWaitTimeMs"] = std::to_string(m_ack_wait_time_ms);
  sim_metadata["EnableSaveDrops"] = std::to_string(m_enableSaveDrops);
  sim_metadata["SoftDetectionEnabled"] = std::to_string(m_treeEnableSoftDetections);
}

void
SetLossRadarMetadata(std::unordered_map<std::string, std::string>& sim_metadata)
{
  sim_metadata["NumBatches"] = std::to_string(m_numBatches);
  sim_metadata["BatchTimeMs"] = std::to_string(m_batchTimeMs);
  sim_metadata["NumCells"] = std::to_string(m_numCells);
}

void
SetNetSeerMetadata(std::unordered_map<std::string, std::string>& sim_metadata)
{
  /* Number of cells of the RING BUFFER */
  sim_metadata["NumCells"] = std::to_string(m_numCells);
  sim_metadata["EventCacheSize"] = std::to_string(m_eventCacheSize);
  sim_metadata["EventCounter"] = std::to_string(m_eventCounter);
}

std::vector<NetDeviceContainer>
ConfigureSwitches(Ptr<Node> s1, Ptr<Node> s2, NetDeviceContainer switch1Devices,
  NetDeviceContainer switch2Devices, std::string out_dir_base)
{
  // Create the switch netdevice, which will do the packet switching
  /* Set switches metadata */
  std::string switchType;

  NetDeviceContainer sw1_devs;
  NetDeviceContainer sw2_devs;

  if (switch_type == "Fancy")
  {
    /* Sets both switches in the middle */
    switchType = "ns3::P4SwitchFancy";
    P4SwitchHelper switch_h(switchType);
    sw1_devs = switch_h.Install<P4SwitchFancy>(s1, switch1Devices);
    sw1_devs.Get(0)->SetAttribute("OutFile", StringValue(out_dir_base + "_s1.json"));
    //sw1_devs.Get(0)->SetAttribute("DropsFile", StringValue(out_dir_base + "_s1.drops"));

    /* second switch */
    sw2_devs = switch_h.Install<P4SwitchFancy>(s2, switch2Devices);
    DynamicCast<P4SwitchFancy>(sw2_devs.Get(0))->SetDebug(false);
    sw2_devs.Get(0)->SetAttribute("OutFile", StringValue(""));
    // to make sure we do not save 
    sw2_devs.Get(0)->SetAttribute("EnableSaveDrops", BooleanValue(false));

    //sw2_devs.Get(0)->SetAttribute("DropsFile", StringValue(""));

    /* TODO: might remove this? */
    /* This should not affect */
    DynamicCast<P4SwitchFancy>(sw2_devs.Get(0))->DisableAllFSM();
  }

  else if (switch_type == "LossRadar")
  {
    // Create the switch netdevice, which will do the packet switching
    switchType = "ns3::P4SwitchLossRadar";
    P4SwitchHelper switch_h(switchType);
    sw1_devs = switch_h.Install<P4SwitchLossRadar>(s1, switch1Devices);
    sw1_devs.Get(0)->SetAttribute("OutFile", StringValue(out_dir_base + "_s1.json"));

    sw2_devs = switch_h.Install<P4SwitchLossRadar>(s2, switch2Devices);
    DynamicCast<P4SwitchLossRadar>(sw2_devs.Get(0))->SetDebug(false);
    sw2_devs.Get(0)->SetAttribute("OutFile", StringValue(""));
  }

  else if (switch_type == "NetSeer")
  {
    // Create the switch netdevice, which will do the packet switching
    switchType = "ns3::P4SwitchNetSeer";
    P4SwitchHelper switch_h(switchType);
    sw1_devs = switch_h.Install<P4SwitchNetSeer>(s1, switch1Devices);
    sw1_devs.Get(0)->SetAttribute("OutFile", StringValue(out_dir_base + "_s1.json"));

    sw2_devs = switch_h.Install<P4SwitchNetSeer>(s2, switch2Devices);
    DynamicCast<P4SwitchNetSeer>(sw2_devs.Get(0))->SetDebug(false);
    sw2_devs.Get(0)->SetAttribute("OutFile", StringValue(""));
  }
  else {
    std::cout << "Specify a valid switch type (Fancy, LossRadar, NetSeer), your type: " << switch_type << std::endl;
  }

  std::vector<NetDeviceContainer> switch_devices;
  switch_devices.push_back(sw1_devs);
  switch_devices.push_back(sw2_devs);

  return switch_devices;
}

/* This needs to be properly updated*/
void
StatefulFailureScheduler(std::unordered_map<std::string, TrafficPrefixStats>, uint32_t num_drops,
  uint32_t num_top_entries_traffic, std::string top_file,
  std::string allowed_to_fail, std::string failed_prefixes_out_file,
  NetDeviceContainer sw)
{
  /* Not done at the end since we end up using a hybrid generator for the Eval 1*/
}

void
HybridFailureScheduler(std::unordered_set<uint32_t> prefixes_to_fail, NetDeviceContainer sw)
{

  std::vector<std::pair<uint32_t, Ptr<NetDevice>>> prefixes;

  Ptr<NetDevice> ndev = NULL;
  std::cout << "Adding prefixes to fail" << std::endl;
  for (auto const& prefix : prefixes_to_fail)
  {
    /* We add +1 because all senders use 1 as ip */
    prefixes.push_back(std::make_pair(prefix + 1, ndev));
    std::cout << Ipv4Address(prefix + 1) << std::endl;
  }

  DynamicCast<P4SwitchNetDevice>(sw.Get(0))->L3SpecialForwardingSetFailures(prefixes);
}

void
StatefulSyntheticFailureScheduler(std::vector<FlowMetadata> flowDist, NetDeviceContainer sw,
  uint32_t num_fail)
{
  NS_ASSERT_MSG(flowDist.size() >= num_fail, "Trying to fail too many prefixes");

  std::vector<std::pair<uint32_t, Ptr<NetDevice>>> prefixes;

  //std::srand (sim_seed);
  //std::random_shuffle(prefixes.begin(), prefixes.end());
  //std::random_device rd;
  //std::mt19937 g (rd (sim_seed));
  std::shuffle(prefixes.begin(), prefixes.end(), std::default_random_engine(sim_seed));

  Ptr<NetDevice> ndev = NULL;
  for (uint32_t i = 0; i < num_fail; i++)
  {
    /* Get the prefix string and remove /24*/
    std::string dst_ip = flowDist[i].prefix.substr(0, flowDist[i].prefix.find("/"));
    /* Add a rule for the prefix and ip x.x.x.1, this is how i did it everywhere might need to be changed*/
    prefixes.push_back(std::make_pair(Ipv4Address(dst_ip.c_str()).Get(), ndev));
    std::cout << Ipv4Address(Ipv4Address(dst_ip.c_str()).Get()) << std::endl;
  }

  std::cout << "Adding prefixes to fail: " << prefixes.size() << std::endl;

  DynamicCast<P4SwitchNetDevice>(sw.Get(0))->L3SpecialForwardingSetFailures(prefixes);
}

/* copy to restore */
void
StatefulSyntheticRestoreScheduler(std::vector<FlowMetadata> flowDist, NetDeviceContainer sw,
  uint32_t num_fail)
{
  NS_ASSERT_MSG(flowDist.size() >= num_fail, "Trying to fail too many prefixes");

  std::vector<std::pair<uint32_t, Ptr<NetDevice>>> prefixes;
  //std::srand (sim_seed);
  //std::random_shuffle(prefixes.begin(), prefixes.end());
  std::shuffle(prefixes.begin(), prefixes.end(), std::default_random_engine(sim_seed));
  //std::shuffle (prefixes.begin (), prefixes.end ());

  Ptr<NetDevice> ndev = NULL;
  for (uint32_t i = 0; i < num_fail; i++)
  {
    /* Get the prefix string and remove /24*/
    std::string dst_ip = flowDist[i].prefix.substr(0, flowDist[i].prefix.find("/"));
    /* Add a rule for the prefix and ip x.x.x.1, this is how i did it everywhere might need to be changed*/
    prefixes.push_back(std::make_pair(Ipv4Address(dst_ip.c_str()).Get(), ndev));
    std::cout << Ipv4Address(Ipv4Address(dst_ip.c_str()).Get()) << std::endl;
  }

  std::cout << "Adding prefixes to fail: " << prefixes.size() << std::endl;

  DynamicCast<P4SwitchNetDevice>(sw.Get(0))->L3SpecialForwardingRemoveFailures(prefixes);
}

int
main(int argc, char* argv[])
{

  std::clock_t simulation_execution_time = std::clock();

  /* Generic cmds */
  GetGeneralSimulationCmds();

  /* Load all switches commands*/
  GetFancyCmds();
  GetLossRadarCmds();
  GetNetSeerCmds();

  cmd.Parse(argc, argv);

  //std::cout << "Tree value " << m_treeEnabled << std::endl;
  //return 0;

  /* Gets overwritten */
  sim_duration = send_duration + traffic_start;

  /* Setting Global Parameters */
  RngSeedManager::SetRun(7); // Changes run number from default of 1 to 7
  RngSeedManager::SetSeed(sim_seed);

  /* Setting global defaults */
  SetGeneralSimulationDefaults();
  SetTcpDefaults();

  /* Switches defaults */
  SetFancyDefaults();
  SetLossRadarDefaults();
  SetNetSeerDefaults();

  //Set simulator's time resolution (click) We use Picoseconds so we can devide nanoseconds
  Time::SetResolution(Time::PS);

  /* Switches Globals */
  static GlobalValue g_myGlobal = GlobalValue("switchId", "Global Switch Id", UintegerValue(1),
    MakeUintegerChecker<uint8_t>());
  static GlobalValue g_debugGlobal =
    GlobalValue("debugGlobal", "Is debug globally enabled for my modules?",
      BooleanValue(debug_flag), MakeBooleanChecker());

  //
  // Explicitly create the nodes required by the topology (shown above).
  //
  //LogComponentEnable ("BulkSendApplication", LOG_ALL);
  //LogComponentEnable ("Ipv4RawSocketImpl", LOG_ALL);
  //LogComponentEnable("TcpSocketBase", LOG_ALL);
  //LogComponentEnable("CsmaNetDevice", LOG_ALL);
  //LogComponentEnable("CsmaChannel", LOG_ALL);
  //LogComponentEnable("QueueDisc", LOG_ALL);
  if (debug_flag)
  {
    LogComponentEnable("P4SwitchNetDevice", LOG_DEBUG);
    LogComponentEnable("P4SwitchFancy", LOG_DEBUG);
    LogComponentEnable("P4SwitchLossRadar", LOG_DEBUG);
    LogComponentEnable("P4SwitchNetSeer", LOG_DEBUG);
    //LogComponentEnable ("ArpCache", LOG_ALL);
    //LogComponentEnable ("ArpL3Protocol", LOG_ALL);
  }

  /* SAVE SIMULATION INFORMATION */

  /* Global parameters */
  std::unordered_map<std::string, std::string> sim_metadata;
  sim_metadata["Seed"] = std::to_string(sim_seed);
  std::filesystem::path relative_path = out_dir_base;
  std::filesystem::path absolute_path = std::filesystem::absolute(relative_path);
  sim_metadata["OutDirBase"] = absolute_path.string();
  sim_metadata["SendDuration"] = std::to_string(send_duration);
  relative_path = in_dir_base;
  absolute_path = std::filesystem::absolute(relative_path);
  sim_metadata["InDirBase"] = absolute_path;
  sim_metadata["SwitchType"] = switch_type;
  sim_metadata["EnableNat"] = std::to_string(enable_nat);

  /* Topology info */
  sim_metadata["NetworkBandwidth"] = network_bandwidth;
  sim_metadata["SwitchDelay"] = std::to_string(switch_to_switch_delay);
  sim_metadata["SyntheticSenderRtt"] = std::to_string(synthetic_sender_rtt);
  sim_metadata["NumSendersPerRtt"] = std::to_string(num_senders_per_rtt);
  sim_metadata["NumReceivers"] = std::to_string(num_receivers);

  sim_metadata["FailDropRate"] = std::to_string(m_fail_drop_rate);
  sim_metadata["TmDropRate"] = std::to_string(m_tm_drop_rate);
  sim_metadata["UniformLossThreshold"] = std::to_string(m_uniformLossThreshold);

  /*Traffic generator info */
  sim_metadata["TrafficType"] = traffic_type;
  sim_metadata["TrafficStart"] = std::to_string(traffic_start);
  sim_metadata["TrafficDuration"] = std::to_string(send_duration);
  sim_metadata["FailTime"] = std::to_string(fail_time);

  /* flow based */
  sim_metadata["SendRate"] = send_rate;
  sim_metadata["FlowsPerSec"] = std::to_string(flows_per_sec);
  sim_metadata["NumFlows"] = std::to_string(num_flows);
  sim_metadata["NumPrefixes"] = std::to_string(synthetic_num_prefixes);
  sim_metadata["NumDrops"] = std::to_string(num_drops);

  /* Synthetic info */
  sim_metadata["NumElephantFlows"] = std::to_string(num_elephant_flows);
  sim_metadata["ElephantShare"] = std::to_string(elephant_share);

  /* trace based */
  sim_metadata["TraceSlice"] = std::to_string(trace_slice);
  sim_metadata["TopFailType"] = top_drop_type;
  sim_metadata["BottomFailType"] = bottom_drop_type;
  sim_metadata["NumTopEntriesTraffic"] = std::to_string(num_top_entries_traffic);
  sim_metadata["NumTopFails"] = std::to_string(num_top_drops);
  sim_metadata["FailSpecificTopIndex"] = std::to_string(fail_specific_top_index);
  sim_metadata["NumBottomFails"] = std::to_string(num_bottom_drops);
  sim_metadata["AllowedToFail"] = allowed_to_fail_file;

  /* Set switches metadata */
  if (switch_type == "Fancy")
  {
    SetFancyMetadata(sim_metadata);
  }
  else if (switch_type == "LossRadar")
  {
    SetLossRadarMetadata(sim_metadata);
  }
  else if (switch_type == "NetSeer")
  {
    SetNetSeerMetadata(sim_metadata);
  }

  /* Save current time */
  time_t seconds_past_epoch = time(0);
  sim_metadata["ExperimentEpoch"] = std::to_string(seconds_past_epoch);

  /* CREATES TOPOLOGY */
  /* Basic 2 switch topology */

  /* Define some variables here so they are global */
  NS_LOG_INFO("Create topology and send traffic");

  /* Save links for later use */
  std::unordered_map<std::string, NetDeviceContainer> links;

  /* latency to node map */
  std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node;


  /* When trying to emulate real traces */
  if (traffic_type == "StatefulTraceTraffic" || traffic_type == "StatefulSyntheticTraffic" ||
    traffic_type == "HybridTraceTraffic")
  {

    /* Set table type */
    Config::SetDefault("ns3::P4SwitchNetDevice::ForwardingType",
      EnumValue(P4SwitchNetDevice::ForwardingType::L3_SPECIAL_FORWARDING));

    /* All all the logic for Traced based traffic */
    /* For synthetic generations right now we set the RTT CDF to one data point */
    std::vector<double> experiment_rtts;
    if (traffic_type == "StatefulTraceTraffic" || traffic_type == "HybridTraceTraffic")
    {
      experiment_rtts =
        GetRTTs(in_dir_base + "_" + std::to_string(trace_slice) + "_rtt_cdfs.txt");

    }
    else if (traffic_type == "StatefulSyntheticTraffic")
    {
      experiment_rtts.push_back(synthetic_sender_rtt); //10ms rtt
    }

    uint32_t num_total_senders = experiment_rtts.size() * num_senders_per_rtt;

    /* Create nodes*/
    NodeContainer senders;
    senders.Create(num_total_senders);

    NodeContainer receivers;
    receivers.Create(num_receivers);

    NodeContainer switches;
    switches.Create(3); // third switch is the NAT

    /* Install internet stack */
    InternetStackHelper internet;
    internet.Install(senders);
    internet.Install(receivers);

    /* Start building topology */
    CsmaHelper csma_hosts;
    CsmaHelper csma_switches;

    DataRate hostsBandwidth(network_bandwidth);
    /* Inter Switch Link */
    DataRate switchBandwidth(network_bandwidth);

    /* Defaults */
    csma_switches.SetChannelAttribute("DataRate", DataRateValue(switchBandwidth));
    csma_switches.SetChannelAttribute("Delay",
      TimeValue(MicroSeconds(switch_to_switch_delay)));
    csma_switches.SetDeviceAttribute("Mtu", UintegerValue(1500));
    csma_switches.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

    csma_hosts.SetChannelAttribute("DataRate", DataRateValue(hostsBandwidth));
    //csma_hosts.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (switch_to_switch_delay))); // to overwrite
    csma_hosts.SetDeviceAttribute("Mtu", UintegerValue(1500));
    csma_hosts.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

    /* Set network links */

    // Add switches since they are simple
    Ptr<Node> s1 = switches.Get(0);
    Ptr<Node> s2 = switches.Get(1);
    Ptr<Node> nat = switches.Get(2);
    Names::Add("s1", s1);
    Names::Add("s2", s2);
    Names::Add("nat", nat);

    links["s1->s2"] = csma_switches.Install(NodeContainer(s1, s2));

    /* creates link between s2 and nat switch */
    if (enable_nat)
    {
      links["s2->nat"] = csma_switches.Install(NodeContainer(s2, nat));
      // Make sure that s2 to nat switch delay is 0
      links["s2->nat"].Get(0)->GetChannel()->SetAttribute("Delay",
        TimeValue(PicoSeconds(0)));
      links["s2->nat"].Get(1)->GetChannel()->SetAttribute("Delay",
        TimeValue(PicoSeconds(0)));
    }

    /* Switch interfaces */
    /* We store all the interfaces that will be attached to each switch */
    NetDeviceContainer switch1Devices;
    NetDeviceContainer switch2Devices;
    NetDeviceContainer natDevices;
    NetDeviceContainer sendersDevices;
    NetDeviceContainer receiversDevices;

    /* Add first interfaces */
    switch1Devices.Add(links["s1->s2"].Get(0));
    switch2Devices.Add(links["s1->s2"].Get(1));

    /* adds basic link to nat */
    if (enable_nat)
    {
      switch2Devices.Add(links["s2->nat"].Get(0));
      natDevices.Add(links["s2->nat"].Get(1));
    }

    /* Start allocating senders */
    for (uint32_t rtt_index = 0; rtt_index < experiment_rtts.size(); rtt_index++)
    {

      /* get round trip delay */
      double round_trip_delay = experiment_rtts[rtt_index];

      /* Subtract whatever the switch to switch adds */
      if (switch_to_switch_delay > 0) {
        round_trip_delay = round_trip_delay - (2 * (double(switch_to_switch_delay) / 1000000));
        /* in case its negative we max it at 0 */
        round_trip_delay = std::max(round_trip_delay, 0.0);

        // fix the delays rtt cdf and remove the switch to switch delay
        experiment_rtts[rtt_index] = round_trip_delay;
      }


      //std::cout << "round trip delay " << round_trip_delay << std::endl;
      /* interface one way delay */
      double interface_delay = round_trip_delay / 2;
      //std::cout << "Interface delay: " << interface_delay << "  Original RTT: " << round_trip_delay << std::endl;

      for (uint32_t rtt_node_index = 0; rtt_node_index < num_senders_per_rtt; rtt_node_index++)
      {
        /* Add node to names*/
        Ptr<Node> sender = senders.Get((rtt_index * num_senders_per_rtt) + rtt_node_index);
        std::stringstream sender_name;
        sender_name << "h_" << rtt_index << "_" << rtt_node_index;
        //std::cout << sender_name.str() << std::endl;
        Names::Add(sender_name.str(), sender);

        /* link it with s1*/
        NetDeviceContainer link = csma_hosts.Install(NodeContainer(sender, s1));
        /* Set properties */
        link.Get(0)->GetChannel()->SetAttribute("DataRate",
          DataRateValue(hostsBandwidth));
        link.Get(1)->GetChannel()->SetAttribute("DataRate",
          DataRateValue(hostsBandwidth));
        link.Get(0)->GetChannel()->SetAttribute("Delay",
          TimeValue(Seconds(interface_delay)));

        /* Delay only needs to be set in one direction since we set the channel
        delay and then it is applied in all directions */
        //link.Get(1)->GetChannel()->SetAttribute("Delay",TimeValue(Seconds(interface_delay));

        links[sender_name.str() + "->" + GetNodeName(s1)] = link;

        /* We save the host into a mapping of RTTs to host */
        if (senders_latency_to_node.count(round_trip_delay) > 0)
        {
          senders_latency_to_node[round_trip_delay].push_back(sender);
        }
        else
        {
          senders_latency_to_node[round_trip_delay] = std::vector<Ptr<Node>>();
          senders_latency_to_node[round_trip_delay].push_back(sender);
        }

        /* Add interfaces to containers */
        switch1Devices.Add(link.Get(1)); // right side
        sendersDevices.Add(link.Get(0)); // left side
      }
    }

    /* Allocating receivers */
    Ptr<Node> last_hop;
    if (enable_nat)
    {
      last_hop = nat;
    }
    else
    {
      last_hop = s2;
    }

    for (uint32_t receiver_index = 0; receiver_index < receivers.GetN(); receiver_index++)
    {
      /* Add node to names*/
      Ptr<Node> receiver = receivers.Get(receiver_index);
      std::stringstream receiver_name;
      receiver_name << "r_" << (receiver_index);
      Names::Add(receiver_name.str(), receiver);

      /* link it with s1*/
      NetDeviceContainer link = csma_hosts.Install(NodeContainer(last_hop, receiver));
      /* Set properties */
      link.Get(0)->GetChannel()->SetAttribute("DataRate", DataRateValue(hostsBandwidth));
      link.Get(1)->GetChannel()->SetAttribute("DataRate", DataRateValue(hostsBandwidth));

      link.Get(0)->GetChannel()->SetAttribute("Delay", TimeValue(PicoSeconds(0)));
      link.Get(1)->GetChannel()->SetAttribute("Delay", TimeValue(PicoSeconds(0)));

      links[GetNodeName(last_hop) + "->" + receiver_name.str()] = link;

      /* Add interfaces to containers */
      if (enable_nat)
      {
        natDevices.Add(link.Get(0)); // left side
      }
      else
      {
        switch2Devices.Add(link.Get(0)); // left side
      }
      receiversDevices.Add(link.Get(1)); // right side
    }

    /* */

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;

    // TODO AND TO FIX: we have a problem when we need to send to prefixes 10.x.x.x then it does an arp request
    // For the time being and as a workaround I will limit the amount of senders to 255 so we are covered up to 10.0.0
    // Usually this should be 255.0.0.0
    // Write some assertion cuz we can only have max 250 devices
    ipv4.SetBase("10.0.0.0", "255.255.255.0");
    /* skips first address so we can set it as gateway*/
    ipv4.NewAddress();
    ipv4.Assign(sendersDevices);

    ipv4.SetBase("20.0.0.0", "255.0.0.0");
    /* skips first address so we can set it as gateway*/
    ipv4.NewAddress();
    ipv4.Assign(receiversDevices);

    // Create the switch netdevice, which will do the packet switching
    /* Set switches metadata */
    std::vector<NetDeviceContainer> switch_devices =
      ConfigureSwitches(s1, s2, switch1Devices, switch2Devices, out_dir_base);

    /* If we have to add the nat switch */
    NetDeviceContainer nat_devs;
    if (enable_nat)
    {
      P4SwitchHelper switch_h("ns3::P4SwitchNAT");
      nat_devs = switch_h.Install<P4SwitchNAT>(nat, natDevices);
    }

    NetDeviceContainer sw1_devs = switch_devices[0];
    NetDeviceContainer sw2_devs = switch_devices[1];

    /* Set the forwarding type and reload tables */
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
      ->SetAttribute("ForwardingType",
        EnumValue(P4SwitchNetDevice::ForwardingType::L3_SPECIAL_FORWARDING));
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))->FillTables();

    if (enable_nat)
    {
      DynamicCast<P4SwitchNetDevice>(sw2_devs.Get(0))
        ->SetAttribute("ForwardingType",
          EnumValue(P4SwitchNetDevice::ForwardingType::PORT_FORWARDING));
      DynamicCast<P4SwitchNetDevice>(sw2_devs.Get(0))
        ->FillTables("control_plane/s2-port-commands.txt");

      /* This has to be removed, since the nat switch does not have a normal forwarding table right now */
      DynamicCast<P4SwitchNetDevice>(nat_devs.Get(0))
        ->SetAttribute("ForwardingType",
          EnumValue(P4SwitchNetDevice::ForwardingType::L3_SPECIAL_FORWARDING));
      DynamicCast<P4SwitchNetDevice>(nat_devs.Get(0))->FillTables();
    }
    else
    {
      DynamicCast<P4SwitchNetDevice>(sw2_devs.Get(0))
        ->SetAttribute("ForwardingType",
          EnumValue(P4SwitchNetDevice::ForwardingType::L3_SPECIAL_FORWARDING));
      DynamicCast<P4SwitchNetDevice>(sw2_devs.Get(0))->FillTables();
    }

    /* Enables bw measurament */
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
      ->EnableOutBandwidthPrint(links["s1->s2"].Get(0));
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
      ->EnableInBandwidthPrint(links["s1->s2"].Get(0));
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
      ->SetBandwidthPrintInterval(bw_print_interval);
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))->StartBandwidthMeasurament();

    /* Set Addresses */
    /* Need to think how to deal with prefixes in a nice way without making the topology infinitely big*/
    /* I can do some address translation for the ones that are prefix based? */

    /* Assign all the gateways after we have the addresses assigned */
    /* We also do ARP spoofing here for the gateway */
    Ipv4StaticRoutingHelper helper;

    for (uint32_t i = 0; i < senders.GetN(); i++)
    {
      Ptr<Node> sender = senders.Get(i);
      Ptr<Ipv4> ipv4 = sender->GetObject<Ipv4>();
      Ptr<Ipv4StaticRouting> Ipv4stat = helper.GetStaticRouting(ipv4);
      Ipv4stat->SetDefaultRoute(Ipv4Address("10.0.0.1"), 1);

      /* ARP Spoofing */
      // https://gist.github.com/SzymonSzott/de5c431d687f7b3a0b10743af6ac7ce2

      Ptr<Ipv4L3Protocol> ip = sender->GetObject<Ipv4L3Protocol>();
      NS_ASSERT(ip != 0);
      ObjectVectorValue interfaces;
      ip->GetAttribute("InterfaceList", interfaces);
      for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++)
      {
        Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface>();
        NS_ASSERT(ipIface != 0);
        Ptr<NetDevice> device = ipIface->GetDevice();
        NS_ASSERT(device != 0);
        Ipv4Address ipAddr = ipIface->GetAddress(0).GetLocal();

        // ignore localhost interface...
        if (ipAddr == Ipv4Address("127.0.0.1"))
        {
          continue;
        }
        else
        {
          /* set the arp table */
          PointerValue ptr;
          ipIface->GetAttribute("ArpCache", ptr);
          Ptr<ArpCache> arp = ptr.Get<ArpCache>();

          arp->SetAliveTimeout(Seconds(3600 * 24));

          Ipv4Address ipaddr = Ipv4Address("10.0.0.1");
          ArpCache::Entry* entry = arp->Add(ipaddr);
          Mac48Address addr = Mac48Address("00:00:10:00:00:01");

          Ipv4Header ipv4Hdr;
          ipv4Hdr.SetDestination(ipaddr);
          Ptr<Packet> p = Create<Packet>(100);
          entry->MarkWaitReply(ArpCache::Ipv4PayloadHeaderPair(p, ipv4Hdr));
          entry->MarkAlive(addr);
          entry->DequeuePending();

          //Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> ("output/arp.txt", std::ios::out);
          //arp->PrintArpCache(stream);

          /* Not needed anymore since we use the direct pointer from the original */
          //ipIface->SetAttribute ("ArpCache", PointerValue (arp));
        }
      }
    }

    for (uint32_t i = 0; i < receivers.GetN(); i++)
    {
      Ptr<Node> receiver = receivers.Get(i);
      Ptr<Ipv4> ipv4 = receiver->GetObject<Ipv4>();
      Ptr<Ipv4StaticRouting> Ipv4stat = helper.GetStaticRouting(ipv4);
      Ipv4stat->SetDefaultRoute(Ipv4Address("20.0.0.1"), 1);

      /* ARP Spoofing */
      // https://gist.github.com/SzymonSzott/de5c431d687f7b3a0b10743af6ac7ce2

      Ptr<Ipv4L3Protocol> ip = receiver->GetObject<Ipv4L3Protocol>();
      NS_ASSERT(ip != 0);
      ObjectVectorValue interfaces;
      ip->GetAttribute("InterfaceList", interfaces);
      for (ObjectVectorValue::Iterator j = interfaces.Begin(); j != interfaces.End(); j++)
      {
        Ptr<Ipv4Interface> ipIface = (*j).second->GetObject<Ipv4Interface>();
        NS_ASSERT(ipIface != 0);
        Ptr<NetDevice> device = ipIface->GetDevice();
        NS_ASSERT(device != 0);
        Ipv4Address ipAddr = ipIface->GetAddress(0).GetLocal();

        // ignore localhost interface...
        if (ipAddr == Ipv4Address("127.0.0.1"))
        {
          continue;
        }
        else
        {
          /* set the arp table */
          PointerValue ptr;
          ipIface->GetAttribute("ArpCache", ptr);
          Ptr<ArpCache> arp = ptr.Get<ArpCache>();

          arp->SetAliveTimeout(Seconds(3600 * 24));

          Ipv4Address ipaddr = Ipv4Address("20.0.0.1");
          ArpCache::Entry* entry = arp->Add(ipaddr);
          Mac48Address addr = Mac48Address("00:00:20:00:00:01");

          Ipv4Header ipv4Hdr;
          ipv4Hdr.SetDestination(ipaddr);
          Ptr<Packet> p = Create<Packet>(100);
          entry->MarkWaitReply(ArpCache::Ipv4PayloadHeaderPair(p, ipv4Hdr));
          entry->MarkAlive(addr);
          entry->DequeuePending();

          //Ptr<OutputStreamWrapper> stream = Create<OutputStreamWrapper> ("output/arp.txt", std::ios::out);
          //arp->PrintArpCache(stream);

          /* Not needed anymore since we use the direct pointer from the original */
          //ipIface->SetAttribute ("ArpCache", PointerValue (arp));
        }
      }
    }

    /* Simple traffic generator */
    // Create an optional packet sink to receive these packets
    NS_LOG_INFO("Create Applications.");

    uint16_t dport_start = 6000;
    uint16_t dport_end = 6100;

    /* Install sinks */
    InstallSinks(receivers, dport_start, dport_end, 0, "TCP");
    /* UDP sinks */
    InstallSinks(receivers, dport_start, dport_end, 0, "UDP");

    //ScheduleTraffic()
    std::string logOutput = out_dir_base + "-first-packet.txt";
    // Flow distribution file
    std::string flowDistFile = in_dir_base + "_" + std::to_string(trace_slice) + ".dist";

    /* Here depending on the experiment we will use a different traffic generator */
    /* So far this is not used */
    if (traffic_type == "StatefulTraceTraffic")
    {
      // Traffic scheduler
      std::unordered_map<std::string, TrafficPrefixStats> traffic_stats =
        StatefulTraceTrafficScheduler(senders_latency_to_node, experiment_rtts, flowDistFile,
          sim_seed, traffic_start, send_duration, fail_time,
          dport_start, dport_end, logOutput);

    }
    /* this is the hybrid generator where we only make real flows of the prefixes that will be failed */
    /* here its a special scenario in which we only fail one prefix at a time, this was used for the caida evals */
    else if (traffic_type == "HybridTraceTraffic" && fail_specific_top_index)
    {

      /* First thing to do is to load all the things and find which prefixes will be failed */

      /* Load all the prefixes available in the stateful file */
      std::string prefix_dist_file = in_dir_base + "_" + std::to_string(trace_slice) + ".dist";
      /* Local top prefixes */

      /* Prefixes in the distribution vector so we can index it in a ranked way*/
      std::vector<uint32_t> stateful_dist_prefixes =
        GetPrefixesFromDistVector(prefix_dist_file);

      /* Another layer here would be to load the "Allowed to fail prefixes
    file". But at the moment we do not have that since we do not use
    stateless analysis */

    /* Prefixes to fail */
      std::unordered_set<uint32_t> prefixes_to_fail;

      NS_ASSERT_MSG(fail_specific_top_index > 0,
        "FailSpecificTopIndex needs to be bigger than 0");
      NS_ASSERT_MSG(fail_specific_top_index - 1 < stateful_dist_prefixes.size(),
        "FailSpecificTopIndex needs to be smaller than the number of prefixes in "
        "the dist file");

      /* Add special prefix */
      prefixes_to_fail.insert(stateful_dist_prefixes[fail_specific_top_index - 1]);
      std::cout << "Prefix to fail " << stateful_dist_prefixes[fail_specific_top_index - 1] << std::endl;

      // used to comppensate the switch to switch delay 
      double rtt_shift = (double(switch_to_switch_delay) / 1000000) * 2;

      std::unordered_map<std::string, TrafficPrefixStats> traffic_stats =
        HybridTrafficScheduler(senders_latency_to_node, prefixes_to_fail, experiment_rtts, rtt_shift,
          flowDistFile, sim_seed, traffic_start, send_duration,
          fail_time, dport_start, dport_end, "");

      /* Load bin file without the prefixes to fail */

      /* Schedule bin file */
      /* Tell the Switch how to do early stops */
      /* Set the early drop value */
      if (switch_type == "NetSeer" || switch_type == "Fancy")
      {
        DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
          ->SetAttribute("EarlyStopCounter", UintegerValue(num_drops));
      }

      /* Select fail time between: 0 + some delta and last packet - 2 seconds */
      std::string prefix;
      uint32_t prefix_with_0_value;
      for (auto const& d : prefixes_to_fail)
      {
        prefix_with_0_value = d;
        prefix = Ipv4AddressToString(Ipv4Address(d + 1));
      }

      TrafficPrefixStats prefix_stats = traffic_stats[prefix];
      double prefix_duration = prefix_stats.last_packet - prefix_stats.first_packet;
      double prefix_fail_margin = prefix_duration * 3 / 4;

      Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable>();
      double failure_time = random_generator->GetValue(
        traffic_start, prefix_stats.last_packet - prefix_fail_margin);

      std::cout << "prefix " << prefix << "  "
        << "first packet " << prefix_stats.first_packet << " max duration "
        << prefix_stats.last_packet << " fail time " << failure_time << std::endl;

      sim_metadata["FailTime"] = std::to_string(failure_time);

      /* Check if prefix is a top prefix or not so we can disable the dedicated counter entries */
      std::string top_file = in_dir_base + ".top"; /* use global top */

      std::vector<uint32_t> top_prefixes = LoadTopPrefixesInt(top_file);

      bool top_prefix_found = false;
      int top_prefix_index = -1;
      for (uint32_t i = 0; i < num_top_entries_system; i++)
      {
        if (prefix_with_0_value == top_prefixes[i])
        {
          top_prefix_found = true;
          top_prefix_index = int(i);
        }
      }

      if (top_prefix_found)
      {
        std::cout << "Failing a top prefix with ID " << top_prefix_index + 1 << std::endl;
      }
      /* Disable top entries */
      else
      {
        std::cout << "Prefix is not a top prefix " << prefix << std::endl;
        /* Disabling top entries*/

        /* Cancel state machine events of the dedicated counter entries, in theory this should stop things */
        /* Tell the Switch how to do early stops */

        if (switch_type == "Fancy")
        {
          DynamicCast<P4SwitchFancy>(sw1_devs.Get(0))->DisableTopEntries();
          DynamicCast<P4SwitchFancy>(sw2_devs.Get(0))->DisableTopEntries();
        }
      }

      /* Schedule failures */
      Simulator::Schedule(Seconds(failure_time), &HybridFailureScheduler, prefixes_to_fail,
        sw1_devs);
    }
    /* In order to speed up things a bit, we generate a hybrid type of traffic:
       1. For all flows that do not experience failures we use the old raw-send mechanism
       2. For all the flows that will experience any type of failure, our TCP/UDP send-rate generator
    */

    /* This was used for the caida experiments in which we failed with bursts
    and different zooming speeds, however, might not be used for the paper
    plots */
    else if (traffic_type == "HybridTraceTraffic")
    {

      /* First thing to do is to load all the things and find which prefixes will be failed */

      /* Load all the prefixes available in the stateful file */
      std::string prefix_dist_file = in_dir_base + "_" + std::to_string(trace_slice) + ".dist";
      /* Local top prefixes */
      std::string top_file = in_dir_base + "_" + std::to_string(trace_slice) + ".top";

      /* Prefixes in the distribution */
      std::unordered_set<uint32_t> stateful_dist_prefixes =
        GetPrefixesFromDistSet(prefix_dist_file);

      /* Load top prefixes file which tells us the top prefixes from the BIN data*/
      /* candidate failable prefixes */
      std::vector<uint32_t> top_prefixes = LoadTopPrefixesInt(top_file);

      if (num_top_entries_traffic > top_prefixes.size())
      {
        // so we do not overflow
        num_top_entries_traffic = top_prefixes.size();
      }

      /* Another layer here would be to load the "Allowed to fail prefixes
    file". But at the moment we do not have that since we do not use
    stateless analysis */

    /* Prefixes to fail */
      std::unordered_set<uint32_t> prefixes_to_fail;

      /* look for prefixes and add them until we have the number we want to fail */
      Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable>();
      uint32_t count = 0;
      while (prefixes_to_fail.size() != num_drops)
      {
        count++;
        uint32_t prefix =
          top_prefixes[random_generator->GetInteger(0, num_top_entries_traffic - 1)];

        /* Check that the prefix is in the stateful_dist */
        if (stateful_dist_prefixes.count(prefix) > 0)
        {
          if (prefixes_to_fail.count(prefix) == 0)
          {
            prefixes_to_fail.insert(prefix);
            count = 0;
          }
        }

        // In case this never finishes
        if (count > 200000)
        {
          NS_LOG_UNCOND("Failed trying to add " << num_drops << " prefixes to fail");
          break;
        }
      }

      /* Now here we have the list of prefixes we want to fail */
      std::cout << "Prefixes to fail" << std::endl;

      std::string failed_prefixes_out_file = out_dir_base + "-failed_prefixes.txt";

      AsciiTraceHelper asciiTraceHelper;
      Ptr<OutputStreamWrapper> file =
        asciiTraceHelper.CreateFileStream(failed_prefixes_out_file);

      for (auto const& d : prefixes_to_fail)
      {
        *(file->GetStream()) << Ipv4Address(d) << "\n";
        std::cout << Ipv4Address(d) << std::endl;
      }
      file->GetStream()->flush();

      // used to comppensate the switch to switch delay 
      double rtt_shift = (double(switch_to_switch_delay) / 1000000) * 2;

      //std::cout << "prefixes to fail length " << prefixes_to_fail.size() << std::endl;

      /* Once we have the list of prefixes that can be failed we will schedule the traffic */
      /* Start TCP scheduler with the prefixes that will get affected*/

      std::unordered_map<std::string, TrafficPrefixStats> traffic_stats =
        HybridTrafficScheduler(senders_latency_to_node, prefixes_to_fail, experiment_rtts, rtt_shift,
          flowDistFile, sim_seed, traffic_start, send_duration,
          fail_time, dport_start, dport_end, logOutput);

      /* Load bin file without the prefixes to fail */

      /* Schedule bin file */

      /* Tell the Switch how to do early stops */
      /* Set the early drop value */
      if (switch_type == "NetSeer" || switch_type == "Fancy")
      {
        DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
          ->SetAttribute("EarlyStopCounter", UintegerValue(num_drops));
      }

      /* Schedule failures */
      Simulator::Schedule(Seconds(fail_time), &HybridFailureScheduler, prefixes_to_fail,
        sw1_devs);
    }
    /* This was used for the heat maps */
    /* Used for both the evaluation of the tree
    /* And dedicated counter entries */
    else if (traffic_type == "StatefulSyntheticTraffic")
    {
      double synthetic_udp_share =
        0.05; // will represent 5% of the traffic and bytes, since we do not care that much here if its flows.
      synthetic_udp_share = 0; /* we used 0% udp so we dont cheat */
      // but why would it be cheating? some internet traffic has udp */
      /* traffic scheduler */

      /* IMPORTANT */
      /* we use this rtt to correct the one introduced by the switch to switch */
      double single_host_rtt;
      for (auto& it : senders_latency_to_node)
      {
        // Get the single rtt
        single_host_rtt = it.first;
      }
      //std::cout << "single host rtt" << single_host_rtt << std::endl;

      // REAL OLD FUNCTION IN FANCY EVAL
      std::vector<FlowMetadata> flowDist = StatefulSyntheticTrafficScheduler(
        senders_latency_to_node, single_host_rtt, sim_seed, flows_per_sec,
        synthetic_num_prefixes, send_rate, traffic_start, send_duration, dport_start,
        dport_end, logOutput, synthetic_udp_share);

      // Parameters
      //double warm_up_time = 2;
      // Flows per sec == total flows
      // Two new parameters
      // elephant_flows 
      // elpehant_byte_share

     //std::vector<FlowMetadata> flowDist = StatefulSyntheticTrafficSchedulerOneShot(
     //  senders_latency_to_node, single_host_rtt, sim_seed, flows_per_sec,
     //  synthetic_num_prefixes, send_rate, traffic_start, warm_up_time, send_duration, num_elephant_flows, elephant_share,
     //  dport_start, dport_end, logOutput, synthetic_udp_share);

      /* Set the early drop value */
      /* TODO: make this a parameter, for the latest experiments I set it to 0 */
      if (switch_type == "NetSeer" || switch_type == "Fancy")
      {
        // NOTE: this can be set for 0 if we dont want to ever stop fancy
        DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
          ->SetAttribute("EarlyStopCounter", UintegerValue(num_drops));
      }

      /* failures scheduler */
      if (fail_time >= 0)
      {
        Simulator::Schedule(Seconds(fail_time), &StatefulSyntheticFailureScheduler,
          flowDist, sw1_devs, num_drops);
        //Simulator::Schedule (Seconds(2.2), &StatefulSyntheticRestoreScheduler, flowDist, sw1_devs, num_drops);
      }
    }

    /* PCAP SETTINGS */

    /* Set pcap logs */
    if (pcap_enabled)
    {
      //csma_hosts.EnablePcap ("output/main-topo", links["h_26_1->s1"].Get (0), true);
      //csma_hosts.EnablePcap("output/main-topo", links["h_22_0->s1"].Get(0), true);
      //csma_hosts.EnablePcap("output/main-topo", links["h_54_0->s1"].Get(0), true);
      csma_hosts.EnablePcap("output/main-topo", links["s1->s2"].Get(0), true);
      csma_hosts.EnablePcap("output/main-topo", links["s1->s2"].Get(1), true);
      if (enable_nat)
      {
        csma_hosts.EnablePcap("output/main-topo", links["s2->nat"].Get(0), true);
      }
      else
      {
        csma_hosts.EnablePcap("output/main-topo", links["s2->r_0"].Get(1), true);
        csma_hosts.EnablePcap("output/main-topo", links["s2->r_1"].Get(1), true);
      }
    }
  }
  /* Old logic for the simple topology without RTT and many TCP flows */
  /* PCAP inject and stateless flows*/
  else
  {
    /* sets port forwarding as forwarding strategy */
    Config::SetDefault("ns3::P4SwitchNetDevice::ForwardingType",
      EnumValue(P4SwitchNetDevice::ForwardingType::PORT_FORWARDING));

    NodeContainer hosts;
    hosts.Create(6);

    NodeContainer switches;
    switches.Create(3);

    Ptr<Node> h0 = hosts.Get(0);
    Ptr<Node> h1 = hosts.Get(1);
    Ptr<Node> h2 = hosts.Get(2);
    Ptr<Node> h3 = hosts.Get(3);
    Ptr<Node> h4 = hosts.Get(4);
    Ptr<Node> h5 = hosts.Get(5);

    Ptr<Node> s1 = switches.Get(0);
    Ptr<Node> s2 = switches.Get(1);

    // Name all the nodes
    Names::Add("h0", h0);
    Names::Add("h1", h1);
    Names::Add("h2", h2);
    Names::Add("h3", h3);
    Names::Add("h4", h4);
    Names::Add("h5", h5);

    Names::Add("s1", s1);
    Names::Add("s2", s2);

    NS_LOG_INFO("Build Topology");
    CsmaHelper csma;

    /* Hosts Links */
    DataRate hostsBandwidth(network_bandwidth);
    /* Inter Switch Link */
    DataRate switchBandwidth(network_bandwidth);

    csma.SetChannelAttribute("DataRate", DataRateValue(switchBandwidth));
    /* using the switch to switch delay */
    csma.SetChannelAttribute("Delay", TimeValue(MicroSeconds(switch_to_switch_delay)));
    csma.SetDeviceAttribute("Mtu", UintegerValue(1500));
    csma.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

    // Create the csma links, from each terminal to the switch
    NetDeviceContainer switch1Devices;
    NetDeviceContainer switch2Devices;
    NetDeviceContainer hostDevices;

    NetDeviceContainer link0 = csma.Install(NodeContainer(h0, s1));
    NetDeviceContainer link1 = csma.Install(NodeContainer(h1, s1));
    NetDeviceContainer link2 = csma.Install(NodeContainer(h2, s1));

    NetDeviceContainer link3 = csma.Install(NodeContainer(s1, s2));

    NetDeviceContainer link4 = csma.Install(NodeContainer(s2, h3));
    NetDeviceContainer link5 = csma.Install(NodeContainer(s2, h4));
    NetDeviceContainer link6 = csma.Install(NodeContainer(s2, h5));

    // Add net devices to respective containers

    // Switch 1
    switch1Devices.Add(link0.Get(1));
    switch1Devices.Add(link1.Get(1));
    switch1Devices.Add(link2.Get(1));
    switch1Devices.Add(link3.Get(0));

    // Switch 2
    switch2Devices.Add(link3.Get(1));
    switch2Devices.Add(link4.Get(0));
    switch2Devices.Add(link5.Get(0));
    switch2Devices.Add(link6.Get(0));

    hostDevices.Add(link0.Get(0));
    hostDevices.Add(link1.Get(0));
    hostDevices.Add(link2.Get(0));
    hostDevices.Add(link4.Get(1));
    hostDevices.Add(link5.Get(1));
    hostDevices.Add(link6.Get(1));

    /* Assign switch type depending on the switch we want to use */

    // Create the switch netdevice, which will do the packet switching
    /* Set switches metadata */
    std::vector<NetDeviceContainer> switch_devices =
      ConfigureSwitches(s1, s2, switch1Devices, switch2Devices, out_dir_base);

    NetDeviceContainer sw1_devs = switch_devices[0];
    NetDeviceContainer sw2_devs = switch_devices[1];

    /* Enables bw measurament */
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))->EnableOutBandwidthPrint(link3.Get(0));
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))->EnableInBandwidthPrint(link3.Get(0));

    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
      ->SetBandwidthPrintInterval(bw_print_interval);
    DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))->StartBandwidthMeasurament();

    // Add internet stack to the terminals
    InternetStackHelper internet;
    internet.Install(hosts);

    /* IP address assignment */

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    ipv4.Assign(hostDevices);

    // Create an optional packet sink to receive these packets
    NS_LOG_INFO("Create Applications.");

    uint16_t dport = 6001;

    for (uint32_t i = 0; i < 1000; i++)
    {
      PacketSinkHelper sink("ns3::TcpSocketFactory",
        Address(InetSocketAddress(Ipv4Address::GetAny(), dport + i)));
      ApplicationContainer app1;
      app1 = sink.Install(h3);
      app1.Start(Seconds(0.0));
    }

    /* Senders */
    /* Inject traffic from caida traces. This generator also deals with packet
    losses something that might not be the best design */
    if (traffic_type == "PcapReplayTraffic")
    {
      /* Enable early stops*/
      DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))->SetAttribute("EarlyStopCounter", UintegerValue(num_top_drops));

      Ptr<TraceSendApplication> trace_send_app = CreateObject<TraceSendApplication>();
      h0->AddApplication(trace_send_app);
      trace_send_app->SetAttribute("Duration", DoubleValue(send_duration));
      trace_send_app->SetAttribute("Scaling", StringValue(scaling_speed));
      trace_send_app->SetAttribute("DstAddr",
        AddressValue(GetNodeNetDevice(main_dst)->GetAddress()));
      trace_send_app->SetAttribute("TopFailType", StringValue(top_drop_type));
      trace_send_app->SetAttribute("BottomFailType", StringValue(bottom_drop_type));
      trace_send_app->SetAttribute("NumTopEntries", UintegerValue(num_top_entries_traffic));
      trace_send_app->SetAttribute("NumTopFails", UintegerValue(num_top_drops));
      trace_send_app->SetAttribute("NumBottomFails", UintegerValue(num_bottom_drops));
      trace_send_app->SetAttribute(
        "InFile", StringValue(in_dir_base + "_" + std::to_string(trace_slice) + ".bin"));
      trace_send_app->SetAttribute(
        "TopFile", StringValue(in_dir_base + "_" + std::to_string(trace_slice) + ".top"));
      trace_send_app->SetAttribute("AllowedToFail", StringValue(allowed_to_fail_file));
      trace_send_app->SetAttribute("OutFile",
        StringValue(out_dir_base + "-failed_prefixes.txt"));
      trace_send_app->SetStartTime(Seconds(traffic_start));
    }
    /* Generates several stateless flows at some rate and number of flows and failures */
    else if (traffic_type == "RawSyntheticTraffic")
    {
      /* TODO: REMOVE? This was added to speed up tests last minute*/
      /* Tell the Switch how to do early stops */
      /* Set the early drop value */
      if (switch_type == "NetSeer" || switch_type == "Fancy")
      {
        DynamicCast<P4SwitchNetDevice>(sw1_devs.Get(0))
          ->SetAttribute("EarlyStopCounter", UintegerValue(num_drops));
      }

      Ptr<RawSendApplication> raw_send_app = CreateObject<RawSendApplication>();
      h0->AddApplication(raw_send_app);
      raw_send_app->SetAttribute("PacketSize", UintegerValue(1024));
      raw_send_app->SetAttribute("SendRate", DataRateValue(DataRate(send_rate)));
      raw_send_app->SetAttribute("Duration", DoubleValue(send_duration));
      raw_send_app->SetAttribute("NumFlows", UintegerValue(num_flows));
      raw_send_app->SetAttribute("DstAddr",
        AddressValue(GetNodeNetDevice(main_dst)->GetAddress()));
      raw_send_app->SetAttribute("NumDrops", UintegerValue(num_drops));
      raw_send_app->SetAttribute("OutFile",
        StringValue(out_dir_base + "-synthetic-sent_flows.txt"));
      raw_send_app->SetStartTime(Seconds(traffic_start));
    }
    /* Sends some real flows using host stack */
    else if (traffic_type == "TestTraffic")
    {

      //TrafficControlHelper tch;
      //tch.Uninstall(link0.Get (0));

      BulkSendHelper sender("ns3::TcpSocketFactory",
        Address(InetSocketAddress(GetNodeIp(h3), dport)));

      sender.SetAttribute("SendSize", UintegerValue(1024));

      /* compute amount of bytes to send */
      uint32_t bytes_to_send =
        uint32_t(DataRate(send_rate).GetBitRate() / 8 * send_duration);
      std::cout << "bytes " << bytes_to_send << std::endl;
      sender.SetAttribute("MaxBytes", UintegerValue(bytes_to_send));
      sender.SetAttribute("MaxBytes", UintegerValue(1000000));

      for (int i = 0; i < 25; i++)
      {
        ApplicationContainer app;
        sender.SetAttribute("Remote",
          AddressValue(InetSocketAddress(GetNodeIp(h3), dport + i)));
        app = sender.Install(h0);
        // Start the application

        double delta = 0;
        if (i > 0)
        {
          delta = 0.01;
        }
        app.Start(Seconds(traffic_start + delta));
        app.Stop(Seconds(sim_duration));
      }

      for (uint32_t i = 0; i < h0->GetNApplications(); i++)
      {
        Ptr<Application> app = h0->GetApplication(i);
        TimeValue t;
        app->GetAttribute("StartTime", t);
        std::cout << "start time :" << t.Get() << std::endl;
      }
    }

    if (pcap_enabled)
    {
      csma.EnablePcap("output/main", link0.Get(0), true);
      csma.EnablePcap("output/main", link3.Get(0), true);
      csma.EnablePcap("output/main", link4.Get(0), true);
    }
  }

  // Set Link failure
  //Simulator::Schedule(Seconds(2.3305), &FailLink, link3);
  //Simulator::Schedule(Seconds(2.9305), &RecoverLink, link3);
  //SetUniformDropRate(link3, 0.01);

  //DataRate d = DataRate(1000000000);
  //std::cout << "data rate " << data_rate_to_str(d) << std::endl;

  /* Time scale */
  PrintNow(print_interval);

  /* Prints first node routing table Just for debugging*/
  //Ptr<OutputStreamWrapper> routingStream =
  //    Create<OutputStreamWrapper> ("output/routes.txt", std::ios::out);
  ///* try to set a default route*/
  ///* prints routing table*/
  //Ptr<Ipv4> ipv4 = Names::Find<Node> ("h_0_0")->GetObject<Ipv4> ();
  //if (ipv4)
  //  {
  //    Ptr<Ipv4RoutingProtocol> rp = ipv4->GetRoutingProtocol ();
  //    NS_ASSERT (rp);
  //    rp->PrintRoutingTable (routingStream, Time::S);
  //  }

  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(sim_duration));
  Simulator::Run();

  // Save total simulation time
  float real_simulation_time = (float(clock() - simulation_execution_time) / CLOCKS_PER_SEC);
  sim_metadata["RealSimulationTime"] = std::to_string(real_simulation_time);
  SaveSimulationMetadata(out_dir_base + ".info", sim_metadata);

  Simulator::Destroy();
  NS_LOG_INFO("Done.");
}