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

// Network topology
//


#include <iostream>
#include <fstream>
#include <iomanip>
#include <bitset>
#include <memory>
#include <filesystem>
#include <ctime>

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

#define START_TIME 2

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("NetSeerTest");

void SaveSimulationMetadata(std::string out_file, std::unordered_map<std::string, std::string> sim_metadata)
{
 if (out_file != "")
 {
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> file = asciiTraceHelper.CreateFileStream(out_file);

    for (auto &it : sim_metadata)
    {
      *(file->GetStream()) <<  it.first << "="  << it.second << "\n";
    }
  file->GetStream()->flush();
 }
}


void test1(int &a, int&b)
{
  b = a;
  a = a+5;
}

void test2(int * a, int * b)
{
  *a = *a+5;
  *b = *a;
}


struct test {
  uint32_t x = 0;
  uint32_t y = 0;
  uint32_t z = 0; 
};

void test_clear_struct_vector(){
  uint32_t N = 10;
  std::vector<test> t = std::vector<test> (10);

  /* fill */
  for (uint32_t i = 0; i < N; i++)
  {
    t[i].x = i + 1;
    t[i].y = i + 2;
    t[i].z = i + 3;
  }

  /* print */ 
  std::fill(t.begin(), t.end(), test());

  for (uint32_t i = 0; i < N; i++)
  {
    std::cout << t[i].x << " " <<  t[i].y << " "  << t[i].z << std::endl;
  }
}


int 
main (int argc, char *argv[])
{

  //std::clock_t simulation_execution_time = std::clock();

  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;

  /* General Simulation Settings */
  bool debug_flag = false;
  bool pcap_enabled = false;
  uint32_t sim_seed = 1;
  std::string out_dir_base = "test";

  cmd.AddValue("DebugFlag", "If enabled debugging messages will be printed", debug_flag);
  cmd.AddValue("PcapEnabled", "If enabled interfaces traffic will be captured", pcap_enabled);
  cmd.AddValue("Seed", "Random seed", sim_seed);
  cmd.AddValue("OutDirBase", "Root of where to put output files", out_dir_base);

  /* Basic Traffic Generation Params */

  /* type of traffic we will send, this is used to decide which app we start */
  std::string traffic_type = "trace"; //synthetic
  double send_duration = 5;
  double sim_duration = 10;

  cmd.AddValue("TrafficType", "Sender we use to send traffic: synthetic or trace based", traffic_type);
  cmd.AddValue("SendDuration", "Duration in seconds to keep the rate", send_duration);
  cmd.AddValue("SimDuration", "Simulation duration", sim_duration);

  /* Synthetic traces */
  std::string send_rate = "100Mbps";
  uint32_t num_flows = 10;
  uint32_t num_drops = 0;
  std::string main_dst = "h3";
    
  cmd.AddValue("SendRate", "Datarate to send", send_rate);
  cmd.AddValue("NumFlows", "Uniform amout of different flows", num_flows);
  cmd.AddValue("DstAddr", "Destination node", main_dst);
  cmd.AddValue("NumDrops", "Number of flows being blackholed", num_drops);

  /* Caida Traces*/ 
  std::string scaling_speed = "x1";
  uint32_t num_top_entries_traffic = 0;
  uint32_t num_top_drops = 0 ;
  uint32_t num_bottom_drops = 0;
  std::string bottom_drop_type = "TopDown";
  std::string top_drop_type = "TopDown";
  std::string allowed_to_fail_file = "";

  std::string in_dir_base = "";
  uint32_t trace_slice = 0;
  //std::string trace_top_prefixes = "";

  cmd.AddValue("Scaling", "Scaling speed of the pcap trace", scaling_speed);
  cmd.AddValue("InDirBase", "Root of where to find all the input files", in_dir_base);
  cmd.AddValue("TraceSlice", "Trace slice to simulate", trace_slice);
  cmd.AddValue("AllowedToFail", "List of prefixes that can be failed", allowed_to_fail_file);
  //cmd.AddValue("TraceBin", "File with the binary trace to inject", trace_bin);
  //cmd.AddValue("TopPrefixesFile", "file with top prefixes parsed from trace", trace_top_prefixes);

  cmd.AddValue("TopFailType", "Way of selecting prefixes in the top region", top_drop_type);
  cmd.AddValue("BottomFailType", "Way of selecting prefixes in the down region", bottom_drop_type);
  cmd.AddValue("NumTopEntriesTraffic", "Number of prefixes considered top", num_top_entries_traffic);
  cmd.AddValue("NumTopFails", "prefixes to fail from the top", num_top_drops);
  cmd.AddValue("NumBottomFails", "prefixes to fail from the bottom", num_bottom_drops);

  /* Switch Configurtation */
  uint32_t m_numBatches = 2;
  uint32_t m_numCells = 2048;
  uint32_t m_numHashes = 3;
  double m_batchTimeMs = 100;
  double m_tm_drop_rate = 0;
  double m_fail_drop_rate = 1;

  cmd.AddValue ("NumberBatches", "Number of batches we measure.", m_numBatches);
  cmd.AddValue ("NumberOfCells", "Number of cells per port and batch", m_numCells);
  cmd.AddValue ("NumberHashes", "Number of hash functions per packet", m_numHashes);
  cmd.AddValue ("BatchTime", "Batch frequency update in ms", m_batchTimeMs);

  cmd.AddValue ("TmDropRate", "Percentage of traffic that gets dropped by the traffic manager.", m_tm_drop_rate);
  cmd.AddValue ("FailDropRate", "Percentage of traffic that gets dropped when there is a failure.", m_fail_drop_rate);

  cmd.Parse (argc, argv);

  std::string full_in_dir_base = in_dir_base + std::to_string(trace_slice);

  uint32_t simulation_start = 0;
  sim_duration = send_duration + simulation_start;
  
  /* Setting Global Parameters */

  RngSeedManager::SetRun(7);   // Changes run number from default of 1 to 7
  RngSeedManager::SetSeed(sim_seed);

  //Set simulator's time resolution (click) We use Picoseconds so we can divide nanoseconds 
  Time::SetResolution(Time::PS);

  /* Set Switch Globals */
  //Config::SetDefault ("ns3::QueueBase::MaxSize", QueueSizeValue(QueueSize("1000p")));
  Config::SetDefault ("ns3::P4SwitchNetDevice::EnableDebug", BooleanValue(debug_flag));

  Config::SetDefault ("ns3::P4SwitchLossRadar::NumberBatches", UintegerValue(m_numBatches));
  Config::SetDefault ("ns3::P4SwitchLossRadar::NumberOfCells", UintegerValue(m_numCells));
  Config::SetDefault ("ns3::P4SwitchLossRadar::NumberHashes", UintegerValue(m_numHashes));
  Config::SetDefault ("ns3::P4SwitchLossRadar::BatchTime", DoubleValue(m_batchTimeMs));


  Config::SetDefault ("ns3::P4SwitchLossRadar::TmDropRate", DoubleValue(m_tm_drop_rate));
  Config::SetDefault ("ns3::P4SwitchLossRadar::FailDropRate", DoubleValue(m_fail_drop_rate));
 
  //Set globals defaults 
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1446)); //MTU 1446
  Config::SetDefault("ns3::CsmaChannel::FullDuplex", BooleanValue(true)); //same than DupAckThreshold

  /* Switches Globals */
  static GlobalValue g_myGlobal = GlobalValue ("switchId", "Global Switch Id", UintegerValue (1), MakeUintegerChecker<uint8_t> ());
  static GlobalValue g_debugGlobal = GlobalValue ("debugGlobal", "Is debug globally enabled for my modules?", BooleanValue(debug_flag), MakeBooleanChecker());

  //
  // Explicitly create the nodes required by the topology (shown above).
  //

  if (debug_flag)
  {
    LogComponentEnable("P4SwitchNetDevice", LOG_DEBUG);
    //LogComponentEnable("P4SwitchNetDevice", LOG_PREFIX_TIME);
    //LogComponentEnable("P4SwitchNetDevice", LOG_PREFIX_FUNC);
    //LogComponentEnable("P4SwitchLossRadar", LOG_WARN); 
    LogComponentEnable("P4SwitchLossRadar", LOG_DEBUG); 
  }

  /* Saves information from this simulation 
  so its easier to know what happened */

  /*

  std::unordered_map<std::string, std::string> sim_metadata;
  sim_metadata["TraceSlice"] = std::to_string(trace_slice);
  sim_metadata["Seed"] = std::to_string(sim_seed);
  std::filesystem::path relative_path = out_dir_base;
  std::filesystem::path absolute_path = std::filesystem::absolute(relative_path);
  sim_metadata["OutDirBase"] = absolute_path.string();
  sim_metadata["SendDuration"] = std::to_string(send_duration);
  relative_path = in_dir_base;
  absolute_path = std::filesystem::absolute(relative_path);
  sim_metadata["InDirBase"] = absolute_path;
  sim_metadata["TopFailType"] = top_drop_type;
  sim_metadata["BottomFailType"] = bottom_drop_type;
  sim_metadata["NumTopEntriesTraffic"] = std::to_string(num_top_entries_traffic);
  sim_metadata["NumTopFails"] = std::to_string(num_top_drops);
  sim_metadata["NumBottomFails"] = std::to_string(num_bottom_drops);
  sim_metadata["NumTopEntriesSystem"] = std::to_string(num_top_entries_system);
  sim_metadata["Pipeline"] =  std::to_string(m_pipeline);
  sim_metadata["PipelineBoost"] = std::to_string(m_pipelineBoost);
  sim_metadata["TreeDepth"] = std::to_string(m_treeDepth);
  sim_metadata["LayerSplit"] = std::to_string(m_layerSplit);
  sim_metadata["CounterWidth"] = std::to_string(m_counterWidth);
  sim_metadata["CostType"] = std::to_string(m_costType);
  sim_metadata["ProbingTimeZoomingMs"] = std::to_string(m_probing_time_zooming_ms);
  sim_metadata["ProbingTimeTopEntriesMs"] = std::to_string(m_probing_time_top_entries_ms);
  sim_metadata["TreeEnabled"] = std::to_string(m_treeEnabled);

  // to differenciate between types of input traffic
  sim_metadata["TrafficType"] = traffic_type;
  sim_metadata["SendRate"] = send_rate;
  sim_metadata["NumFlows"] = std::to_string(num_flows);
  sim_metadata["NumDrops"] = std::to_string(num_drops);
  */
  /* Save current time */
  /*
  time_t seconds_past_epoch = time(0);
  sim_metadata["ExperimentEpoch"] = std::to_string(seconds_past_epoch);

  sim_metadata["FailDropRate"] = std::to_string(m_fail_drop_rate);
  sim_metadata["UniformLossThreshold"] = std::to_string(m_uniformLossThreshold);
  sim_metadata["AllowedToFail"] = allowed_to_fail_file;
  */
  

  //sim_metadata[""] = ;
  //SaveSimulationMetadata(out_dir_base + ".info", sim_metadata);

  /* Basic 2 switch topology */

  NS_LOG_INFO ("Create nodes.");
  NodeContainer hosts;
  hosts.Create (6);

  NodeContainer switches;
  switches.Create (3);

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

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;

  DataRate bw("100Gbps");
  csma.SetChannelAttribute ("DataRate", DataRateValue(bw));
  csma.SetChannelAttribute ("Delay", TimeValue (MicroSeconds (50)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  //std::stringstream queue_size_str;
  //queue_size_str << queue_size << "p";
  csma.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue("1000p"));

  // Create the csma links, from each terminal to the switch
  NetDeviceContainer switch0Devices;
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

  // Switch 0
  switch0Devices.Add(link0.Get(1));
  switch0Devices.Add(link1.Get(1));
  switch0Devices.Add(link2.Get(1));
  switch0Devices.Add(link3.Get(0));

  // Switch 1
  switch1Devices.Add(link3.Get(1));
  switch1Devices.Add(link4.Get(0));
  switch1Devices.Add(link5.Get(0));
  switch1Devices.Add(link6.Get(0));

  hostDevices.Add(link0.Get(0));
  hostDevices.Add(link1.Get(0));
  hostDevices.Add(link2.Get(0));
  hostDevices.Add(link4.Get(1));
  hostDevices.Add(link5.Get(1));
  hostDevices.Add(link6.Get(1));

  // Create the switch netdevice, which will do the packet switching
  std::string switchType = "ns3::P4SwitchLossRadar";
  P4SwitchHelper switch_h(switchType);
  NetDeviceContainer devs = switch_h.Install<P4SwitchLossRadar> (s1, switch0Devices);

  devs.Get(0)->SetAttribute("OutFile", StringValue(out_dir_base + "_s1.json"));

  //if (debug_flag)
  //{
  //  DynamicCast<P4SwitchLossRadar> (devs.Get(0))->SetDebug(true);
  //}
  devs = switch_h.Install<P4SwitchLossRadar> (s2, switch1Devices);
  //devs.Get(0)->SetAttribute("OutFile", StringValue(out_dir_base + "_s2.json"));
  DynamicCast<P4SwitchLossRadar> (devs.Get(0))->SetDebug(false);
  devs.Get(0)->SetAttribute("OutFile", StringValue(""));


  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.Install (hosts);

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  //
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (hostDevices);

  //
  // Create an OnOff application to send UDP datagrams from node zero to node 1.
  //
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 6001;

  BulkSendHelper sender ("ns3::TcpSocketFactory", 
                     Address (InetSocketAddress (GetNodeIp(h3), port)));
  sender.SetAttribute("SendSize", UintegerValue(1400));
  sender.SetAttribute("MaxBytes", UintegerValue(100000));

  ApplicationContainer app = sender.Install (h0);
  //Start the application
  app.Start (Seconds (0));
  app.Stop (Seconds (sim_duration));

  ApplicationContainer app2 = sender.Install (h0);
  //Start the application
  app2.Start (Seconds (0));
  app2.Stop (Seconds (sim_duration));

  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  ApplicationContainer app1;
  app1 = sink.Install (h3);
  app1.Start (Seconds (0.0));
  app1 = sink.Install (h4);
  app1.Start (Seconds (0.0));
  app1 = sink.Install (h5);
  app1.Start (Seconds (0.0));

  NS_LOG_INFO ("Configure Tracing.");

  if (pcap_enabled)
  { 
    csma.EnablePcap("output/loss-radar-2sw", link0.Get(0), true); //h0 -> s1
    csma.EnablePcap("output/loss-radar-2sw", link3.Get(0), true); //s1 -> s2
  }
  
  // Set Link failure 
  //Simulator::Schedule(Seconds(2.3305), &FailLink, link3);
  //Simulator::Schedule(Seconds(2.9305), &RecoverLink, link3);
  //SetUniformDropRate(link3, 0.01);

 if (traffic_type == "trace")
 {
    Ptr<TraceSendApplication> trace_send_app = CreateObject<TraceSendApplication>();
    h0->AddApplication(trace_send_app);
    trace_send_app->SetAttribute("Duration", DoubleValue(send_duration));
    trace_send_app->SetAttribute("Scaling", StringValue(scaling_speed));
    trace_send_app->SetAttribute("DstAddr", AddressValue(GetNodeNetDevice(main_dst)->GetAddress()));
    trace_send_app->SetAttribute("TopFailType", StringValue(top_drop_type));
    trace_send_app->SetAttribute("BottomFailType", StringValue(bottom_drop_type));
    trace_send_app->SetAttribute("NumTopEntries", UintegerValue(num_top_entries_traffic));
    trace_send_app->SetAttribute("NumTopFails", UintegerValue(num_top_drops));
    trace_send_app->SetAttribute("NumBottomFails", UintegerValue(num_bottom_drops));
    trace_send_app->SetAttribute("InFile", StringValue(in_dir_base + "_" + std::to_string(trace_slice) + ".bin"));
    trace_send_app->SetAttribute("TopFile", StringValue(in_dir_base + "_" + std::to_string(trace_slice) + ".top"));
    trace_send_app->SetAttribute("AllowedToFail", StringValue(allowed_to_fail_file));
    trace_send_app->SetAttribute("OutFile", StringValue(out_dir_base + "-failed_prefixes.txt"));
    trace_send_app->SetStartTime(Seconds(simulation_start));
 }

 else if (traffic_type == "synthetic")
 {
    Ptr<RawSendApplication> raw_send_app = CreateObject<RawSendApplication>();
    h0->AddApplication(raw_send_app);
    raw_send_app->SetAttribute("SendRate", DataRateValue(DataRate(send_rate)));
    raw_send_app->SetAttribute("Duration", DoubleValue(send_duration));
    raw_send_app->SetAttribute("NumFlows", UintegerValue(num_flows));
    raw_send_app->SetAttribute("DstAddr", AddressValue(GetNodeNetDevice(main_dst)->GetAddress()));
    raw_send_app->SetAttribute("NumDrops", UintegerValue(num_drops));
    raw_send_app->SetAttribute("OutFile", StringValue(out_dir_base + "-synthetic-sent_flows.txt"));
    raw_send_app->SetStartTime(Seconds(simulation_start));
    //raw_send_app->SetStopTime(Seconds(20));
 }
   
  //Ptr<NetDevice> srcPort = GetNodeNetDevice("h0");
  //Ptr<NetDevice> dstPort = GetNodeNetDevice("h3");
  //raw_send(srcPort, dstPort);
  PrintNow(0.01);

  NS_LOG_INFO ("Run Simulation.");

  Simulator::Stop(Seconds(sim_duration));
  Simulator::Run ();

  Simulator::Stop(Seconds(0));

  // Save total simulation time
  //float real_simulation_time = (float(clock() - simulation_execution_time) / CLOCKS_PER_SEC);
  //sim_metadata["RealSimulationTime"] = std::to_string(real_simulation_time);
  //SaveSimulationMetadata(out_dir_base + ".info", sim_metadata);

  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
