//
// Created by edgar costa molero on 05.05.18.
//



#include <fstream>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/utils-module.h"
#include "ns3/traffic-generation-module.h"
#include "ns3/custom-applications-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("flow-error-model-test");


static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
    NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
    *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

static void
RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
{
    NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
    file->Write (Simulator::Now (), p);
}

void ClearError(Ptr<FlowErrorModel> em){

    em->SetAttribute("FlowErrorRate", DoubleValue(0));
    em->SetNormalErrorModelAttribute("ErrorRate", DoubleValue(0));
    em->Reset();
    //    DynamicCast<PointToPointNetDevice>(link2.Get (1))->SetReceiveErrorModel(em);

}

int
main (int argc, char *argv[])
{
    CommandLine cmd;
    cmd.Parse (argc, argv);

    NodeContainer nodes;
    nodes.Create (3);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("10Mbps"));
    pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

    Config::SetDefault("ns3::TcpSocket::DataRetries", UintegerValue(10)); //retranmissions
    Config::SetDefault("ns3::TcpSocketBase::MinRto", TimeValue(MilliSeconds(200))); //min RTO value that can be set
    Config::SetDefault("ns3::TcpSocketBase::Sack", BooleanValue(true)); //enable sack

    LogComponentEnable("FlowErrorModel", LOG_ALL);
    //LogComponentEnable("ErrorModel", LOG_ALL);

    NetDeviceContainer link1;
    NetDeviceContainer link2;
    link1 = pointToPoint.Install (NodeContainer(nodes.Get(0), nodes.Get(1)));
    link2 = pointToPoint.Install (NodeContainer(nodes.Get(1), nodes.Get(2)));

    Ptr<RateErrorModel> em2 = CreateObject<RateErrorModel>();
    em2->SetAttribute ("ErrorRate", DoubleValue (0.01));

    //em->Disable();
//    Ptr<FlowErrorModel> em = CreateObject<FlowErrorModel> ();
//    Ptr<BurstErrorModel> rem = CreateObject<BurstErrorModel>();
//    em->SetNormalErrorModel(rem);
//    em->SetAttribute("FlowErrorRate", DoubleValue(0.5));
//    em->SetNormalErrorModelAttribute("ErrorRate", DoubleValue(0.01));
//    em->SetNormalErrorModelAttribute("BurstSize", StringValue ("ns3::UniformRandomVariable[Min=5|Max=5]"));
//    //em->SetNormalErrorModelAttribute("ErrorUnit", EnumValue(RateErrorModel::ERROR_UNIT_PACKET) );
//
//    //Simulator::Schedule(Seconds(8), &ClearError, em);
//    //Simulator::Schedule(Seconds(8), &ClearError, em);
//
//    DynamicCast<PointToPointNetDevice>(link2.Get (1))->SetReceiveErrorModel(em);

    SetFlowErrorModel(link2);
    ChangeFlowErrorDropRate(link2, 0);

    Simulator::Schedule(Seconds(1), &ChangeFlowErrorDropRate, link2, 0);
    Simulator::Schedule(Seconds(1.1), &SetFlowErrorNormalDropRate,link2, 0.1);
    Simulator::Schedule(Seconds(1.1), &SetFlowErrorNormalBurstSize, link2, 1,5);
    Simulator::Schedule(Seconds(1.8), &ClearFlowErrorModel, link2);
    Simulator::Schedule(Seconds(2), &SetFlowErrorNormalDropRate,link2, 0.6);
    Simulator::Schedule(Seconds(10), &ClearFlowErrorModel, link2);

    //Simulator::Schedule(Seconds(20), &SetFlowErrorModel, link2);

    InternetStackHelper stack;
    stack.Install (nodes);

    Ipv4AddressHelper address;
    address.SetBase ("10.1.1.0", "255.255.255.252");
    Ipv4InterfaceContainer interfaces = address.Assign (link1);
    address.NewNetwork();
    Ipv4InterfaceContainer interfaces1 = address.Assign (link2);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    //traffic
    uint16_t dst_port = 7001;
    InstallSink(nodes.Get(2), dst_port, 0, "TCP");

    for (int i =0 ; i < 100; i++) {
        InstallNormalBulkSend(nodes.Get(0), nodes.Get(2), dst_port, 50000, 1);
    }
    //InstallNormalBulkSend(nodes.Get(0), nodes.Get(2), dst_port, 1500000, 1);
    //InstallNormalBulkSend(nodes.Get(0), nodes.Get(2), dst_port, 1500000, 1);

    pointToPoint.EnablePcap("out", link1.Get(0), bool(1));

    //Packet::EnablePrinting();
    PcapHelper pcapHelper;
    Ptr<PcapFileWrapper> pcap_file = pcapHelper.CreateFile("drops.pcap", std::ios::out, PcapHelper::DLT_PPP);

    link2.Get (1)->TraceConnectWithoutContext ("PhyRxDrop", MakeBoundCallback (&TracePcap, pcap_file));
    link2.Get (1)->TraceConnectWithoutContext ("PhyTxDrop", MakeBoundCallback (&TracePcap, pcap_file));
    link2.Get (1)->TraceConnectWithoutContext ("MacTxDrop", MakeBoundCallback (&TracePcap, pcap_file));

    //Simulator::Stop (Seconds (100000));
    Simulator::Run ();
    Simulator::Destroy ();

    return 0;
}
