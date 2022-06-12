/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef RAW_SEND_APPLICATION_H
#define RAW_SEND_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/data-rate.h"

#include <algorithm>
#include <random>

namespace ns3 {

class Address;
class Socket;

/**
 * \ingroup applications
 * \defgroup rawsend RawSendApplication
 * 
**/

class RawSendApplication: public Application {
public:
	/**
	 * \brief Get the type ID.
	 * \return the object TypeId
	 */
	static TypeId GetTypeId(void);

	RawSendApplication();

	virtual ~RawSendApplication();

	
protected:
	virtual void DoDispose(void);
private:
	// inherited from Application base class.
	virtual void StartApplication(void);    // Called at time specified by Start
	virtual void StopApplication(void);     // Called at time specified by Stop

  Ptr<Packet> CreateTcpSynPacket(uint32_t dstIp, uint16_t srcPort, uint16_t dstPort, uint8_t tos);
  void AllocatePackets ();
  void CleanPackets ();
  void RandomInitializePackets (bool randomize_dst_ip);
  void SendPackets(void);
  void SaveFlows ();

	TypeId m_tid;         

  uint32_t m_packetSize;
  DataRate m_sendRate;
  uint32_t m_nPackets;
  double m_duration;
  uint32_t m_nFlows;
  uint32_t m_packetIndex;
  Address m_dstAddr;
  uint32_t m_dropFlows;

  //std::vector<Ptr<Packet>> m_packets;
  Ptr<Packet> * m_packets = NULL;
  Ptr<NetDevice> m_device;
  EventId m_sendEvent;
  Time m_nextTx;
  std::string m_outFile;

  
};

} // namespace ns3

#endif /* RAW_SEND_APPLICATION_H */
