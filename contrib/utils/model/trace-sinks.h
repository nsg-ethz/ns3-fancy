/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
//
// Created by edgar costa molero on 01.05.18.
//

#ifndef TRACE_SINKS_H
#define TRACE_SINKS_H

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"

namespace ns3 {

void
CwndChangeStream(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd);

void
CwndChange(std::string header, uint32_t oldCwnd, uint32_t newCwnd);

void
TracePcap(Ptr<PcapFileWrapper> file, Ptr<const Packet> packet);

void
RxDropAscii(Ptr<OutputStreamWrapper> file, Ptr<const Packet> packet);

void
TxDrop(std::string string, Ptr<const Packet> p);

void
PrintPacket(Ptr<const Packet> p);
}

#endif //TRACE_SINKS_H
