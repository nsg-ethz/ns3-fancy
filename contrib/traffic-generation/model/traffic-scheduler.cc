/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include <random>
#include "ns3/custom-applications-module.h"
#include "ns3/applications-module.h"
#include "ns3/utils-module.h"
#include "ns3/traffic-generation-module.h"


NS_LOG_COMPONENT_DEFINE("TrafficScheduler");


namespace ns3 {

  std::vector<FlowMetadata>
    StatefulSyntheticTrafficSchedulerOneShot(
      std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node, double rtt,
      uint32_t seed, uint32_t total_flows, uint32_t prefixes, DataRate bw, double start_time, double warm_up_time,
      double send_duration, uint32_t elephant_flows, double elephant_byte_share,
      uint16_t start_port, uint16_t end_port, std::string output_file, double udp_share)
  {

    /* Assertions */
    NS_ASSERT_MSG(total_flows >= elephant_flows, "Trying to start more elephant flows than total flows:" << total_flows << "<=" << elephant_flows);

    // If all flows are elephant we fix the 
    if (total_flows == elephant_flows)
    {
      elephant_byte_share = 1;
    }
    if (elephant_flows == 0)
    {
      elephant_byte_share = 0;
    }
    double mice_byte_share = 1 - elephant_byte_share;
    uint32_t mice_flows = total_flows - elephant_flows;

    std::cout << "Starting one shot Stateful Synthetic traffic Scheduler" << std::endl;

    /* we create a file where we store the first packet
       of each prefix we use that to count the detection time */
    std::unordered_map<std::string, double> first_packet;

    //Log output file
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> first_packet_file;
    if (output_file != "")
    {
      first_packet_file = asciiTraceHelper.CreateFileStream(output_file);
    }

    Ipv4Address start_addr = Ipv4Address("192.0.1.1");

    /* computing flow sizes */
    uint32_t elephant_avg_packet_size = 500; // 750
    uint32_t mice_avg_packet_size = 500; // 750
    double flow_duration = send_duration;
    double total_byte_rate = bw.GetBitRate() / 8;
    double prefix_byte_rate = total_byte_rate / prefixes;
    double prefix_bytes = prefix_byte_rate * send_duration;

    /* flow byte rates */
    double elephant_flow_bytes = (prefix_bytes * elephant_byte_share) / elephant_flows;
    double mice_flow_bytes = (prefix_bytes * mice_byte_share) / mice_flows;

    if (elephant_flow_bytes < elephant_avg_packet_size)
      elephant_avg_packet_size = elephant_flow_bytes;

    if (mice_flow_bytes < mice_avg_packet_size)
      mice_avg_packet_size = mice_flow_bytes;

    uint32_t elephant_packets_to_send = (elephant_flow_bytes) / elephant_avg_packet_size;
    uint32_t mice_packets_to_send = (mice_flow_bytes) / mice_avg_packet_size;

    // Some info about the flows
    std::cout << "Starting " << elephant_flows << " elephant flows. Packets to send: " << elephant_packets_to_send << " bytes/s: " << elephant_flow_bytes << std::endl;
    std::cout << "Starting " << mice_flows << " mice flows. Packets to send: " << mice_packets_to_send << " bytes/s: " << mice_flow_bytes << std::endl;

    // Create fake prefixes
      //Load flow distribution
    std::vector<FlowMetadata> prefixesDist;
    for (uint32_t i = 0; i < prefixes; i++)
    {
      FlowMetadata flow;
      flow.prefix = Ipv4AddressToString(Ipv4Address(start_addr.Get() + (256 * i)));
      flow.bytes = 0;
      flow.packets = 0;
      flow.duration = flow_duration;
      flow.rtt = rtt;
      prefixesDist.push_back(flow);
    }

    NS_LOG_DEBUG("Number of prefixes to use: " << prefixesDist.size());

    double startTime = start_time;
    double simulationTime = send_duration;
    uint64_t num_flows_started = 0;

    Ptr<UniformRandomVariable> random_variable = CreateObject<UniformRandomVariable>();

    // for every prefix 
    for (uint32_t j = 0; j < prefixes; j++)
    {
      FlowMetadata flow = prefixesDist[j];

      for (double f = 0; f < total_flows; f++)
      {
        /* start time */
        double flow_start_time = startTime + random_variable->GetValue(0, warm_up_time);
        double rtt = flow.rtt;

        NS_ASSERT_MSG(senders_latency_to_node[rtt].size() >= 1, "There are no source hosts for rtt: " << rtt);
        Ptr<Node> src = RandomFromVector<Ptr<Node>>(senders_latency_to_node[rtt], random_variable);

        // Destination port 
        uint16_t dport = random_variable->GetInteger(start_port, end_port);

        // Get protocol
        double proto_change = random_variable->GetValue(0, 1);
        std::string protocol = "TCP";
        if (proto_change < udp_share)
        {
          protocol = "UDP";
        }

        // Set parameters depending on the flow type
        if (f < elephant_flows)
        {
          flow.bytes = elephant_flow_bytes;
          flow.packets = elephant_packets_to_send;
        }
        else
        {
          flow.bytes = mice_flow_bytes;
          flow.packets = mice_packets_to_send;
        }

        if (num_flows_started < 5)
        {
          NS_LOG_UNCOND(
            "Flow Features:    " << "\tSrc: " << GetNodeName(src)
            << "(" << GetNodeIp(src) << ")"
            << "\tDst: " << flow.prefix
            << "\tReal_rtt: " << flow.rtt
            << "\tRtt Found: " << rtt
            << "\tBytes: " << flow.bytes
            << "\tPackets: " << flow.packets
            << "\tStart time: " << flow_start_time
            << "\tProtocol " << protocol
            << "\tDuration: " << flow.duration);
        }

        InstallRateSend(src, flow.prefix, dport, flow.packets, flow.bytes, flow.duration, rtt, flow_start_time, protocol);
        // Just print the first 100
        num_flows_started++;
        /* update first packet data structure */
        if (first_packet.count(flow.prefix) == 0)
        {
          first_packet[flow.prefix] = flow_start_time;
        }
      }
    }

    NS_LOG_UNCOND("Number of flows Started: " << num_flows_started);
    if (output_file != "")
    {
      for (auto& it : first_packet)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }
    }
    return prefixesDist;
  }

  std::vector<FlowMetadata>
    StatefulSyntheticTrafficScheduler(
      std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node, double rtt,
      uint32_t seed, uint32_t flows_per_sec, uint32_t prefixes, DataRate bw, double start_time,
      double duration, uint16_t start_port, uint16_t end_port, std::string output_file,
      double udp_share)
  {
    std::cout << "Starting Stateful Synthetic traffic Scheduler" << std::endl;

    /* we create a file where we store the first packet
       of each prefix we use that to count the detection time */
    std::unordered_map<std::string, double> first_packet;

    //Log output file
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> first_packet_file;
    if (output_file != "")
    {
      first_packet_file = asciiTraceHelper.CreateFileStream(output_file);
    }

    //Load flow distribution
    std::vector<FlowMetadata> flowDist;

    Ipv4Address start_addr = Ipv4Address("192.0.1.1");

    /* computing flow sizes */
    uint32_t avg_packet_size = 500; // 750
    double flow_duration = 1;
    double total_byte_rate = bw.GetBitRate() / 8;
    double prefix_byte_rate = total_byte_rate / prefixes;
    double flow_byte_rate = prefix_byte_rate / flows_per_sec;

    if (flow_byte_rate < avg_packet_size)
      avg_packet_size = flow_byte_rate;

    uint32_t packets_to_send = (flow_byte_rate * flow_duration) / avg_packet_size;

    // Create fake prefixes
    for (uint32_t i = 0; i < prefixes; i++)
    {
      FlowMetadata flow;
      flow.prefix = Ipv4AddressToString(Ipv4Address(start_addr.Get() + (256 * i)));
      flow.bytes = flow_byte_rate;
      flow.duration = flow_duration;
      flow.packets = packets_to_send;
      flow.rtt = rtt;

      flowDist.push_back(flow);
    }

    NS_LOG_DEBUG("Number of flows to use: " << flowDist.size());

    double startTime = start_time;
    double simulationTime = duration;
    uint64_t num_flows_started = 0;

    Ptr<UniformRandomVariable> random_variable = CreateObject<UniformRandomVariable>();

    // for every second
    while ((startTime - 1) < simulationTime) {
      // for every prefix 
      for (uint32_t j = 0; j < prefixes; j++)
      {
        FlowMetadata flow = flowDist[j];

        // Start flows per sec flows during the first 250ms of each second. 
        for (double f = 0; f < flows_per_sec; f++)
        {
          //double flow_start_time = startTime + ((0.25/flows_per_sec) * f);
          double flow_start_time = startTime + random_variable->GetValue(0, 1);
          double rtt = flow.rtt;

          NS_ASSERT_MSG(senders_latency_to_node[rtt].size() >= 1, "There are no source hosts for rtt: " << rtt);
          Ptr<Node> src = RandomFromVector<Ptr<Node>>(senders_latency_to_node[rtt], random_variable);

          //Destination port
          uint16_t dport = random_variable->GetInteger(start_port, end_port);

          // removes /24
          /* This is not needed here */
          //std::string dst_ip = flow.prefix.substr(0, flow.prefix.find("/"));
          //replaces last 0 with a 1
          //dst_ip = dst_ip.substr(0, dst_ip.length() - 1) + "1";

          // Get protocol
          double proto_change = random_variable->GetValue(0, 1);
          std::string protocol = "TCP";
          if (proto_change < udp_share)
          {
            protocol = "UDP";
          }

          if (num_flows_started < 5)
          {
            NS_LOG_UNCOND(
              "Flow Features:    " << "\tSrc: " << GetNodeName(src)
              << "(" << GetNodeIp(src) << ")"
              << "\tDst: " << flow.prefix
              << "\tReal_rtt: " << flow.rtt
              << "\tRtt Found: " << rtt
              << "\tBytes: " << flow.bytes
              << "\tPackets: " << flow.packets
              << "\tStart time: " << flow_start_time
              << "\tProtocol " << protocol
              << "\tDuration: " << flow.duration);
          }

          InstallRateSend(src, flow.prefix, dport, flow.packets, flow.bytes, flow.duration, rtt, flow_start_time, protocol);

          // Just print the first 100


          num_flows_started++;

          /* update first packet data structure */
          if (first_packet.count(flow.prefix) == 0)
          {
            first_packet[flow.prefix] = flow_start_time;
          }
        }
      }
      /* increase by one */
      startTime += 1;
    }

    NS_LOG_UNCOND("Number of flows Started: " << num_flows_started);
    if (output_file != "")
    {
      for (auto& it : first_packet)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }
    }
    return flowDist;
  }

  std::unordered_map<std::string, TrafficPrefixStats>
    StatefulTrafficScheduler(std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node,
      std::vector<double> rtt_cdf, std::string flowDistFile,
      uint32_t seed, uint32_t flows_per_sec, double start_time, double duration,
      double failure_time, uint16_t start_port, uint16_t end_port, std::string output_file)
  {

    std::cout << "Starting Stateful Traffic Scheduler" << std::endl;

    /* we create a file where we store the first packet
       of each prefix we use that to count the detection time */

    std::unordered_map<std::string, double> first_packet;
    std::unordered_map<std::string, double> first_packet_after_failure;

    /* Status to maybe use externally and for the failures or something */
    std::unordered_map<std::string, TrafficPrefixStats> prefixes_stats;

    //Log output file
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> first_packet_file;
    if (output_file != "")
    {
      first_packet_file = asciiTraceHelper.CreateFileStream(output_file);
    }

    //Load flow distribution
    std::unordered_set<uint32_t> empty_filter;
    std::vector<FlowMetadata> flowDist = GetFlowsPerPrefixFromDist(flowDistFile, empty_filter);

    NS_LOG_DEBUG("Number of flows to use: " << flowDist.size());

    //Usage object

    std::random_device rd;  //Will be used to obtain a seed for the random number engine
    std::mt19937 gen(rd()); //Standard mersenne_twister_engine seeded with rd()
    gen.seed(seed);
    std::exponential_distribution<double> FlowsPerSec(flows_per_sec);

    double startTime = start_time;
    double simulationTime = duration;
    uint64_t num_flows_started = 0;

    Ptr<UniformRandomVariable> random_variable = CreateObject<UniformRandomVariable>();

    while ((startTime - 1) < simulationTime) {

      //get a random flow
      FlowMetadata flow = RandomFromVector<FlowMetadata>(flowDist, random_variable);

      /* Not sure this is needed anymore */
      //if (flow.duration == 0) {
      //  flow.duration = 0.25;
      //}

      double rtt = FindClosest(rtt_cdf, flow.rtt);
      NS_ASSERT_MSG(senders_latency_to_node[rtt].size() >= 1, "There are no source hosts for rtt: " << rtt);
      /* Get one of the possible hosts suitable for this RTT */
      Ptr<Node> src = RandomFromVector<Ptr<Node>>(senders_latency_to_node[rtt], random_variable);

      //Destination port
      uint16_t dport = random_variable->GetInteger(start_port, end_port);

      startTime += FlowsPerSec(gen);

      /* WARNING: The flows loader handles this */
      // removes /24
      //std::string dst_ip = flow.prefix.substr(0, flow.prefix.find("/"));
      //replaces last 0 with a 1
      //dst_ip = dst_ip.substr(0, dst_ip.length() - 1) + "1";
      //std::cout << int(flow.protocol) << std::endl;
      std::string protocol = "";
      if (flow.protocol == 6)
      {
        protocol = "TCP";
      }
      else
      {
        protocol = "UDP";
      }

      if (num_flows_started < 100)
      {
        NS_LOG_UNCOND(
          "Flow Features:    " << "\tSrc: " << GetNodeName(src)
          << "(" << GetNodeIp(src) << ")"
          << "\tDst: " << flow.prefix
          << "\tReal_rtt: " << flow.rtt
          << "\tRtt Found: " << rtt
          << "\tBytes: " << flow.bytes
          << "\tPackets: " << flow.packets
          << "\tStart time: " << startTime
          << "\tProtocol " << protocol
          << "\tDuration: " << flow.duration);
      }

      InstallRateSend(src, flow.prefix, dport, flow.packets, flow.bytes, flow.duration, rtt, startTime, protocol);

      //return prefixes_stats;

      /* update stats*/
      if (prefixes_stats.count(flow.prefix) == 0)
      {
        TrafficPrefixStats stats;
        stats.packets = flow.packets;
        stats.bytes = flow.bytes;
        stats.flows = 1;
        prefixes_stats[flow.prefix] = stats;
      }
      else
      {
        prefixes_stats[flow.prefix].packets += flow.packets;
        prefixes_stats[flow.prefix].bytes += flow.bytes;
        prefixes_stats[flow.prefix].flows += 1;
      }

      num_flows_started++;

      /* update first packet data structure */
      if (first_packet.count(flow.prefix) == 0)
      {
        first_packet[flow.prefix] = startTime;
        prefixes_stats[flow.prefix].first_packet = startTime;
      }
      /* for the first packet after our network failure, which comes in bursts */
      if ((startTime > failure_time) && (first_packet_after_failure.count(flow.prefix) == 0))
      {
        first_packet_after_failure[flow.prefix] = startTime;
        prefixes_stats[flow.prefix].first_packet_after_failure = startTime;
      }
    }

    NS_LOG_UNCOND("Number of flows Started: " << num_flows_started);
    if (output_file != "")
    {
      *(first_packet_file->GetStream()) << "#First" << "\n";
      for (auto& it : first_packet)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }

      *(first_packet_file->GetStream()) << "#After failure" << "\n";
      for (auto& it : first_packet_after_failure)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }
    }
    return prefixes_stats;
  }

  /* Based on a copy of the trace not on a distribution */
  std::unordered_map<std::string, TrafficPrefixStats>
    StatefulTraceTrafficScheduler(std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node,
      std::vector<double> rtt_cdf, std::string flowDistFile,
      uint32_t seed, double start_time, double duration,
      double failure_time, uint16_t start_port, uint16_t end_port, std::string output_file)
  {

    std::cout << "Starting Stateful Trace Traffic Scheduler" << std::endl;

    /* we create a file where we store the first packet
       of each prefix we use that to count the detection time */

    std::unordered_map<std::string, double> first_packet;
    std::unordered_map<std::string, double> first_packet_after_failure;

    /* Status to maybe use externally and for the failures or something */
    std::unordered_map<std::string, TrafficPrefixStats> prefixes_stats;

    //Log output file
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> first_packet_file;
    if (output_file != "")
    {
      first_packet_file = asciiTraceHelper.CreateFileStream(output_file);
    }

    //Load flow distribution
    std::unordered_set<uint32_t> empty_filter;
    std::vector<FlowMetadata> flowDist = GetFlowsPerPrefixFromDist(flowDistFile, empty_filter);

    NS_LOG_UNCOND("Number of flows to use: " << flowDist.size());

    //Usage object
    double startTime = start_time;
    double simulationTime = duration;
    uint64_t num_flows_started = 0;

    Ptr<UniformRandomVariable> random_variable = CreateObject<UniformRandomVariable>();


    for (uint32_t i = 0; i < flowDist.size(); i++)
    {

      FlowMetadata flow = flowDist[i];
      double rtt = FindClosest(rtt_cdf, flow.rtt);
      NS_ASSERT_MSG(senders_latency_to_node[rtt].size() >= 1, "There are no source hosts for rtt: " << rtt);
      /* Get one of the possible hosts suitable for this RTT */
      Ptr<Node> src = RandomFromVector<Ptr<Node>>(senders_latency_to_node[rtt], random_variable);

      //Destination port
      uint16_t dport = random_variable->GetInteger(start_port, end_port);

      startTime = start_time + flow.start_time;

      /* WARNING: The flows loader handles this */
      // removes /24
      //std::string dst_ip = flow.prefix.substr(0, flow.prefix.find("/"));
      //replaces last 0 with a 1
      //dst_ip = dst_ip.substr(0, dst_ip.length() - 1) + "1";
      //std::cout << int(flow.protocol) << std::endl;
      std::string protocol = "";
      if (flow.protocol == 6)
      {
        protocol = "TCP";
      }
      else
      {
        protocol = "UDP";
      }

      if (num_flows_started < 100)
      {
        NS_LOG_UNCOND(
          "Flow Features:    " << "\tSrc: " << GetNodeName(src)
          << "(" << GetNodeIp(src) << ")"
          << "\tDst: " << flow.prefix
          << "\tReal_rtt: " << flow.rtt
          << "\tRtt Found: " << rtt
          << "\tBytes: " << flow.bytes
          << "\tPackets: " << flow.packets
          << "\tStart time: " << startTime
          << "\tProtocol " << protocol
          << "\tDuration: " << flow.duration);
      }

      InstallRateSend(src, flow.prefix, dport, flow.packets, flow.bytes, flow.duration, rtt, startTime, protocol);

      //return prefixes_stats;

      /* update stats*/
      if (prefixes_stats.count(flow.prefix) == 0)
      {
        TrafficPrefixStats stats;
        stats.packets = flow.packets;
        stats.bytes = flow.bytes;
        stats.flows = 1;
        prefixes_stats[flow.prefix] = stats;
      }
      else
      {
        prefixes_stats[flow.prefix].packets += flow.packets;
        prefixes_stats[flow.prefix].bytes += flow.bytes;
        prefixes_stats[flow.prefix].flows += 1;
      }

      num_flows_started++;

      /* update first packet data structure */
      if (first_packet.count(flow.prefix) == 0)
      {
        first_packet[flow.prefix] = startTime;
        prefixes_stats[flow.prefix].first_packet = startTime;
      }
      /* for the first packet after our network failure, which comes in bursts */
      if ((startTime > failure_time) && (first_packet_after_failure.count(flow.prefix) == 0))
      {
        first_packet_after_failure[flow.prefix] = startTime;
        prefixes_stats[flow.prefix].first_packet_after_failure = startTime;
      }
    }

    NS_LOG_UNCOND("Number of flows Started: " << num_flows_started);
    if (output_file != "")
    {
      *(first_packet_file->GetStream()) << "#First" << "\n";
      for (auto& it : first_packet)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }

      *(first_packet_file->GetStream()) << "#After failure" << "\n";
      for (auto& it : first_packet_after_failure)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }
    }
    return prefixes_stats;
  }

  std::unordered_map<std::string, TrafficPrefixStats>
    HybridTrafficScheduler(std::unordered_map<double, std::vector<Ptr<Node>>> senders_latency_to_node,
      std::unordered_set<uint32_t> prefixes_to_send, std::vector<double> rtt_cdf, double rtt_offset,
      std::string flowDistFile, uint32_t seed, double start_time, double duration,
      double failure_time, uint16_t start_port, uint16_t end_port,
      std::string output_file)
  {

    std::cout << "Starting Hybrid Traffic Scheduler" << std::endl;

    /* we create a file where we store the first packet
       of each prefix we use that to count the detection time */

    std::unordered_map<std::string, double> first_packet;
    std::unordered_map<std::string, double> last_packet;
    std::unordered_map<std::string, double> first_packet_after_failure;

    /* Status to maybe use externally and for the failures or something */
    std::unordered_map<std::string, TrafficPrefixStats> prefixes_stats;

    //Log output file
    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> first_packet_file;
    if (output_file != "")
    {
      first_packet_file = asciiTraceHelper.CreateFileStream(output_file);
    }

    //Load flow distribution
    std::vector<FlowMetadata> flowDist = GetFlowsPerPrefixFromDist(flowDistFile, prefixes_to_send);

    NS_LOG_UNCOND("Number of flows to use: " << flowDist.size());

    //Usage object
    double startTime = start_time;
    double simulationTime = duration;
    uint64_t num_flows_started = 0;
    Ptr<UniformRandomVariable> random_variable = CreateObject<UniformRandomVariable>();

    for (uint32_t i = 0; i < flowDist.size(); i++)
    {
      FlowMetadata flow = flowDist[i];
      // fix rtt to adapt it to the inter-switch delay if exists
      double corrected_flow_rtt = std::max(flow.rtt - rtt_offset, 0.0);
      double rtt = FindClosest(rtt_cdf, corrected_flow_rtt);
      NS_ASSERT_MSG(senders_latency_to_node[rtt].size() >= 1, "There are no source hosts for rtt: " << rtt);
      /* Get one of the possible hosts suitable for this RTT */
      Ptr<Node> src = RandomFromVector<Ptr<Node>>(senders_latency_to_node[rtt], random_variable);

      //Destination port
      uint16_t dport = random_variable->GetInteger(start_port, end_port);

      startTime = start_time + flow.start_time;

      /* WARNING: The flows loader handles this */
      // removes /24
      //std::string dst_ip = flow.prefix.substr(0, flow.prefix.find("/"));
      //replaces last 0 with a 1
      //dst_ip = dst_ip.substr(0, dst_ip.length() - 1) + "1";
      //std::cout << int(flow.protocol) << std::endl;
      std::string protocol = "";
      if (flow.protocol == 6)
      {
        protocol = "TCP";
      }
      else
      {
        protocol = "UDP";
      }

      if (num_flows_started < 50)
      {

        NS_LOG_UNCOND(
          "Flow Features:    " << "\tSrc: " << GetNodeName(src)
          << "(" << GetNodeIp(src) << ")"
          << "\tDst: " << flow.prefix
          << "\tReal_Flow_Rtt " << flow.rtt
          << "\tLink_Rtt " << corrected_flow_rtt
          << "\tRtt_Found: " << rtt
          << "\tBytes: " << flow.bytes
          << "\tPackets: " << flow.packets
          << "\tStart time: " << startTime
          << "\tProtocol " << protocol
          << "\tDuration: " << flow.duration);

      }

      InstallRateSend(src, flow.prefix, dport, flow.packets, flow.bytes, flow.duration, rtt, startTime, protocol);

      //return prefixes_stats;

      /* update stats*/
      if (prefixes_stats.count(flow.prefix) == 0)
      {
        TrafficPrefixStats stats;
        stats.packets = flow.packets;
        stats.bytes = flow.bytes;
        stats.flows = 1;
        stats.last_packet = startTime + flow.duration;
        prefixes_stats[flow.prefix] = stats;

      }
      else
      {
        prefixes_stats[flow.prefix].packets += flow.packets;
        prefixes_stats[flow.prefix].bytes += flow.bytes;
        prefixes_stats[flow.prefix].flows += 1;
        prefixes_stats[flow.prefix].last_packet = std::max(prefixes_stats[flow.prefix].last_packet, startTime + flow.duration);
      }

      num_flows_started++;

      /* update first packet data structure */
      if (first_packet.count(flow.prefix) == 0)
      {
        first_packet[flow.prefix] = startTime;
        prefixes_stats[flow.prefix].first_packet = startTime;
      }
      /* for the first packet after our network failure, which comes in bursts */
      if ((startTime > failure_time) && (first_packet_after_failure.count(flow.prefix) == 0))
      {
        first_packet_after_failure[flow.prefix] = startTime;
        prefixes_stats[flow.prefix].first_packet_after_failure = startTime;
      }
    }

    NS_LOG_UNCOND("Number of flows Started: " << num_flows_started);
    /* Only if there is output file, in this case we do not use it */
    if (output_file != "")
    {
      *(first_packet_file->GetStream()) << "#First" << "\n";
      for (auto& it : first_packet)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }

      *(first_packet_file->GetStream()) << "#After failure" << "\n";
      for (auto& it : first_packet_after_failure)
      {
        *(first_packet_file->GetStream()) << it.first << " " << it.second << "\n";
      }
    }
    return prefixes_stats;
  }


}
