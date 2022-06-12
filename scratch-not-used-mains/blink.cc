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


/*  this is blink main i checked */
/* https://github.com/nsg-ethz/ns-3-dev-git/blob/b3ce70649db14053f40051858e825d18dd3a5fe7/scratch/swift-p4.cc*/

#include <fstream>
#include <ctime>
#include <set>
#include <string>
#include <iostream>
#include <unordered_map>
#include <stdlib.h>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/csma-module.h"
#include "ns3/mobility-module.h"
#include "ns3/netanim-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/traffic-generation-module.h"
#include "ns3/flow-monitor-module.h"

#include "ns3/utils-module.h"

// TOPOLOGY
//+---+                                                 +---+
//|s1 |                                           +-----+d1 |
//+---+----+                                      |     +---+
//         |                                      |
//  .      |    +------+              +------+    |       .
//  .      +----+      |              |      +----+       .
//  .           |  sw1 +--------------+  sw2 |            .
//  .           |      |              |      +---+        .
//  .      +----+------+              +------+   |        .
//         |                                     |
//+---+    |                                     |      +---+
//|sN +----+                                     +------+dN |
//+---+                                                 +---+

using namespace ns3;

std::string fileNameRoot = "swift-p4";    // base name for trace files, etc

NS_LOG_COMPONENT_DEFINE (fileNameRoot);

const char *file_name = g_log.Name();
std::string script_name = file_name;

int
main(int argc, char *argv[]) {

  //INITIALIZATION
  std::clock_t simulation_execution_time = std::clock();


  //Set simulator's time resolution (click)
  Time::SetResolution(Time::NS);

  //Fat tree parameters
  DataRate networkBandwidth("10Gbps");
  DataRate sendersBandwidth("4Gbps");
  DataRate receiversBandwidth("4Gbps");

  //Command line arguments
  std::string simulationName = "test";
  std::string outputDir = "outputs/";

  std::string inputDir = "swift_datasets/test/";
  std::string prefixes_failures_file = "";
  std::string prefixes_features_file = "";

  uint16_t queue_size = 1000;
  uint64_t runStep = 1;
  double rtt_shift = 1;

  bool enable_loss = false;
  double prefixes_loss = 0;
  uint64_t network_delay = 1; //ns?
  uint16_t num_hosts_per_rtt = 1;
  uint16_t num_servers_per_prefix = 1;
  uint32_t flowsPersec = 100;
  double duration = 5;
  double failure_time = 0;
  double stop_time = 0;
  double forced_stop_time = 0;
  bool debug = false;
  bool save_pcap = false;
  bool fail_all_prefixes = false;

  CommandLine cmd;

  //Misc
  cmd.AddValue("Debug", "If debug enabled", debug);

  //Naming and in/out folders
  cmd.AddValue("SimulationName", "Name of the experiment: ", simulationName);
  cmd.AddValue("OutputDir", "Set it to something if you want to store the simulation output "
                            "in a particular folder inside outputs", outputDir);
  cmd.AddValue("InputDir", "Where to find all the input data", inputDir);
  cmd.AddValue("PrefixFailures", "", prefixes_failures_file);
  cmd.AddValue("PrefixFeatures", "", prefixes_features_file);

  //General
  //Links properties
  cmd.AddValue("LinkBandwidth", "Bandwidth of link, used in multiple experiments", networkBandwidth);
  cmd.AddValue("NetworkDelay", "Added delay between nodes", network_delay);
  cmd.AddValue("EnableLoss", "Enable Error Per Prefix", enable_loss);
  cmd.AddValue("PrefixesLoss", "Sets the same loss for all prefixes", prefixes_loss);
  cmd.AddValue("EnablePcap", "Save traffic in a pcap file", save_pcap);
  cmd.AddValue("FailAll", "If enabled all prefixes will be failed ignoring what the prefixes_to_fail file says",
               fail_all_prefixes);

  //Experiment
  cmd.AddValue("QueueSize", "Interfaces Queue length", queue_size);
  cmd.AddValue("RunStep", "Random generator starts at", runStep);
  cmd.AddValue("NumHostsPerRtt", "Number of hosts in each side", num_hosts_per_rtt);
  cmd.AddValue("NumServersPerPrefix", "Number of prefixes a Prefix has", num_servers_per_prefix);
  cmd.AddValue("FlowsPerSec", "Number of hosts in each side", flowsPersec);
  cmd.AddValue("Duration", "", duration);
  cmd.AddValue("FailureTime", "Time when all prefixes will be lost", failure_time);
  cmd.AddValue("StopTime", "Time when the simulation finishes", stop_time);
  cmd.AddValue("ForcedStopTime", "Time when we force the simulation to stop, this time is from the real world", forced_stop_time);
  cmd.AddValue("RttShift", "Paramater to shift the used RTT distribution. It will shift senders and flows rtts",
               rtt_shift);

  cmd.Parse(argc, argv);

  //Change that if i want to get different random values each run otherwise i will always get the same.
  RngSeedManager::SetRun(runStep);   // Changes run number from default of 1 to 7
  RngSeedManager::SetSeed(runStep);

  if (debug) {
//		LogComponentEnable("Ipv4GlobalRouting", LOG_DEBUG);
    //LogComponentEnable("Ipv4GlobalRouting", LOG_ERROR);
    LogComponentEnable("swift-p4", LOG_DEBUG);
    LogComponentEnable("swift-utils", LOG_LEVEL_DEBUG);
    LogComponentEnable("utils", LOG_DEBUG);
    LogComponentEnable("traffic-generation", LOG_DEBUG);
    LogComponentEnable("swift-generation", LOG_DEBUG);
    //LogComponentEnable("custom-bulk-app", LOG_DEBUG);
    //LogComponentEnable("PacketSink", LOG_ALL);
    //LogComponentEnable("TcpSocketBase", LOG_ALL);
  }

  NS_LOG_DEBUG("Simulation name " << simulationName);

  //Update root name
  std::ostringstream run;
  run << runStep;

  //Timeout calculations (in milliseconds)
  std::string outputNameRoot = outputDir;

  outputNameRoot = outputNameRoot + simulationName + "/";
  system(("mkdir -p " + outputNameRoot).c_str());

  //copy input files that should go to output
  if (file_exists(inputDir + "prefixes_stats.txt")) {
    system(("cp " + inputDir + "prefixes_stats.txt " + outputNameRoot + "prefixes_stats.txt").c_str());
  }
  if (file_exists(inputDir + "subnetwork_prefixes.txt")) {
    system(("cp " + inputDir + "subnetwork_prefixes.txt " + outputNameRoot + "subnetwork_prefixes.txt").c_str());
  }
  if (file_exists(inputDir + "subnetwork_shares.txt")) {
    system(("cp " + inputDir + "subnetwork_shares.txt " + outputNameRoot + "subnetwork_shares.txt").c_str());
  }
  if (file_exists(inputDir + "rtt_cdfs.txt")) {
    system(("cp " + inputDir + "rtt_cdfs.txt " + outputNameRoot + "rtt_cdfs.txt").c_str());
  }
  //Copy faliure files to output
  if (prefixes_failures_file != "") {
    system(("cp " + inputDir + prefixes_failures_file + " " + outputNameRoot + prefixes_failures_file).c_str());
  }
  if (prefixes_features_file != "") {
    system(("cp " + inputDir + prefixes_features_file + " " + outputNameRoot + prefixes_features_file).c_str());
  }

  //TCP
  uint16_t rtt = 200;
  uint16_t initial_rto = 2000;

  //GLOBAL CONFIGURATION
  //Routing

  Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

  //Tcp Socket (general socket conf)
  Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(10000000));
  Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(10000000));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1446)); //MTU 1446
  Config::SetDefault("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(4294967295));
  Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));

  //Can be much slower than my rtt because packet size of syn is 60bytes
  Config::SetDefault("ns3::TcpSocket::ConnTimeout",
                     TimeValue(MilliSeconds(initial_rto))); // connection retransmission timeout
  Config::SetDefault("ns3::TcpSocket::ConnCount", UintegerValue(5)); //retrnamissions during connection
  Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(10)); //retranmissions
  Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(rtt / 50)));
// 	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue(2));
  Config::SetDefault("ns3::TcpSocket::TcpNoDelay", BooleanValue(true)); //disable nagle's algorithm
  Config::SetDefault("ns3::TcpSocket::PersistTimeout",
                     TimeValue(NanoSeconds(6000000000))); //persist timeout to porbe for rx window

  //Tcp Socket Base: provides connection orientation, sliding window, flow control; congestion control is delegated to the subclasses (i.e new reno)

  Config::SetDefault("ns3::TcpSocketBase::MaxSegLifetime", DoubleValue(10));
  Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(rtt))); //min RTO value that can be set
  Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MicroSeconds(1)));
  Config::SetDefault("ns3::TcpSocketBase::ReTxThreshold", UintegerValue(3)); //same than DupAckThreshold

  Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true)); //enable sack
  Config::SetDefault("ns3::TcpSocketBase::LimitedTransmit", BooleanValue(true)); //enable sack

  //TCP L4
  //TcpNewReno
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
  //Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpVeno::GetTypeId()));
  Config::SetDefault("ns3::RttEstimator::InitialEstimation", TimeValue(MicroSeconds(initial_rto)));//defautlt 1sec

  //QUEUES
  //PFIFO
  Config::SetDefault("ns3::PfifoFastQueueDisc::Limit", UintegerValue(queue_size));

  //Define acsii helper
  AsciiTraceHelper asciiTraceHelper;

  //Define Interfaces

  //Define point to point
  PointToPointHelper p2p;

//   create point-to-point link from A to R
  p2p.SetDeviceAttribute("DataRate", DataRateValue(DataRate(networkBandwidth)));
  p2p.SetChannelAttribute("Delay", TimeValue(MicroSeconds(network_delay)));
  p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));

  //This got deprecated
//    p2p.SetQueue("ns3::DropTailQueue", "Mode", StringValue("QUEUE_MODE_PACKETS"));
//    p2p.SetQueue("ns3::DropTailQueue", "MaxPackets", UintegerValue(queue_size));

  //New way of setting the queue length and mode
  std::stringstream queue_size_str;
  queue_size_str << queue_size << "p";
  p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(queue_size_str.str()));

  //SIMULATION STARTS

  std::clock_t begin_time = std::clock();

  //Load Input files

  //Load RTT CDF
  std::vector<double> src_rtts = GetSubnetworkRtts(inputDir + "rtt_cdfs.txt", simulationName);

  //Modify RTT CDF to scale it to the shift we want
  if (rtt_shift != double(1)) {
    for (auto it = src_rtts.begin(); it != src_rtts.end(); it++) {
      *it = *it * rtt_shift;
    }
  }

//    if (debug) {
//        NS_LOG_DEBUG("Reading RTT CDF file: ");
//        std::cout << "Rtt to src length: " << src_rtts.size() << "\n";
//        for (const auto &e : src_rtts) {
//            std::cout << "Rtt(s): " << e << " Link Delay(s): " << e / 2 << "\n";
//        }
//    }

  //Load Subnetwork Prefix mappings
  PrefixMappings prefixes_mappings = GetSubnetworkPrefixMappings(inputDir + "subnetwork_prefixes.txt", simulationName);

  NS_LOG_DEBUG("Prefix Sizes:");
  NS_LOG_DEBUG("Unique Different Prefixes: " << prefixes_mappings.trace_set.size());
  NS_LOG_DEBUG("Simulation Different Prefixes: " << prefixes_mappings.sim_set.size());

  //Load Trace Prefix Features
  std::unordered_map<std::string, PrefixFeatures> trace_prefixes_features;

  NS_LOG_DEBUG("Features file:" << prefixes_features_file);

  if (prefixes_features_file != "") {
    trace_prefixes_features = GetPrefixFeatures(inputDir + prefixes_features_file, prefixes_mappings.trace_set);
  }

  NS_LOG_DEBUG("Trace prefixes features length: " << trace_prefixes_features.size());

  //Load prefix failures
  std::unordered_map<std::string, std::vector<FailureEvent>> prefix_failures;
  if (prefixes_failures_file != "")
  {
    prefix_failures = GetPrefixFailures(
      inputDir + prefixes_failures_file, simulationName);
  }
  NS_LOG_DEBUG("Prefixes failure length: " << prefix_failures.size());

  //Populate Prefixes Object
  std::unordered_map<std::string, PrefixMetadata> prefixes = LoadPrefixesMetadata(prefixes_mappings,
                                                                                  trace_prefixes_features);
  NS_LOG_DEBUG("Prefixes: " << prefixes.size());

  ////////////////////////////////////////////////////////////////////////////////////////////////////
  //Build Topology
  ////////////////////////////////////////////////////////////////////////////////////////////////////

  //Number of senders and receivers
  int num_senders = src_rtts.size() * num_hosts_per_rtt;
  //TODO: add multiple receivers per prefix
  int num_receivers = prefixes.size();

  //Senders
  NodeContainer senders;
  senders.Create(num_senders);

  //Receivers
  NodeContainer receivers;
  receivers.Create(num_receivers);

  //Single link Switches
  Ptr<Node> sw1 = CreateObject<Node>();
  Ptr<Node> sw2 = CreateObject<Node>();

  // Install Internet stack : aggregate ipv4 udp and tcp implementations to nodes
  InternetStackHelper stack;
  stack.Install(senders);
  stack.Install(receivers);
  stack.Install(sw1);
  stack.Install(sw2);

  //Add two switches
  Names::Add("sw1", sw1);
  Names::Add("sw2", sw2);

  //Install net devices between nodes (so add links) would be good to save them somewhere I could maybe use the namesystem or map
  //Install internet stack to nodes, very easy
  std::unordered_map<std::string, NetDeviceContainer> links;

  //SETTING LINKS: delay and bandwidth.

  //sw1 -> sw2
  //Interconnect middle switches : sw1 -> sw2
  links[GetNodeName(sw1) + "->" + GetNodeName(sw2)] = p2p.Install(NodeContainer(sw1, sw2));
  //Set delay and bandwdith
  links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue(
    NanoSeconds(network_delay)));
  links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0)->SetAttribute("DataRate", DataRateValue(networkBandwidth));
  links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(1)->SetAttribute("DataRate", DataRateValue(networkBandwidth));

  NS_LOG_DEBUG("Link Add: " << GetNodeName(sw1)
                            << " -> "
                            << GetNodeName(sw2)
                            << " delay(ns):"
                            << network_delay
                            << " bandiwdth"
                            << networkBandwidth);

  //Keys are not the real delay its the interface delay x 2 which in oder words it represents the RTT
  std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node;

  //Assing senders, sw1, sw2 ips since they do not depend on the prefixes
  Ipv4AddressHelper address("10.0.1.0", "255.255.255.0");
  address.Assign(links[GetNodeName(sw1) + "->" + GetNodeName(sw2)]);
  address.NewNetwork();

  //Give names to hosts using names class and connect them to the respective switch.
  //Senders
  int host_count = 0;
  uint32_t rtt_cdf_index = 0;

  //Saves senders to delay for debugging
  Ptr<OutputStreamWrapper> sender_to_delay_file = asciiTraceHelper.CreateFileStream(
    outputNameRoot + "sender_to_delay.txt");

  for (NodeContainer::Iterator host = senders.Begin(); host != senders.End(); host++) {

    //Get delay from the sorted rtt cdf vector
    double round_trip_delay = src_rtts[rtt_cdf_index];
    double interface_delay = round_trip_delay / 2;

    //Assign host name, and add it to the names system
    std::stringstream host_name;
    host_name << "s_" << host_count;
    //NS_LOG_DEBUG("Naming Host: " << host_name.str());
    Names::Add(host_name.str(), (*host));

    //save delay in file
    *(sender_to_delay_file->GetStream()) << host_name.str() << " " << interface_delay << "\n";

    //add link host-> sw1
    links[host_name.str() + "->" + GetNodeName(sw1)] = p2p.Install(NodeContainer(*host, sw1));

    //Set delay from rtt_cdf /2
    links[host_name.str() + "->" + GetNodeName(sw1)].Get(0)->SetAttribute("DataRate",
                                                                          DataRateValue(sendersBandwidth));
    links[host_name.str() + "->" + GetNodeName(sw1)].Get(1)->SetAttribute("DataRate",
                                                                          DataRateValue(sendersBandwidth));

    links[host_name.str() + "->" + GetNodeName(sw1)].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue(
      Seconds(interface_delay)));

    NS_LOG_DEBUG("Link Add: " << host_name.str()
                              << " -> "
                              << GetNodeName(sw1)
                              << " delay(s):"
                              << interface_delay
                              << " bandiwdth: "
                              << sendersBandwidth);

    //Assign IP
    address.Assign(links[host_name.str() + "->" + GetNodeName(sw1)]);
    address.NewNetwork();

    //Add host to the mapping
    //Store this node in the latency to node map
    if (senders_latency_to_node.count(round_trip_delay) > 0) {
      senders_latency_to_node[round_trip_delay].push_back(*host);
    } else {
      senders_latency_to_node[round_trip_delay] = std::vector<Ptr<Node>>();
      senders_latency_to_node[round_trip_delay].push_back(*host);
    }
    host_count++;

    if (host_count % num_hosts_per_rtt == 0) {
      rtt_cdf_index++;
    }
  }

//    NS_LOG_DEBUG("Rtt to src length: " << senders_latency_to_node.size());
//    if (debug){
//        for(auto it = senders_latency_to_node.begin(); it != senders_latency_to_node.end(); it++){
//            NS_LOG_DEBUG("rtt: " << it->first << " " << it->second.size() << "\n");
//        }
//    }

  //Receivers
  int dst_index = 0;
  for (auto it = prefixes.begin(); it != prefixes.end(); it++) {

    std::stringstream host_name;
    host_name << "p_" << dst_index;
    //NS_LOG_DEBUG("Naming Host: " << host_name.str());
    Names::Add(host_name.str(), receivers.Get(dst_index));

    //Add link sw2->host

    links[GetNodeName(sw2) + "->" + host_name.str()] = p2p.Install(NodeContainer(sw2, receivers.Get(dst_index)));

    links[GetNodeName(sw2) + "->" + host_name.str()].Get(0)->SetAttribute("DataRate",
                                                                          DataRateValue(receiversBandwidth));
    links[GetNodeName(sw2) + "->" + host_name.str()].Get(1)->SetAttribute("DataRate",
                                                                          DataRateValue(receiversBandwidth));

    links[GetNodeName(sw2) + "->" + host_name.str()].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue(
      NanoSeconds(network_delay)));

    NS_LOG_DEBUG("Link Add: " << GetNodeName(sw2)
                              << " -> "
                              << host_name.str()
                              << " delay(ms):"
                              << network_delay
                              << " bandiwdth:"
                              << sendersBandwidth);

    //Update prefixes object
    it->second.link = links[GetNodeName(sw2) + "->" + host_name.str()];
    it->second.server = receivers.Get(dst_index);

    /*
     * I need to do some hacking in order to asing IPs following the prefixes I got. For the Ip we have to assing
     * a Network Ip : 10.10.0.0
     * a Mask    Ip : 255.255.255.0
     * a Base    Ip : 0.0.128.1
     * Given the prerequisite that the first Ip to assign should be 10.10.128.1
     * To get the base ip we do :      net_ip XOR first_ip. For example:
     * Base = 10.10.0.0 ^ 10.10.128.1 = 0.0.128.1
     * */

//        //Assign IP
//        std::string net_ip = it->second.prefix_ip;
//        std::string base_ip = it->second.ips_to_allocate[0];
//        std::string net_mask = it->second.prefix_mask;
//
//        const char *net_ip_p = net_ip.c_str();
//        const char *base_ip_p = base_ip.c_str();
//        const char *net_mask_p = net_mask.c_str();
//
//        Ipv4Address base_address(base_ip_p);
//        Ipv4Address net_address(net_ip_p);
//        Ipv4AddressHelper address(net_ip_p, net_mask_p, Ipv4Address(base_address.Get() ^ net_address.Get()));

    IpMask prefix_address = GetIpMask(it->first);
    Ipv4AddressHelper address(prefix_address.ip.c_str(), prefix_address.mask.c_str());

    address.Assign(links[GetNodeName(sw2) + "->" + host_name.str()]);

    //Assign some failure rate if (enabled to the interface)
    if (enable_loss) {
      NS_LOG_DEBUG("Setting prefix loss: " << it->first << " " << it->second.features.loss << " "
                                           << it->second.features.minBurst << " " << it->second.features.maxBurst);

      if (prefixes_loss > 0)
      {
        SetFlowErrorModelFromFeatures(it->second.link, 0, prefixes_loss, 1, 1);
      }
      else
      {
        SetFlowErrorModelFromFeatures(it->second.link, 0, it->second.features.loss, it->second.features.minBurst,
                                      it->second.features.maxBurst);
      }
    }
    //still install model, since its needed for the partial failures
    else
    {
      SetFlowErrorModelFromFeatures(it->second.link, 0, 0, 1, 1);
    }
    dst_index++;
  }

  //Assign IPS
  //Uninstall FIFO queue //  //uninstall qdiscs
  TrafficControlHelper tch;
  for (auto it : links) {
    tch.Uninstall(it.second);
  }

  //DEBUG FEATURE
  //Create a ip to node mapping, saves the ip to name for debugging
  std::unordered_map<std::string, Ptr<Node>> ipToNode;

  for (uint32_t host_i = 0; host_i < senders.GetN(); host_i++) {
    Ptr<Node> host = senders.Get(host_i);
    ipToNode[Ipv4AddressToString(GetNodeIp(host))] = host;
  }
  for (uint32_t host_i = 0; host_i < receivers.GetN(); host_i++) {
    Ptr<Node> host = receivers.Get(host_i);
    ipToNode[Ipv4AddressToString(GetNodeIp(host))] = host;
  }
  //Store in a file ip -> node name
  Ptr<OutputStreamWrapper> ipToName_file = asciiTraceHelper.CreateFileStream(outputNameRoot + "ip_to_name.txt");
  for (auto it = ipToNode.begin(); it != ipToNode.end(); it++) {
    *(ipToName_file->GetStream()) << it->first << " " << GetNodeName(it->second) << "\n";
  }
  ipToName_file->GetStream()->flush();

  //Saving metadata file with simulation configurations
  Ptr<OutputStreamWrapper> metadata_file = asciiTraceHelper.CreateFileStream(outputNameRoot + "metadata.txt");
  *(metadata_file->GetStream()) << "simulation_name " << simulationName << "\n";
  *(metadata_file->GetStream()) << "run_step " << runStep << "\n";
  *(metadata_file->GetStream()) << "duration " << duration << "\n";
  *(metadata_file->GetStream()) << "failure_time " << failure_time << "\n";
  *(metadata_file->GetStream()) << "flows_per_sec " << flowsPersec << "\n";
  *(metadata_file->GetStream()) << "input_dir " << inputDir << "\n";
  *(metadata_file->GetStream()) << "output_dir " << outputDir << "\n";
  *(metadata_file->GetStream()) << "queue_size " << queue_size << "\n";
  *(metadata_file->GetStream()) << "network_bw " << networkBandwidth << "\n";
  *(metadata_file->GetStream()) << "senders_bw " << sendersBandwidth << "\n";
  *(metadata_file->GetStream()) << "receivers_bw " << receiversBandwidth << "\n";
  *(metadata_file->GetStream()) << "emulated_congestion_on " << enable_loss << "\n";
  *(metadata_file->GetStream()) << "prefixes_loss " << prefixes_loss << "\n";
  *(metadata_file->GetStream()) << "rtt_cdf_size " << src_rtts.size() << "\n";
  *(metadata_file->GetStream()) << "num_senders " << num_senders << "\n";
  *(metadata_file->GetStream()) << "num_receivers " << num_receivers << "\n";
  *(metadata_file->GetStream()) << "prefixes " << prefixes.size() << "\n";
  *(metadata_file->GetStream()) << "prefixes_failed " << prefix_failures.size() << "\n";
  *(metadata_file->GetStream()) << "rtt_shift " << rtt_shift << "\n";
  metadata_file->GetStream()->flush();

  NS_LOG_DEBUG("Time To Set Hosts: " << float(clock() - begin_time) / CLOCKS_PER_SEC);

  //Populate tables
  begin_time = std::clock();

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();
  float routing_time = float(clock() - begin_time) / CLOCKS_PER_SEC;
  *(metadata_file->GetStream()) << "routing_time " << routing_time << "\n";
  metadata_file->GetStream()->flush();
  NS_LOG_DEBUG("Time Installing Routes: " << routing_time);

  //START TRAFFIC
  //Install Traffic sinks at receivers

  NS_LOG_DEBUG("Starting Sinks");
  begin_time = std::clock();

  std::unordered_map<std::string, std::vector<uint16_t>> hostToPort = InstallSinks(receivers, 35, 0, "TCP");

  NS_LOG_DEBUG("Time Starting Sinks: " << float(clock() - begin_time) / CLOCKS_PER_SEC);


  NS_LOG_DEBUG("Starting Traffic Scheduling");
  begin_time = std::clock();

  NodesUsage nodes_usage = NodesUsage();

//    std::cout << "Rtt to src length: " << senders_latency_to_node.size() << "\n";
//    for(auto it = senders_latency_to_node.begin(); it != senders_latency_to_node.end(); it++){
//        std::cout << "rtt: " << it->first << " " << it->second.size() << "\n";
//    }

  nodes_usage = SendSwiftTraffic(senders_latency_to_node,
                                 src_rtts,
                                 prefixes,
                                 prefixes_mappings,
                                 hostToPort,
                                 inputDir + "flows_per_prefix.txt",
                                 outputNameRoot + "flows.log",
                                 runStep,
                                 flowsPersec,
                                 duration,
                                 rtt_shift);

  //save ranks
  nodes_usage.Save(outputNameRoot + "bytes", false, true);

  std::vector<UsageStruct> receiversVector = nodes_usage.GetReceiversVector();
  float time_scheduling_traffic = float(clock() - begin_time) / CLOCKS_PER_SEC;
  *(metadata_file->GetStream()) << "time_scheduling_traffic " << time_scheduling_traffic << "\n";
  metadata_file->GetStream()->flush();
  NS_LOG_DEBUG("Time Scheduling: " << time_scheduling_traffic);

  //Senders function
  //////////////////
  //TRACES
  ///////////////////

  //Capture traffic between sw1 and sw2 at the first switch.
//  p2p.EnablePcap(outputNameRoot, links[GetNodeName(sw1)+"->"+GetNodeName(sw2)].Get(0), bool(1));
//
//  //Only save TX traffic
  if (save_pcap) {

//    p2p.EnablePcap(outputNameRoot + "tx_rx.pcap", links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0), bool(1));

    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> pcap_file = pcapHelper.CreateFile(
      outputNameRoot + "tx.pcap", std::ios::out,
      PcapHelper::DLT_PPP);
    links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0)->TraceConnectWithoutContext("PhyTxBegin",
                                                                                         MakeBoundCallback(&TracePcap,
                                                                                                           pcap_file));

  }

  NS_LOG_DEBUG("Total Bytes Received By Servers: " << nodes_usage.GetTotalRx());

  
  //TODO: DO this better and cleaner
  //Schedule Prefixes to fail

  Ptr<OutputStreamWrapper> prefixes_failed_file = asciiTraceHelper.CreateFileStream(
    outputNameRoot + "failed_prefixes.txt");

  //we ignore what the prefix file says and we fail all the prefixes at failure_time.
  if (fail_all_prefixes) {

    if (failure_time > 0) {
      for (auto it : prefixes) {
        NetDeviceContainer link = it.second.link;

        NS_LOG_DEBUG("Scheduling prefix fail: " << it.first);

        *(prefixes_failed_file->GetStream()) << it.first << "\n";
        prefixes_failed_file->GetStream()->flush();

        Simulator::Schedule(Seconds(failure_time), &FailLink, link);
      }
    }
  } else {
    for (auto prefix_to_fail: prefix_failures) {

      NetDeviceContainer link = prefixes[prefix_to_fail.first].link;
      NS_LOG_DEBUG("Scheduling prefix fail: " << prefix_to_fail.first);

      *(prefixes_failed_file->GetStream()) << prefix_to_fail.first << "\n";
      prefixes_failed_file->GetStream()->flush();

       //TODO add more debugging messages
      for (auto failure: prefix_to_fail.second) {

        NS_LOG_DEBUG("Failure features: " << failure.failure_time << " "
                                                                     << failure.failure_intensity << " "
                                                                     << failure.recovery_time);

        if (failure.failure_time > 0) {
          if (failure.failure_intensity == 1)
          {
            Simulator::Schedule(Seconds(failure.failure_time), &FailLink, link);
          }
          else
          {
            //partial failure, only interface 0
            Simulator::Schedule(Seconds(failure.failure_time), &ChangeFlowErrorDropRate, link, failure.failure_intensity);
          }
        }
        if (failure.recovery_time > 0) {

          //If it was a total failure we only recover the interface
          if (failure.failure_intensity == 1){
            Simulator::Schedule(Seconds(failure.recovery_time), &RecoverLink, link);
          }

          //set flow error model to 0
          else
          {
            Simulator::Schedule(Seconds(failure.recovery_time), &ChangeFlowErrorDropRate, link ,0);
          }
        }
      }
    }
  }

  //Simulation Starts

  if (stop_time != 0) {
    //We schedule it here otherwise simulations never finish
    std::cout << "\n";
    //TODO PUT A CHECKER IF THE THIUNG IS 0 call the normal function
    //we reset the variable so the previous time does not count
    if (forced_stop_time > 0)
    {
      simulation_execution_time = std::clock();
      PrintNowMemStop(1, simulation_execution_time, forced_stop_time);
    }
    else{
      PrintNowMem(1, simulation_execution_time);
    }

    Simulator::Stop(Seconds(stop_time));

    *(metadata_file->GetStream()) << "stop_time "
                                  << stop_time << "\n";

    
  }
  Simulator::Run();

  //This will run after a stop

  //if we force stoped the simulation then we trite that down
  if (forced_stop_time > 0 and  (float(clock() - simulation_execution_time) / CLOCKS_PER_SEC) > forced_stop_time)
  {
    *(metadata_file->GetStream()) << "forced_stop: 1\n";

  }
  else
  {
    *(metadata_file->GetStream()) << "forced_stop: 0\n";
  }

  *(metadata_file->GetStream()) << "real_time_duration "
                                << (float(clock() - simulation_execution_time) / CLOCKS_PER_SEC) << "\n";
  metadata_file->GetStream()->flush();
  Simulator::Destroy();

  return 0;

}