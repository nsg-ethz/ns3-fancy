/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Created by edgar costa molero on 01.05.18.
//

#include "trace-sinks.h"

NS_LOG_COMPONENT_DEFINE ("trace-sinks");

namespace ns3 {

    void
    CwndChangeStream(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd) {
        //  NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
        *stream->GetStream() << Simulator::Now().GetSeconds() << " " << newCwnd << std::endl;
    }

    void
    CwndChange(std::string header, uint32_t oldCwnd, uint32_t newCwnd)
    {
        NS_LOG_UNCOND (header << " " << Simulator::Now ().GetSeconds () << " " << newCwnd);
    }

    void
    TracePcap(Ptr<PcapFileWrapper> file, Ptr<const Packet> packet) {
        file->Write(Simulator::Now(), packet);
    }

    void
    RxDropAscii(Ptr<OutputStreamWrapper> file, Ptr<const Packet> packet) {
        //	Ptr<PcapFileWrapper> file OLD VERSION
        //NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());

        Ptr<Packet> p = packet->Copy();

        PppHeader ppp_header;
        p->RemoveHeader(ppp_header);
        Ipv4Header ip_header;
        p->RemoveHeader(ip_header);

        std::ostringstream oss;
        oss << Simulator::Now().GetSeconds() << " "
            << ip_header.GetSource() << " "
            << ip_header.GetDestination() << " "
            << int(ip_header.GetProtocol()) << " ";

        if (ip_header.GetProtocol() == uint8_t(17)) { //udp
            UdpHeader udpHeader;
            p->PeekHeader(udpHeader);
            oss << int(udpHeader.GetSourcePort()) << " "
                << int(udpHeader.GetDestinationPort()) << " ";

        } else if (ip_header.GetProtocol() == uint8_t(6)) {//tcp
            TcpHeader tcpHeader;
            p->PeekHeader(tcpHeader);
            oss << int(tcpHeader.GetSourcePort()) << " "
                << int(tcpHeader.GetDestinationPort()) << " ";
        }

        oss << packet->GetSize() << "\n";
        *(file->GetStream()) << oss.str();
        (file->GetStream())->flush();

        //  file->Write (Simulator::Now (), p);
    }

    void
    TxDrop(std::string s, Ptr<const Packet> p) {
        static int counter = 0;
        NS_LOG_UNCOND (counter++ << " " << s << " at " << Simulator::Now().GetSeconds());
    }

    void
    PrintPacket(Ptr<const Packet> p)
    {
        NS_LOG_UNCOND("Dropped packet " << p->ToString() << " " << Simulator::Now().GetSeconds());
    }

}