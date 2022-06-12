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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/p4-switch-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/utils.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SwitchTest");

int 
main (int argc, char *argv[])
{

  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;
  cmd.Parse (argc, argv);

  //Set simulator's time resolution (click)
  Time::SetResolution(Time::NS);

  Config::SetDefault("ns3::CsmaChannel::FullDuplex", BooleanValue(true)); //same than DupAckThreshold


  //Set globals
  //Tcp Socket (general socket conf)
  //Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4000000));
  //Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4000000));
  Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1446)); //MTU 1446
//Config::SetDefault("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(4294967295));
//Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));
//
////Can be much slower than my rtt because packet size of syn is 60bytes
//Config::SetDefault("ns3::TcpSocket::ConnTimeout",
//                    TimeValue(MilliSeconds(200))); // connection retransmission timeout
//Config::SetDefault("ns3::TcpSocket::ConnCount", UintegerValue(5)); //retrnamissions during connection
//Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(15)); //retranmissions
////Config::SetDefault("ns3::TcpSocket::DelAckTimeout", TimeValue(MilliSeconds(rtt / 50)));
// 	Config::SetDefault ("ns3::TcpSocket::DelAckCount", UintegerValue(2));
//Config::SetDefault("ns3::TcpSocket::TcpNoDelay", BooleanValue(true)); //disable nagle's algorithm
//Config::SetDefault("ns3::TcpSocket::PersistTimeout",
//                    TimeValue(NanoSeconds(6000000000))); //persist timeout to porbe for rx window
//
////Tcp Socket Base: provides connection orientation, sliding window, flow control; congestion control is delegated to the subclasses (i.e new reno)
//
//Config::SetDefault("ns3::TcpSocketBase::MaxSegLifetime", DoubleValue(10));
//Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(10))); //min RTO value that can be set
//Config::SetDefault("ns3::TcpSocketBase::ClockGranularity", TimeValue(MicroSeconds(1)));
//Config::SetDefault("ns3::TcpSocketBase::ReTxThreshold", UintegerValue(3)); //same than DupAckThreshold
//
//Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true)); //enable sack
//Config::SetDefault("ns3::TcpSocketBase::LimitedTransmit", BooleanValue(true)); //enable sack
//
////TCP L4
////TcpNewReno
//Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));
//Config::SetDefault("ns3::RttEstimator::InitialEstimation", TimeValue(MicroSeconds(50)));//defautlt 1sec

  static GlobalValue g_myGlobal = GlobalValue ("switchId", "Global Switch Id", UintegerValue (0), MakeUintegerChecker<uint8_t> ());

  //
  // Explicitly create the nodes required by the topology (shown above).
  //

  LogComponentEnable("P4SwitchNetDevice", LOG_DEBUG);

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

  Ptr<Node> s0 = switches.Get(0);
  Ptr<Node> s1 = switches.Get(1);

  // Name all the nodes
  Names::Add("h0", h0);
  Names::Add("h1", h1);
  Names::Add("h2", h2);
  Names::Add("h3", h3);
  Names::Add("h4", h4);
  Names::Add("h5", h5);

  Names::Add("s0", s0);
  Names::Add("s1", s1);

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;

  DataRate bw("100Mbps");
  csma.SetChannelAttribute ("DataRate", DataRateValue(bw));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
  csma.SetDeviceAttribute("Mtu", UintegerValue(1500));

  // Create the csma links, from each terminal to the switch
  NetDeviceContainer switch0Devices;
  NetDeviceContainer switch1Devices;
  NetDeviceContainer switch2Devices;
  NetDeviceContainer hostDevices;

  NetDeviceContainer link0 = csma.Install(NodeContainer(h0, s0));
  NetDeviceContainer link1 = csma.Install(NodeContainer(h1, s0));
  NetDeviceContainer link2 = csma.Install(NodeContainer(h2, s0));

  NetDeviceContainer link3 = csma.Install(NodeContainer(s0, s1));

  NetDeviceContainer link4 = csma.Install(NodeContainer(s1, h3));
  NetDeviceContainer link5 = csma.Install(NodeContainer(s1, h4));
  NetDeviceContainer link6 = csma.Install(NodeContainer(s1, h5));

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
  std::string switchType = "ns3::P4SwitchNetDevice";
  P4SwitchHelper switch_h(switchType);
  switch_h.Install<P4SwitchNetDevice> (s0, switch0Devices);
  switch_h.Install<P4SwitchNetDevice> (s1, switch1Devices);

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
                     Address (InetSocketAddress (Ipv4Address ("10.1.1.4"), port)));
  sender.SetAttribute("SendSize", UintegerValue(1024));
  sender.SetAttribute("MaxBytes", UintegerValue(100*1000000));

  ApplicationContainer app = sender.Install (h0);
  // Start the application
  app.Start (Seconds (1.0));
  app.Stop (Seconds (20));

  sender.SetAttribute("Remote", AddressValue (Address (InetSocketAddress (Ipv4Address ("10.1.1.5"), port))));
  app = sender.Install (h1);
  // Start the application
  app.Start (Seconds (1.0));
  app.Stop (Seconds (20));

  sender.SetAttribute("Remote", AddressValue (Address (InetSocketAddress (Ipv4Address ("10.1.1.6"), port))));
  app = sender.Install (h2);
  // Start the application
  app.Start (Seconds (1.0));
  app.Stop (Seconds (20));

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

  csma.EnablePcapAll ("output/switch-test-2sw", true);

  // Set Link failure 
  //Simulator::Schedule(Seconds(8.3305), &FailLink, link3);
  //Simulator::Schedule(Seconds(8.9305), &RecoverLink, link3);
  //SetUniformDropRate(link3, 0.0001);
  PrintNow(1);
  
  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Stop(Seconds(60));
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
