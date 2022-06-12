/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2017 Luciano Jerez Chaves
 *
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
 * Author: Luciano Jerez Chaves <luciano@lrc.ic.unicamp.br>
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"

using namespace ns3;

int
main (int argc, char *argv[])
{
  bool verbose = false;
  bool trace = false;

  CommandLine cmd;
  cmd.AddValue ("verbose",  "Tell application to log if true", verbose);
  cmd.AddValue ("trace",    "Tracing traffic to files", trace);
  cmd.AddValue ("duplex",   "ns3::CsmaChannel::FullDuplex");
  cmd.Parse (argc, argv);

  if (verbose)
    {
      LogComponentEnable ("CsmaNetDevice", LOG_LEVEL_ALL);
      LogComponentEnable ("CsmaChannel", LOG_LEVEL_ALL);
      LogComponentEnable ("Backoff", LOG_LEVEL_ALL);
    }

  // Create the host nodes
  NodeContainer hosts;
  hosts.Create (2);

  // Connecting the hosts
  CsmaHelper csmaHelper;
  csmaHelper.SetChannelAttribute ("DataRate", DataRateValue (DataRate ("100Mbps")));
  csmaHelper.SetChannelAttribute ("Delay", TimeValue (MilliSeconds (1)));
  NetDeviceContainer hostDevices = csmaHelper.Install (hosts);

  // Enable pcap traces
  if (trace)
    {
      csmaHelper.EnablePcap ("csma-duplex", hostDevices);
    }

  // Installing the tcp/ip stack into hosts
  InternetStackHelper internet;
  internet.Install (hosts);

  // Set IPv4 host address
  Ipv4AddressHelper ipv4switches;
  Ipv4InterfaceContainer internetIpIfaces;
  ipv4switches.SetBase ("10.1.1.0", "255.255.255.0");
  internetIpIfaces = ipv4switches.Assign (hostDevices);
  Ipv4Address h0Addr = internetIpIfaces.GetAddress (0);
  Ipv4Address h1Addr = internetIpIfaces.GetAddress (1);

  ApplicationContainer senderApps, sinkApps;

  // TCP traffic from host 0 to 1
  BulkSendHelper sender0 ("ns3::TcpSocketFactory", InetSocketAddress (h1Addr, 10000));
  PacketSinkHelper sink1 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 10000));
  senderApps.Add (sender0.Install (hosts.Get (0)));
  sinkApps.Add (sink1.Install (hosts.Get (1)));

  // TCP traffic from host 1 to 0
  BulkSendHelper sender1 ("ns3::TcpSocketFactory", InetSocketAddress (h0Addr, 10001));
  PacketSinkHelper sink0 ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 10001));
  senderApps.Add (sender1.Install (hosts.Get (1)));
  sinkApps.Add (sink0.Install (hosts.Get (0)));

  sinkApps.Start (Seconds (0));
  senderApps.Start (Seconds (0.01));
  senderApps.Stop (Seconds (5));

  // Run the simulation
  Simulator::Run ();
  Simulator::Destroy ();

  // Transmitted bytes
  Ptr<PacketSink> pktSink0 = DynamicCast<PacketSink> (sinkApps.Get (1));
  Ptr<PacketSink> pktSink1 = DynamicCast<PacketSink> (sinkApps.Get (0));
  std::cout << "Total bytes sent from H0 to H1: " << pktSink1->GetTotalRx () << std::endl;
  std::cout << "Total bytes sent from H1 to H0: " << pktSink0->GetTotalRx () << std::endl;
}