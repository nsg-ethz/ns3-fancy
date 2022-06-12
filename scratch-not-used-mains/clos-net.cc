//
/*

                                                        +----------------------------------------------------------+
                                                        |          +-----------------------------------------|  E4 +--+
                                                        |          |        |-----------------------------------+--+  |
                                                     +--+--+    +--+--+  +-----+   +-----+                      |     |
                                                  +--+ S5  |    |  S6 |  | S7  |   | S8  +----------------------+     |
                                                  |  +-----+    +--+--+  +--+--+   +--+--+                            |
                                                  |                |        |         |                               |
  +----------------------------------+            |                |        |         |                               |
  |        +------------------|  E3  +------------------------------------------+     |                               |
  |        |         +-----------+---+            |                |        |   |     |                               |
  |        |         |           |                |                |        |   |     |                               |
  |        |         |           |                |                |        |   |     |                               |
  |        |         |           |                |                |        |   |     |                               |
  |        |         |           |                |                |        |   |     |                               |
  |        |         |           |                |                |        |   |     |                               |
+-+--+   +-+---+  +--+--+  +-----+                |                |        |   |     |                               |
| S1 |   | S2  |  |  S3 |  | S4  |                |                |        |   |     |                               |
+-+--+   ++----+  +--+--+  +--+--+                |                |        |   |     |                               |
  |       |          |        |                   |                |        |   |     |                               |
  |       |          |        |                   |                |        |   |     |                               |
  |       |          |        |                   |                |        |   |     |                               |
  |       |          |        |                   |                |        |   |     |                               |
  |       |          |        |                   |                |        |   |     |                               |
  |    +--+--+-------+        |                  ++----------------+        |   |     |                               |
  +----+ E1  |                |                  |  E2  |-------------------+   |     |                               |
       +-+---+----------------+                  +---+--------------------------------+                               |
         |                                           |                          |                                     |
         |                                           |                          |                                     |
         |                                           |                          |                                     |
         |                                           |                          |                                     |
         |                                           |                      +---+---+                                 |
         +---+-------+                               |                      |  P2   |                                 |
             |  P1   +-------------------------------+                      |       +---------------------------------+
             |       |                                                      +-------+
             +-------+							         |
                 |                                          			 |
	         |                                          			 |
        	 |                                          			 |      
        	 |                                          			 |      
		+-------+                                                     +-------+ 
            	|  H1   +                                                     |  H2   +
		|       |                                                     |       |
             	+-------+                                                     +-------+ 




*/
// 
//
#include <iostream>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-static-routing-helper.h"
#include "ns3/ipv4-list-routing-helper.h"
//	For installing bridge functionality on Node
#include "ns3/log.h"
#include "ns3/bridge-module.h"
//	Anim Module prequisite 
#include "ns3/netanim-module.h"
#include "ns3/animation-interface.h"
//	Used for Pinger application
#include "ns3/v4ping-helper.h"


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("clos_nw");

bool verbose = true;
bool use_drop = false;
bool showPings = true;
ns3::Time timeout = ns3::Seconds (0);

bool SetVerbose (std::string value)
{
	verbose = true;
	return true;
}
//
//	Printing RTT, for pings
//
static void PingRtt (std::string context, Time rtt)
{
	std::cout << context << " " << rtt << std::endl;
}

bool SetDrop (std::string value)
{
	use_drop = true;
	return true;
}

bool SetTimeout (std::string value)
{
	try {
		timeout = ns3::Seconds (atof (value.c_str ()));
		return true;
	}
	catch (...) { return false; }
	return false;
}

int main (int argc, char *argv[])
{
	LogComponentEnableAll (LogLevel (LOG_PREFIX_TIME | LOG_PREFIX_NODE));
	LogComponentEnable ("Rip", LOG_LEVEL_ALL);
	LogComponentEnable ("Ipv4Interface", LOG_LEVEL_ALL);
	LogComponentEnable ("Icmpv4L4Protocol", LOG_LEVEL_ALL);
	LogComponentEnable ("V4Ping", LOG_LEVEL_ALL);
	//
	// Create nodes for CLOS network
	//
	
	NS_LOG_INFO ("Create nodes.");
	NodeContainer terminals_A,terminals_B;
	terminals_A.Create (1);
	terminals_B.Create (1);
	//
	// Creating switches
	// 
	NodeContainer spinePlane1Switch, spinePlane2Switch, edgePlane1Switch, edgePlane2Switch, podSwitch1, podSwitch2;
	spinePlane1Switch.Create (4);
	edgePlane1Switch.Create (1);
	edgePlane2Switch.Create (1);
	podSwitch1.Create(1);
	podSwitch2.Create(1);

	NS_LOG_INFO ("Build Topology");
	CsmaHelper csma;
	
	csma.SetChannelAttribute ("DataRate", StringValue("100Mbps"));
	csma.SetChannelAttribute ("Delay",   StringValue("20ms"));
        csma.SetQueue ("ns3::DropTailQueue", "Mode", StringValue ("QUEUE_MODE_PACKETS"), "MaxPackets", UintegerValue (1024));
	//
	//	Creating network devices and switches for CSMA links
	//
	NetDeviceContainer terminalDevices_A;
	NetDeviceContainer terminalDevices_B;
	//
	//	Spine switches
	//
	NetDeviceContainer spinePlane1SwitchDevices[4];
	for (int i=0; i<4; i++)
		spinePlane1SwitchDevices[i]= NetDeviceContainer();
	//
	// 	Edge switches
	//
	NetDeviceContainer edgePlane1SwitchDevices[1];
	for (int i=0; i<1; i++)
		edgePlane1SwitchDevices[i]= NetDeviceContainer();
	NetDeviceContainer edgePlane2SwitchDevices[1];
	for (int i=0; i<1; i++)
		edgePlane2SwitchDevices[i]= NetDeviceContainer();
        // 
	// Pod switches
	// 
	NetDeviceContainer podSwitch1Devices[1];
	for (int i=0; i<1; i++)
		podSwitch1Devices[i]= NetDeviceContainer();
	NetDeviceContainer podSwitch2Devices[1];
	for (int i=0; i<1; i++)
		podSwitch2Devices[i]= NetDeviceContainer();
	//
	//	Create links between Host H1 and switch P1
	//
	NetDeviceContainer link_H1P1 = csma.Install (NodeContainer (terminals_A.Get(0), podSwitch1.Get(0)));
	terminalDevices_A.Add (link_H1P1.Get(0));
	podSwitch1Devices[0].Add (link_H1P1.Get(1));
	//
	//	Create links between switch P1 and Switch E1
	//
	NetDeviceContainer link_P1E1 = csma.Install (NodeContainer ( podSwitch1.Get(0), edgePlane1Switch.Get(0)));
	podSwitch1Devices[0].Add (link_P1E1.Get(0));
	edgePlane1SwitchDevices[0].Add (link_P1E1.Get(1));
	//
	//	Create links between switch E1 to Switch S1
	//
	NetDeviceContainer link_E1S1 = csma.Install (NodeContainer ( edgePlane1Switch.Get(0), spinePlane1Switch.Get(0)));
	edgePlane1SwitchDevices[0].Add(link_E1S1.Get(0));
	spinePlane1SwitchDevices[0].Add(link_E1S1.Get(1));
	//
	//	Create links between switch E1 to Switch S2
	//
	NetDeviceContainer link_E1S2 = csma.Install (NodeContainer ( edgePlane1Switch.Get(0), spinePlane1Switch.Get(1)));
	edgePlane1SwitchDevices[0].Add(link_E1S2.Get(0));
	spinePlane1SwitchDevices[1].Add(link_E1S2.Get(1));
	//
	//	Create links between switch E1 to Switch S3
	//
	NetDeviceContainer link_E1S3 = csma.Install (NodeContainer ( edgePlane1Switch.Get(0), spinePlane1Switch.Get(2)));
	edgePlane1SwitchDevices[0].Add(link_E1S3.Get(0));
	spinePlane1SwitchDevices[2].Add(link_E1S3.Get(1));
	//
	//	Create links between switch E1 to Switch S4
	//
	NetDeviceContainer link_E1S4 = csma.Install (NodeContainer ( edgePlane1Switch.Get(0), spinePlane1Switch.Get(3)));
	edgePlane1SwitchDevices[0].Add(link_E1S4.Get(0));
	spinePlane1SwitchDevices[3].Add(link_E1S4.Get(1));
	//
	//	Create links between switch S1 to Switch E3
	//
	NetDeviceContainer link_S1E3 = csma.Install (NodeContainer ( spinePlane1Switch.Get(0), edgePlane2Switch.Get(0)));
	spinePlane1SwitchDevices[0].Add(link_S1E3.Get(0));
	edgePlane2SwitchDevices[0].Add(link_S1E3.Get(1));
	//
	//	Create links between switch S2 to Switch E3
	//
	NetDeviceContainer link_S2E3 = csma.Install (NodeContainer ( spinePlane1Switch.Get(1),edgePlane2Switch.Get(0)));
	spinePlane1SwitchDevices[1].Add(link_S2E3.Get(0));
	edgePlane2SwitchDevices[0].Add(link_S2E3.Get(1));
	//
	//	Create links between switch S3 to Switch E3
	//
	NetDeviceContainer link_S3E3 = csma.Install (NodeContainer (  spinePlane1Switch.Get(2),edgePlane2Switch.Get(0) ));
	spinePlane1SwitchDevices[2].Add(link_S3E3.Get(0));
	edgePlane2SwitchDevices[0].Add(link_S3E3.Get(1));
	//
	//	Create links between switch S4 to Switch E3
	//
	NetDeviceContainer link_S4E3 = csma.Install (NodeContainer ( spinePlane1Switch.Get(3), edgePlane2Switch.Get(0)));
	spinePlane1SwitchDevices[3].Add(link_S4E3.Get(0));
	edgePlane2SwitchDevices[0].Add(link_S4E3.Get(1));
	//
	//	Create links between switch E3 and Switch P2
	//
	NetDeviceContainer link_E3P2 = csma.Install (NodeContainer ( podSwitch2.Get(0),edgePlane2Switch.Get(0) ));
	podSwitch2Devices[0].Add(link_E3P2.Get(0));
	edgePlane2SwitchDevices[0].Add(link_E3P2.Get(1));
	//
	//	Create links between switch P2 and Host H2
	//
	NetDeviceContainer link_P2H2 = csma.Install (NodeContainer ( terminals_B.Get(0), podSwitch2.Get(0)));
	terminalDevices_B.Add(link_P2H2.Get(0));
 	podSwitch2Devices[0].Add(link_P2H2.Get(1));
	//
	//	Create the switch netdevice, which will do the packet switching And bridge installation
	//
	Ptr<Node> pPodSwitch1 = podSwitch1.Get(0);
	Ptr<Node> pPodSwitch2 = podSwitch2.Get(0);
	Ptr<Node> pEdgeSwitch_1 = edgePlane1Switch.Get(0);
	Ptr<Node> pEdgeSwitch_3 = edgePlane2Switch.Get(0);
	Ptr<Node> pSpineSwitch_1 = spinePlane1Switch.Get(0);
	Ptr<Node> pSpineSwitch_2 = spinePlane1Switch.Get(1);
	Ptr<Node> pSpineSwitch_3 = spinePlane1Switch.Get(2);
	Ptr<Node> pSpineSwitch_4 = spinePlane1Switch.Get(3);

	BridgeHelper bridge;
	bridge.Install ( pPodSwitch1, podSwitch1Devices[0]);
	bridge.Install ( pPodSwitch2, podSwitch2Devices[0]);
	bridge.Install ( pEdgeSwitch_1, edgePlane1SwitchDevices[0]);
	bridge.Install ( pEdgeSwitch_3, edgePlane2SwitchDevices[0]);
	bridge.Install ( pSpineSwitch_1, spinePlane1SwitchDevices[0]);
	bridge.Install ( pSpineSwitch_2, spinePlane1SwitchDevices[1]);
	bridge.Install ( pSpineSwitch_3, spinePlane1SwitchDevices[2]);
	bridge.Install ( pSpineSwitch_4, spinePlane1SwitchDevices[3]);
	//
	//	Add internet stack to the terminals(All Hosts and switches as well)
	//
	InternetStackHelper internet;
	internet.Install (terminals_A);
	internet.Install (terminals_B);
	internet.Install (podSwitch1);
	internet.Install (podSwitch2);
	internet.Install (edgePlane1Switch);
	internet.Install (edgePlane2Switch);
	internet.Install (spinePlane1Switch);
	//
	//	Assigning IP addresses
	//
	NS_LOG_INFO ("Assign IP Addresses.");


	Ipv4AddressHelper ipv4_1;
	ipv4_1.SetBase ("10.0.1.0", "255.255.255.0");
	ipv4_1.Assign (link_H1P1);

	Ipv4AddressHelper ipv4_2;
	ipv4_2.SetBase ("10.0.3.0", "255.255.255.0");
	ipv4_2.Assign (link_P1E1);

	Ipv4AddressHelper ipv4_3;
	ipv4_3.SetBase ("10.0.4.0", "255.255.255.0");
	ipv4_3.Assign (link_E1S1);

	Ipv4AddressHelper ipv4_4;
	ipv4_4.SetBase ("10.0.5.0", "255.255.255.0");
	ipv4_4.Assign (link_E1S2);

	Ipv4AddressHelper ipv4_5;
	ipv4_5.SetBase ("10.0.6.0", "255.255.255.0");
	ipv4_5.Assign (link_E1S3);

	Ipv4AddressHelper ipv4_6;
	ipv4_6.SetBase ("10.0.7.0", "255.255.255.0");
	ipv4_6.Assign (link_E1S4);

	Ipv4AddressHelper ipv4_7;
	ipv4_7.SetBase ("10.0.8.0", "255.255.255.0");
	ipv4_7.Assign (link_S1E3);
	
	Ipv4AddressHelper ipv4_8;
	ipv4_8.SetBase ("10.0.9.0", "255.255.255.0");
	ipv4_8.Assign (link_S2E3);

	Ipv4AddressHelper ipv4_9;
	ipv4_9.SetBase ("10.0.10.0", "255.255.255.0");
	ipv4_9.Assign (link_S3E3);

	Ipv4AddressHelper ipv4_10;
	ipv4_10.SetBase ("10.0.11.0", "255.255.255.0");
	ipv4_10.Assign (link_S4E3);

	Ipv4AddressHelper ipv4_11;
	ipv4_11.SetBase ("10.0.12.0", "255.255.255.0");
	ipv4_11.Assign (link_E3P2);

	Ipv4AddressHelper ipv4_12;
	ipv4_12.SetBase ("10.0.13.0", "255.255.255.0");
	ipv4_12.Assign (link_P2H2);
	//
	//	Adding static routes
	//
	Ipv4StaticRoutingHelper ipv4RoutingHelper;
	//
	// Host H1 
	//
	Ptr<Ipv4StaticRouting> staticRouting_H1 = ipv4RoutingHelper.GetStaticRouting ((terminals_A.Get(0))->GetObject<Ipv4>());
	staticRouting_H1->AddNetworkRouteTo (Ipv4Address ("10.0.0.0"), Ipv4Mask("255.255.0.0"),Ipv4Address("10.0.1.2"), 1,1);
        //
	// Switch P1
	//
	Ptr<Ipv4StaticRouting> staticRouting_P1 = ipv4RoutingHelper.GetStaticRouting (pPodSwitch1->GetObject<Ipv4>());
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.1.1"), 1,1);

	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.4.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.5.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.6.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.7.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.8.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.9.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.10.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.11.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	staticRouting_P1->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.2"), 2,1);
	//
	// Switch E1
	//
	Ptr<Ipv4StaticRouting> staticRouting_E1 = ipv4RoutingHelper.GetStaticRouting (pEdgeSwitch_1->GetObject<Ipv4>());
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.1"), 1,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.3.1"), 1,0);

	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.4.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.4.2"), 2,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.8.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.4.2"), 2,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.4.2"), 2,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.4.2"), 2,0);

	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.5.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.5.2"), 3,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.9.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.5.2"), 3,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.5.2"), 3,2);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.5.2"), 3,2);

	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.6.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.6.2"), 4,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.10.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.6.2"), 4,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.6.2"), 4,3);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.6.2"), 4,3);

	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.7.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.7.2"), 5,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.11.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.7.2"), 5,0);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.7.2"), 5,4);
	staticRouting_E1->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.7.2"), 5,4);
	//
	// Switch S1
	//
	Ptr<Ipv4StaticRouting> staticRouting_S1 = ipv4RoutingHelper.GetStaticRouting (pSpineSwitch_1->GetObject<Ipv4>());
	staticRouting_S1->AddNetworkRouteTo (Ipv4Address ("10.0.4.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.4.1"), 1,1);
	staticRouting_S1->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.4.1"), 1,1);
	staticRouting_S1->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.4.1"), 1,1);

	staticRouting_S1->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.8.2"), 2,1);
	staticRouting_S1->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.8.2"), 2,1);
	staticRouting_S1->AddNetworkRouteTo (Ipv4Address ("10.0.8.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.8.2"), 2,1);
	//
	// Switch S2
	//
	Ptr<Ipv4StaticRouting> staticRouting_S2 = ipv4RoutingHelper.GetStaticRouting (pSpineSwitch_2->GetObject<Ipv4>());
	staticRouting_S2->AddNetworkRouteTo (Ipv4Address ("10.0.5.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.5.1"), 1,1);
	staticRouting_S2->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.5.1"), 1,1);
	staticRouting_S2->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.5.1"), 1,1);

	staticRouting_S2->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.9.2"), 2,1);
	staticRouting_S2->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.9.2"), 2,1);
	staticRouting_S2->AddNetworkRouteTo (Ipv4Address ("10.0.9.0"), Ipv4Mask("255.255.255.0"),Ipv4Address("10.0.9.2"), 2,1);
	//
	// Switch S3
	//
	Ptr<Ipv4StaticRouting> staticRouting_S3 = ipv4RoutingHelper.GetStaticRouting (pSpineSwitch_3->GetObject<Ipv4>());
	staticRouting_S3->AddNetworkRouteTo (Ipv4Address ("10.0.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.6.1"), 1,1); 
	staticRouting_S3->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.6.1"), 1,1); 
	staticRouting_S3->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.6.1"), 1,1); 

	staticRouting_S3->AddNetworkRouteTo (Ipv4Address ("10.0.10.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.10.2"), 2,1); 
	staticRouting_S3->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.10.2"), 2,1); 
	staticRouting_S3->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.10.2"), 2,1); 
	//
	// Switch S4
	//
	Ptr<Ipv4StaticRouting> staticRouting_S4 = ipv4RoutingHelper.GetStaticRouting (pSpineSwitch_4->GetObject<Ipv4>());
	staticRouting_S4->AddNetworkRouteTo (Ipv4Address ("10.0.7.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.7.1"), 1,1); 
	staticRouting_S4->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.7.1"), 1,1); 
	staticRouting_S4->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.7.1"), 1,1); 

	staticRouting_S4->AddNetworkRouteTo (Ipv4Address ("10.0.11.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.11.2"), 2,1); 
	staticRouting_S4->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.11.2"), 2,1); 
	staticRouting_S4->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.11.2"), 2,1); 
	//
	// Switch E3
	//
	Ptr<Ipv4StaticRouting> staticRouting_E3 = ipv4RoutingHelper.GetStaticRouting (pEdgeSwitch_3->GetObject<Ipv4>());
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.4.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.8.1"), 1,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.8.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.8.1"), 1,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.8.1"), 1,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.8.1"), 1,1); 

	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.5.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.9.1"), 2,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.9.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.9.1"), 2,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.9.1"), 2,2); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.9.1"), 2,2); 

	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.10.1"), 3,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.10.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.10.1"), 3,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.10.1"), 3,3); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.10.1"), 3,3); 

	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.7.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.11.1"), 4,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.11.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.11.1"), 4,1); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.11.1"), 4,4); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.11.1"), 4,4); 

	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.1"), 5,0); 
	staticRouting_E3->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.1"), 5,0); 
	//
	// Switch P2
	//
	Ptr<Ipv4StaticRouting> staticRouting_P2 = ipv4RoutingHelper.GetStaticRouting (pPodSwitch2->GetObject<Ipv4>());
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.12.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.1.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.3.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.4.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.5.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.6.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.7.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.8.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.9.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.10.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 
	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.11.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.12.2"), 1,0); 

	staticRouting_P2->AddNetworkRouteTo (Ipv4Address ("10.0.13.0"), Ipv4Mask("255.255.255.0"), Ipv4Address("10.0.13.1"), 2,0); 
	//
	// Host H2
	//
	Ptr<Ipv4StaticRouting> staticRouting_H2 = ipv4RoutingHelper.GetStaticRouting (terminals_B.Get(0)->GetObject<Ipv4>());
	staticRouting_H2->AddNetworkRouteTo (Ipv4Address ("10.0.0.0"), Ipv4Mask("255.255.0.0"), Ipv4Address("10.0.13.2"), 1,0); 

	NS_LOG_INFO ("Static routing done");
	{
		Ptr<OutputStreamWrapper> routintable = Create<OutputStreamWrapper>("routingtable",std::ios::out);
		Ipv4GlobalRoutingHelper ipv4global;
		ipv4global.PrintRoutingTableAllAt(Seconds(5.0), routintable);
	}

	NS_LOG_INFO ("Create Applications.");

	ApplicationContainer app;
	Simulator::Stop(Seconds (30.0));

	NS_LOG_INFO ("Create ping Application.");
	//
	//	Pinger Application : Start
	//
	uint32_t packetSize = 16;
	Time interPacketInterval =  Seconds(2);
	V4PingHelper PingHost = V4PingHelper(Ipv4Address ("10.0.1.1") );

	PingHost.SetAttribute ("Interval", TimeValue (interPacketInterval));
	PingHost.SetAttribute ("Size", UintegerValue (packetSize));
	if (showPings)
	{
		PingHost.SetAttribute ("Verbose", BooleanValue (true));
	}
	NodeContainer PingDstHosts;
	PingDstHosts.Add(spinePlane1Switch.Get(3));
	PingDstHosts.Add(edgePlane1Switch.Get(0));
	PingDstHosts.Add(podSwitch2.Get(0));
	PingDstHosts.Add(terminals_B.Get(0));
	app = PingHost.Install(PingDstHosts);
	NS_LOG_INFO ("Pinger Application starts.");
	app.Start (Seconds (16.0));
	app.Stop (Seconds (29.0));
	NS_LOG_INFO ("Pinger Application stops.");
	//
	//	Pinger Application : Stop
	//
	NS_LOG_INFO ("Configure Tracing.");
	Config::Connect ("/NodeList/*/ApplicationList/*/$ns3::V4Ping/Rtt", MakeCallback (&PingRtt));
	
	AsciiTraceHelper ascii;
	csma.EnableAsciiAll (ascii.CreateFileStream ("clos_nw.tr"));
	//
	//	Interface packet dumps
	//
	csma.EnablePcapAll ("clos_nw", false);

	NS_LOG_INFO ("Run Simulation.");
	//
	//	Animator 
	//
	AnimationInterface anim("clos_nw.xml");

	anim.SetConstantPosition(spinePlane1Switch.Get(0), 5, 5);
	anim.SetConstantPosition(spinePlane1Switch.Get(1)  , 10, 5);
	anim.SetConstantPosition(spinePlane1Switch.Get(2),  15, 5);
	anim.SetConstantPosition(spinePlane1Switch.Get(3),  20, 5);

	anim.SetConstantPosition(edgePlane1Switch.Get(0),  10, 10);
	anim.SetConstantPosition(edgePlane2Switch.Get(0),  20, 10);

	anim.SetConstantPosition(podSwitch1.Get(0),  12.5, 15);
	anim.SetConstantPosition(podSwitch2.Get(0),  17.5, 15);

	anim.SetConstantPosition(terminals_A.Get(0),  12.5, 20);
	anim.SetConstantPosition(terminals_B.Get(0),  17.5, 20);

	Simulator::Run ();
	Simulator::Destroy ();
	NS_LOG_INFO ("Done.");
}