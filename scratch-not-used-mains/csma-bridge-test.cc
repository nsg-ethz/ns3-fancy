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
//        n0     n1
//        |      | 
//       ----------
//       | Switch |
//       ----------
//        |      | 
//        n2     n3
//
//
// - CBR/UDP flows from n0 to n1 and from n3 to n0
// - DropTail queues 
// - Tracing of queues and packet receptions to file "csma-bridge.tr"

#include <iostream>
#include <fstream>

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/bridge-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("CsmaBridgeExample");

int 
main (int argc, char *argv[])
{
  //
  // Users may find it convenient to turn on explicit debugging
  // for selected modules; the below lines suggest how to do this
  //
#if 0 
  LogComponentEnable ("CsmaBridgeExample", LOG_LEVEL_INFO);
#endif

  //
  // Allow the user to override any of the defaults and the above Bind() at
  // run-time, via command-line arguments
  //
  CommandLine cmd;
  cmd.Parse (argc, argv);

  Config::SetDefault("ns3::CsmaChannel::FullDuplex", BooleanValue(true)); //same than DupAckThreshold
  //Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(917440));
  //Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(917440));

  //
  // Explicitly create the nodes required by the topology (shown above).
  //
  NS_LOG_INFO ("Create nodes.");
  NodeContainer terminals;
  terminals.Create (2);

  NodeContainer csmaSwitch;
  csmaSwitch.Create (2);

  NS_LOG_INFO ("Build Topology");
  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", DataRateValue (DataRate("100Mbps")));
  csma.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));

  // Create the csma links, from each terminal to the switch

  NetDeviceContainer terminalDevices;
  NetDeviceContainer switchDevices0;
  NetDeviceContainer switchDevices1;

  NetDeviceContainer link = csma.Install (NodeContainer (terminals.Get (0), csmaSwitch.Get(0)));
  terminalDevices.Add (link.Get (0));
  switchDevices0.Add (link.Get (1));

  NetDeviceContainer link1 = csma.Install (NodeContainer (csmaSwitch.Get(0),csmaSwitch.Get(1)));
  switchDevices0.Add (link1.Get (0));
  switchDevices1.Add (link1.Get (1));

  NetDeviceContainer link2 = csma.Install (NodeContainer (csmaSwitch.Get(1), terminals.Get(1)));
  terminalDevices.Add (link2.Get (1));
  switchDevices1.Add (link2.Get (0));
  

  // Create the bridge netdevice, which will do the packet switching
  Ptr<Node> switchNode0 = csmaSwitch.Get (0);
  Ptr<Node> switchNode1 = csmaSwitch.Get (1);

  BridgeHelper bridge;
  bridge.Install (switchNode0, switchDevices0);
  bridge.Install (switchNode1, switchDevices1);

  // Add internet stack to the terminals
  InternetStackHelper internet;
  internet.Install (terminals);

  // We've got the "hardware" in place.  Now we need to add IP addresses.
  //
  NS_LOG_INFO ("Assign IP Addresses.");
  Ipv4AddressHelper ipv4;
  ipv4.SetBase ("10.1.1.0", "255.255.255.0");
  ipv4.Assign (terminalDevices);

  //
  // Create an OnOff application to send UDP datagrams from node zero to node 1.
  //
  NS_LOG_INFO ("Create Applications.");
  uint16_t port = 9;   // Discard port (RFC 863)

  BulkSendHelper sender ("ns3::TcpSocketFactory", 
                     Address (InetSocketAddress (Ipv4Address ("10.1.1.2"), port)));
  sender.SetAttribute("SendSize", UintegerValue(1024));
  sender.SetAttribute("MaxBytes", UintegerValue(100*1000000));

  ApplicationContainer app = sender.Install(terminals.Get(0));

  // Start the application
  app.Start (Seconds (1.0));
  app.Stop (Seconds (6));

  // Create an optional packet sink to receive these packets
  PacketSinkHelper sink ("ns3::TcpSocketFactory",
                         Address (InetSocketAddress (Ipv4Address::GetAny (), port)));
  app = sink.Install (terminals.Get (1));
  app.Start (Seconds (0.0));

  NS_LOG_INFO ("Configure Tracing.");

  //
  // Configure tracing of all enqueue, dequeue, and NetDevice receive events.
  // Trace output will be sent to the file "csma-bridge.tr"
  //
  AsciiTraceHelper ascii;
  csma.EnableAsciiAll (ascii.CreateFileStream ("csma-bridge.tr"));

  //
  // Also configure some tcpdump traces; each interface will be traced.
  // The output files will be named:
  //     csma-bridge-<nodeId>-<interfaceId>.pcap
  // and can be read by the "tcpdump -r" command (use "-tt" option to
  // display timestamps correctly)
  //
  csma.EnablePcapAll ("output/csma-bridge", false);

  //
  // Now, do the actual simulation.
  //
  NS_LOG_INFO ("Run Simulation.");
  Simulator::Run ();
  Simulator::Destroy ();
  NS_LOG_INFO ("Done.");
}
