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
#include "ns3/flow-monitor-module.h"
#include "ns3/utils-module.h"
#include "ns3/traffic-generation-module.h"

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

std::string fileNameRoot = "congestion-test";    // base name for trace files, etc

NS_LOG_COMPONENT_DEFINE (fileNameRoot);

const char *file_name = g_log.Name();
std::string script_name = file_name;


void ChangeLinkBandwidth(NetDeviceContainer link, DataRate rate){

  link.Get(0)->SetAttribute("DataRate", DataRateValue(rate));
  link.Get(1)->SetAttribute("DataRate", DataRateValue(rate));
  //Simulator::Schedule(Seconds(delay), &ChangeLinkBandwidth, delay, link, next_rate, previous_rate);
  
}

int
main(int argc, char *argv[]) {

    //INITIALIZATION

    //Set simulator's time resolution (click)
    Time::SetResolution(Time::NS);

    //Fat tree parameters
    DataRate sendersBandwidth("10Mbps");
    DataRate receiversBandwidth("10Mbps");
    DataRate networkBandwidth("10Mbps");

    //Command line arguments
    std::string simulationName = "test";
    std::string outputDir = "outputs/";
    std::string pcap_name = "tx";

    std::string inputDir = "swift_datasets/test/";

    uint16_t queue_size = 1000;
    uint64_t runStep = 1;

    uint64_t network_delay = 10; //ns?
    uint32_t flowsPersec = 100;
    double duration = 5;
    bool save_pcap = false;

    CommandLine cmd;
    //General
    //Links properties
    cmd.AddValue("LinkBandwidth", "Bandwidth of link, used in multiple experiments", networkBandwidth);
    cmd.AddValue("NetworkDelay", "Added delay between nodes", network_delay);

    //Experiment
    cmd.AddValue("QueueSize", "Interfaces Queue length", queue_size);
    cmd.AddValue("RunStep", "Random generator starts at", runStep);
    cmd.AddValue("EnablePcap", "Save traffic in a pcap file", save_pcap);
    cmd.AddValue("PcapName", "Save traffic in a pcap file", pcap_name);

    cmd.Parse(argc, argv);

    //Change that if i want to get different random values each run otherwise i will always get the same.
    RngSeedManager::SetRun(runStep);   // Changes run number from default of 1 to 7

    //Update root name
    std::ostringstream run;
    run << runStep;

    //Timeout calculations (in milliseconds)
    std::string outputNameRoot = outputDir;

    outputNameRoot = outputNameRoot + "/";

    //TCP
    uint16_t rtt = 200;
    uint16_t initial_rto = 2000;

    //GLOBAL CONFIGURATION
    //Routing

    Config::SetDefault("ns3::Ipv4GlobalRouting::RespondToInterfaceEvents", BooleanValue(true));

    //Tcp Socket (general socket conf)
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(4000000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(4000000));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1446)); //MTU 1446
    Config::SetDefault("ns3::TcpSocket::InitialSlowStartThreshold", UintegerValue(4294967295));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(1));

    //Can be much slower than my rtt because packet size of syn is 60bytes
    Config::SetDefault("ns3::TcpSocket::ConnTimeout",
                       TimeValue(MilliSeconds(initial_rto))); // connection retransmission timeout
    Config::SetDefault("ns3::TcpSocket::ConnCount", UintegerValue(5)); //retrnamissions during connection
    Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(15)); //retranmissions
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
    p2p.SetChannelAttribute("Delay", TimeValue(MilliSeconds(network_delay)));
    p2p.SetDeviceAttribute("Mtu", UintegerValue(1500));

     //New way of setting the queue length and mode
    std::stringstream queue_size_str;
    queue_size_str << queue_size << "p";
    p2p.SetQueue("ns3::DropTailQueue", "MaxSize", StringValue(queue_size_str.str()));

    //SIMULATION STARTS


    ////////////////////////////////////////////////////////////////////////////////////////////////////
    //Build Topology
    ////////////////////////////////////////////////////////////////////////////////////////////////////

    //Number of senders and receivers
    int num_senders = 4;
    int num_receivers = 1;
    //Senders
    NodeContainer senders;
    senders.Create(num_senders);

    NodeContainer receivers;
    receivers.Create(num_receivers);

    //naming

    uint32_t i = 0;
    for (NodeContainer::Iterator host = senders.Begin(); host != senders.End(); host++, i++) {

        //Assign host name, and add it to the names system
        std::stringstream host_name;
        host_name << "s" << i;
        Names::Add(host_name.str(), (*host));

    }
    i = 0;
    for (NodeContainer::Iterator host = receivers.Begin(); host != receivers.End(); host++, i++) {

        //Assign host name, and add it to the names system
        std::stringstream host_name;
        host_name << "d" << i;
        Names::Add(host_name.str(), (*host));
    }

    //Single link Switches
    Ptr<Node> sw1 = CreateObject<Node>();
    Ptr<Node> sw2 = CreateObject<Node>();

    //Add two switcheIIs
    Names::Add("sw1", sw1);
    Names::Add("sw2", sw2);

    // Install Internet stack : aggregate ipv4 udp and tcp implementations to nodes
    InternetStackHelper stack;
    stack.Install(senders);
    stack.Install(receivers);
    stack.Install(sw1);
    stack.Install(sw2);

    //Install net devices between nodes (so add links) would be good to save them somewhere I could maybe use the namesystem or map
    //Install internet stack to nodes, very easy
    std::unordered_map<std::string, NetDeviceContainer> links;

    //SETTING LINKS: delay and bandwidth.

    //sw1 -> sw2
    //Interconnect middle switches : sw1 -> sw2
    links[GetNodeName(sw1) + "->" + GetNodeName(sw2)] = p2p.Install(NodeContainer(sw1, sw2));
    //Set delay and bandwdith
    links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0)->GetChannel()->SetAttribute("Delay", TimeValue(
            MilliSeconds(network_delay)));

    links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0)->SetAttribute("DataRate", DataRateValue(networkBandwidth));

    NS_LOG_DEBUG("Link Add: " << GetNodeName(sw1)
                              << " -> "
                              << GetNodeName(sw2)
                              << " delay(ns):"
                              << network_delay
                              << " bandiwdth"
                              << networkBandwidth);


    //Assing senders, sw1, sw2 ips since they do not depend on the prefixes
    Ipv4AddressHelper address("10.0.1.0", "255.255.255.0");
    address.Assign(links[GetNodeName(sw1) + "->" + GetNodeName(sw2)]);
    address.NewNetwork();

    //Senders
    for (NodeContainer::Iterator host = senders.Begin(); host != senders.End(); host++) {

        //add link host-> sw1
        links[GetNodeName(*host) + "->" + GetNodeName(sw1)] = p2p.Install(NodeContainer(*host, sw1));
        links[GetNodeName(*host) + "->" + GetNodeName(sw1)].Get(0)->SetAttribute("DataRate", DataRateValue(sendersBandwidth));

        //Assign IP
        address.Assign(links[GetNodeName(*host) + "->" + GetNodeName(sw1)]);
        address.NewNetwork();

    }
    //Receivers
    for (NodeContainer::Iterator host = receivers.Begin(); host != receivers.End(); host++) {

        links[GetNodeName(sw2) + "->" + GetNodeName(*host)] = p2p.Install(NodeContainer(sw2, GetNodeName(*host)));
        links[GetNodeName(sw2) + "->" + GetNodeName(*host)].Get(0)->SetAttribute("DataRate", DataRateValue(receiversBandwidth));
        address.Assign(links[GetNodeName(sw2) + "->" + GetNodeName(*host)]);
        address.NewNetwork();

    }

    //Assign IPS
    //Uninstall FIFO queue //  //uninstall qdiscs
    TrafficControlHelper tch;
    for (auto it : links) {
        tch.Uninstall(it.second);
    }

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
    //START TRAFFIC
    //Install Traffic sinks at receivers

    NS_LOG_DEBUG("Starting Sinks");
    std::unordered_map<std::string, std::vector<uint16_t>> hostsToPorts = InstallSinks(receivers, 10, 0, "TCP");
    NS_LOG_DEBUG("Starting Traffic Scheduling");

    Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable>();

    for (NodeContainer::Iterator host = senders.Begin(); host != senders.End(); host++){

        for (int l =0 ; l < 2; l++) {

            Ptr<Node> dst = receivers.Get(random_variable->GetInteger(0, receivers.GetN() - 1));
            std::vector<uint16_t> availablePorts = hostsToPorts[GetNodeName(dst)];
            uint16_t dport = RandomFromVector<uint16_t>(availablePorts, rand);
            InstallNormalBulkSend(*host, dst, dport, 80000000, random_variable->GetInteger(1, 3));
            break;
        }
        break;
    }

    //set repetitive event.
    NetDeviceContainer link = links[GetNodeName(sw1) + "->" + GetNodeName(sw2)];
    //ChangeLinkBandwidth(0.5, link, networkBandwidth, DataRate("1Mbps"));

//    for (int i = 0; i <100; i++){
//        Simulator::Schedule(Seconds(i), &ChangeLinkDropRate , link,  0.8);
//        Simulator::Schedule(Seconds(i+0.5), &ChangeLinkDropRate, link,  0);
//    }

//    for (int i = 0; i <200; i++){
//        Simulator::Schedule(Seconds(i), &ChangeLinkBandwidth , link,  DataRate("10Mbps"));
//        Simulator::Schedule(Seconds(i+0.5), &ChangeLinkBandwidth, link,  DataRate("1Mbps"));
//    }

    //Simulator::Schedule(Seconds(10), &ChangeLinkBandwidth , link,  DataRate("1Mbps"));
    //Simulator::Schedule(Seconds(20), &ChangeLinkBandwidth, link,  DataRate("10Mbps"));

    //////////////////
    //TRACES
    ///////////////////

//  //Only save TX traffic
    if (save_pcap) {

        //p2p.EnablePcap(outputNameRoot + "tx_rx.pcap", links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0), bool(1));

        PcapHelper pcapHelper;
        Ptr<PcapFileWrapper> pcap_file = pcapHelper.CreateFile(
                outputNameRoot + pcap_name + ".pcap", std::ios::out,
                PcapHelper::DLT_PPP);
        links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(0)->TraceConnectWithoutContext("PhyTxBegin",
                                                                                             MakeBoundCallback(&TracePcap,
                                                                                                               pcap_file));
        links[GetNodeName(sw1) + "->" + GetNodeName(sw2)].Get(1)->TraceConnectWithoutContext("PhyTxBegin",
                                                                                             MakeBoundCallback(&TracePcap,
                                                                                                               pcap_file));
    }



    //traces
//    link2.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&TracePcap, pcap_file));
//    link2.Get (1)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&TracePcap, pcap_file));
//    link2.Get (1)->TraceConnectWithoutContext ("MacTxDrop", MakeBoundCallback (&TracePcap, pcap_file));


    //Simulation Starts
    Simulator::Stop(Seconds(10000));
    Simulator::Run();
    Simulator::Destroy();

    return 0;
}
