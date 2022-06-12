/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef SIMPLE_SEND_H
#define SIMPLE_SEND_H

#include <string.h>
#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"

namespace ns3 {

class SimpleSend : public Application
{
public:
  SimpleSend ();
  virtual ~SimpleSend ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
  Socket::SocketType            m_socket_type;
  uint64_t        m_counter;
};

}

#endif /* SIMPLE_SEND_H */
