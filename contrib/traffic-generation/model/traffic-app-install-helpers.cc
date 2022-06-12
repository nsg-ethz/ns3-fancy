/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <random>

#include "ns3/applications-module.h"
#include "ns3/traffic-generation-module.h"
#include "ns3/custom-applications-module.h"
#include "ns3/utils-module.h"

#include <algorithm>

NS_LOG_COMPONENT_DEFINE ("TrafficAppInstallHelpers");

namespace ns3 {

void
InstallSink (Ptr<Node> node, uint16_t sinkPort, uint32_t duration, std::string protocol)
{
  //create sink helper
  PacketSinkHelper packetSinkHelper ("ns3::TcpSocketFactory",
                                     InetSocketAddress (Ipv4Address::GetAny (), sinkPort));

  if (protocol == "UDP")
    {
      packetSinkHelper.SetAttribute ("Protocol", StringValue ("ns3::UdpSocketFactory"));
    }

  ApplicationContainer sinkApps = packetSinkHelper.Install (node);

  sinkApps.Start (Seconds (0));
  //Only schedule a stop it duration is bigger than 0 seconds
  if (duration != 0)
    {
      sinkApps.Stop (Seconds (duration));
    }
}

std::unordered_map<std::string, std::vector<uint16_t>>
InstallSinks (NodeContainer receivers, uint16_t start_port, uint16_t end_port, uint32_t duration,
              std::string protocol)
{

  std::unordered_map<std::string, std::vector<uint16_t>> hostsToPorts;

  for (uint32_t i = 0; i < receivers.GetN (); i++)
    {
      std::string host_name = GetNodeName (receivers.Get (i));
      hostsToPorts[host_name] = std::vector<uint16_t> ();
      for (uint16_t j = start_port; j <= end_port; j++)
        {
          InstallSink (receivers.Get (i), j, duration, protocol);
        }
    }
  return hostsToPorts;
}

Ptr<Socket>
InstallSimpleSend (Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t sinkPort, DataRate dataRate,
                   uint32_t numPackets, std::string protocol)
{
  Ptr<Socket> ns3Socket;
  uint32_t num_packets = numPackets;

  //had to put an initial value
  if (protocol == "TCP")
    {
      ns3Socket = Socket::CreateSocket (srcHost, TcpSocketFactory::GetTypeId ());
    }
  else
    {
      ns3Socket = Socket::CreateSocket (srcHost, UdpSocketFactory::GetTypeId ());
    }

  Ipv4Address addr = GetNodeIp (dstHost);

  Address sinkAddress (InetSocketAddress (addr, sinkPort));

  Ptr<SimpleSend> app = CreateObject<SimpleSend> ();
  app->Setup (ns3Socket, sinkAddress, 1440, num_packets, dataRate);

  srcHost->AddApplication (app);

  app->SetStartTime (Seconds (1.));
  //Stop not needed since the simulator stops when there are no more packets to send.
  //app->SetStopTime (Seconds (1000.));
  return ns3Socket;
}

void
InstallOnOffSend (Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, DataRate dataRate,
                  uint32_t packet_size, uint64_t max_size, double startTime)
{
  Ipv4Address addr = GetNodeIp (dstHost);
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<OnOffApplication> onoff_sender = CreateObject<OnOffApplication> ();

  onoff_sender->SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  onoff_sender->SetAttribute ("Remote", AddressValue (sinkAddress));

  onoff_sender->SetAttribute ("DataRate", DataRateValue (dataRate));
  onoff_sender->SetAttribute ("PacketSize", UintegerValue (packet_size));
  onoff_sender->SetAttribute ("MaxBytes", UintegerValue (max_size));

  srcHost->AddApplication (onoff_sender);
  onoff_sender->SetStartTime (Seconds (startTime));
  //TODO: check if this has some implication.
  onoff_sender->SetStopTime (Seconds (1000));
}

/* Used by fancy schedulers */
void
InstallRateSend (Ptr<Node> srcHost, std::string dst_ip, uint16_t dport, uint32_t n_packets,
                 uint64_t max_size, double duration, double rtt, double startTime,
                 std::string protocol)
{

  NS_LOG_DEBUG ("test: " << n_packets << " " << max_size << " " << duration << " " << rtt);

  //if (duration <= 0) {
  //  NS_LOG_DEBUG("Install Rate Send: \t Bulk Send Instead!");
  //  InstallNormalBulkSend(srcHost, dstHost, dport, max_size, startTime);
  //  return;
  //}

  Ipv4Address addr = Ipv4Address (Ipv4Address (dst_ip.c_str ()).Get ());
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<RateSendApplication> rate_send_app = CreateObject<RateSendApplication> ();

  if (protocol == "TCP")
    {
      rate_send_app->SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
    }
  else if (protocol == "UDP")
    {
      rate_send_app->SetAttribute ("Protocol", TypeIdValue (UdpSocketFactory::GetTypeId ()));
    }
  rate_send_app->SetAttribute ("Remote", AddressValue (sinkAddress));

  double avg_size_per_packet;
  double interval_duration;
  double bytes_per_sec;

  long int max_size_int = (long int) (max_size);

  // this is done to ajust to payload sizes, but in reality for us it does not matter that much anymore.
  max_size_int = std::max(long(64 * n_packets), max_size_int); // we force it a bit...
  max_size_int = max_size_int - (n_packets * 54);

  NS_ASSERT_MSG (max_size_int >= 0, "Too many packets: " << n_packets << " for the flow size: " << max_size);

  //real number of packets to send
  //remove syn, ack, fin, ack
  // lets remove max 2 packets, for blink we removed 4
  if (n_packets > 2)
  {
    n_packets = n_packets - 2;
  }

  //We will also consider 2 times RTT in order to take into account that for flow duration, since we can not get rid of syn and fin
  if (duration > 2 * rtt)
  {
    duration -= (2 * rtt);
  }
  else
  {
    //very small duration 1ms
    duration = 0.001; 
  }

  bytes_per_sec = double (max_size_int) / duration;
  avg_size_per_packet = double (max_size_int) / n_packets;
  // we set it to zero, in theory it does not even matter since it will last 1 iteration right?
  if (bytes_per_sec == 0)
  {
    interval_duration = 0;
  }
  else
  {
    interval_duration = avg_size_per_packet / bytes_per_sec;
  }
  
  uint64_t bytes_per_period = uint64_t (avg_size_per_packet);

  NS_LOG_DEBUG ("Flow Features:    "
                << "\tNumber Packets(no syn/fin): " << n_packets << "\t Bytes To send: " << max_size
                << "\t Payload Bytes to Send: " << max_size_int << "\t Duration: " << duration);

  NS_LOG_DEBUG ("Install Rate Send: "
                << "\tBytes per sec: " << bytes_per_sec << "\t Avg Payload Size: "
                << avg_size_per_packet << "\t Interval Duration: " << interval_duration
                << "\t Packets Per Second: " << 1 / interval_duration << "\n");

  rate_send_app->SetAttribute ("MaxBytes", UintegerValue (max_size_int));
  rate_send_app->SetAttribute ("BytesPerInterval", UintegerValue (bytes_per_period));
  rate_send_app->SetAttribute ("IntervalDuration", DoubleValue (interval_duration));

  srcHost->AddApplication (rate_send_app);
  rate_send_app->SetStartTime (Seconds (startTime));
  //TODO: check if this has some implication.
  rate_send_app->SetStopTime (Seconds (10000));
}

/* old method used in blink, the only difference at the time of writing this
   with the new one is that it uses dstHost noe instead of dst ip */
void
InstallRateSend (Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint32_t n_packets,
                 uint64_t max_size, double duration, double rtt, double startTime)
{

  NS_LOG_DEBUG ("test: " << n_packets << " " << max_size << " " << duration << " " << rtt);

  if (duration <= 0)
    {
      NS_LOG_DEBUG ("Install Rate Send: \t Bulk Send Instead!");
      InstallNormalBulkSend (srcHost, dstHost, dport, max_size, startTime);
      return;
    }

  Ipv4Address addr = GetNodeIp (dstHost);
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<RateSendApplication> rate_send_app = CreateObject<RateSendApplication> ();

  rate_send_app->SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  rate_send_app->SetAttribute ("Remote", AddressValue (sinkAddress));

  double avg_size_per_packet;
  double interval_duration;
  double bytes_per_sec;

  long int max_size_int = (long int) (max_size);
  max_size_int = max_size_int - (n_packets * 54);
  NS_ASSERT_MSG (max_size_int > 0, "Too many packets: " << n_packets
                                                        << " for the flow size: " << max_size
                                                        << " bytes left" << max_size_int);

  //real number of packets to send
  //remove syn, ack, fin, ack
  if (n_packets > 4)
    {
      n_packets = n_packets - 4;
    }

  //We will also consider 2 times RTT in order to take into account that for flow duration, since we can not get rid of it
  if (duration > 2 * rtt)
    {
      duration -= (2 * rtt);
    }
  else
    {
      //very small duration
      duration = 0.001;
    }

  bytes_per_sec = double (max_size_int) / duration;
  avg_size_per_packet = double (max_size_int) / n_packets;
  interval_duration = avg_size_per_packet / bytes_per_sec;
  uint64_t bytes_per_period = uint64_t (avg_size_per_packet);

  NS_LOG_DEBUG ("Flow Features:    "
                << "\tNumber Packets(no syn/fin): " << n_packets << "\t Bytes To send: " << max_size
                << "\t Payload Bytes to Send: " << max_size_int << "\t Duration: " << duration);

  NS_LOG_DEBUG ("Install Rate Send: "
                << "\tBytes per sec: " << bytes_per_sec << "\t Avg Payload Size: "
                << avg_size_per_packet << "\t Interval Duration: " << interval_duration
                << "\t Packets Per Second: " << 1 / interval_duration << "\n");

  rate_send_app->SetAttribute ("MaxBytes", UintegerValue (max_size_int));
  rate_send_app->SetAttribute ("BytesPerInterval", UintegerValue (bytes_per_period));
  rate_send_app->SetAttribute ("IntervalDuration", DoubleValue (interval_duration));

  srcHost->AddApplication (rate_send_app);
  rate_send_app->SetStartTime (Seconds (startTime));
  /* Should we stop the flow??? */
  rate_send_app->SetStopTime (Seconds (10000));
}

void
InstallNormalBulkSend (Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size,
                       double startTime)
{

  Ipv4Address addr = GetNodeIp (dstHost);
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<BulkSendApplication> bulkSender = CreateObject<BulkSendApplication> ();

  bulkSender->SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  bulkSender->SetAttribute ("MaxBytes", UintegerValue (size));
  bulkSender->SetAttribute ("Remote", AddressValue (sinkAddress));

  //Install app
  srcHost->AddApplication (bulkSender);

  bulkSender->SetStartTime (Seconds (startTime));
  //bulkSender->SetStopTime(Seconds(1000));

  return;
  //  return socket;
}

//DO THE SAME WITH THE BULK APP, WHICH IS PROBABLY WHAT WE WANT TO HAVE.
void
InstallBulkSend (Ptr<Node> srcHost, Ptr<Node> dstHost, uint16_t dport, uint64_t size,
                 double startTime, Ptr<OutputStreamWrapper> fctFile,
                 Ptr<OutputStreamWrapper> counterFile, Ptr<OutputStreamWrapper> flowsFile,
                 uint64_t flowId, uint64_t *recordedFlowsCounter, double *startRecordingTime,
                 double recordingTime)
{

  Ipv4Address addr = GetNodeIp (dstHost);
  Address sinkAddress (InetSocketAddress (addr, dport));

  Ptr<CustomBulkApplication> bulkSender = CreateObject<CustomBulkApplication> ();

  //  Ptr<Socket> socket;
  //  socket = Socket::CreateSocket (srcHost, TcpSocketFactory::GetTypeId ());
  //  bulkSender->SetSocket(socket);

  bulkSender->SetAttribute ("Protocol", TypeIdValue (TcpSocketFactory::GetTypeId ()));
  bulkSender->SetAttribute ("MaxBytes", UintegerValue (size));
  bulkSender->SetAttribute ("Remote", AddressValue (sinkAddress));
  bulkSender->SetAttribute ("FlowId", UintegerValue (flowId));

  bulkSender->SetOutputFile (fctFile);
  bulkSender->SetCounterFile (counterFile);
  bulkSender->SetFlowsFile (flowsFile);

  bulkSender->SetStartRecordingTime (startRecordingTime);
  bulkSender->SetRecordingTime (recordingTime);
  bulkSender->SetRecordedFlowsCounter (recordedFlowsCounter);

  //Install app
  srcHost->AddApplication (bulkSender);

  bulkSender->SetStartTime (Seconds (startTime));
  bulkSender->SetStopTime (Seconds (1000));

  return;
}

void
SendBindTest (Ptr<Node> src, NodeContainer receivers,
              std::unordered_map<std::string, std::vector<uint16_t>> hostsToPorts, uint32_t flows)
{

  Ptr<UniformRandomVariable> rand = CreateObject<UniformRandomVariable> ();
  for (uint32_t i = 0; i < flows; i++)
    {
      Ptr<Node> dst = receivers.Get (random_variable->GetInteger (0, receivers.GetN () - 1));
      std::vector<uint16_t> availablePorts = hostsToPorts[GetNodeName (dst)];
      uint16_t dport = RandomFromVector<uint16_t> (availablePorts, rand);
      InstallNormalBulkSend (src, dst, dport, 50000, random_variable->GetValue (1, 10));
    }
}

} // namespace ns3
