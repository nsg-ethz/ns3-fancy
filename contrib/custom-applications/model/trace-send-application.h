/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#ifndef CAIDA_SEND_APPLICATION_H
#define CAIDA_SEND_APPLICATION_H

#include "ns3/address.h"
#include "ns3/application.h"
#include "ns3/event-id.h"
#include "ns3/ptr.h"
#include "ns3/traced-callback.h"
#include "ns3/data-rate.h"

#include <unordered_set>
#include <algorithm>
#include <random>

namespace ns3 {

class Address;
class Socket;

/**
 * \ingroup applications
 * \defgroup rawsend TraceSendApplication
 * 
**/

struct packet 
{
  uint64_t ts = 0;
  uint32_t dst = 0;
  uint16_t size = 0;
};

enum fail_types {
  TopDown,
  Random,
  All,
  Unknown
};

class TraceSendApplication: public Application {
public:
	/**
	 * \brief Get the type ID.
	 * \return the object TypeId
	 */
	static TypeId GetTypeId(void);

	TraceSendApplication();

	virtual ~TraceSendApplication();

	
protected:
	virtual void DoDispose(void);
private:
	// inherited from Application base class.
	virtual void StartApplication(void);    // Called at time specified by Start
	virtual void StopApplication(void);     // Called at time specified by Stop

  Ptr<Packet> CreateTcpSynPacket(uint16_t srcPort, uint16_t dstPort, 
                                                     uint16_t packet_size, uint32_t dst_addr, uint8_t tos);
  void CleanPackets ();
  void SendPackets(void);

  fail_types GetFailType(std::string const& str_type);

  void SaveFailedPrefixes ();
  void LoadBinaryFile ();
  void LoadTopPrefixes(std::string top_prefixes_file);
  void LoadAllowedPrefixes(std::string allowed_prefixes_file);

  int AddToFailAll(int min_limit, int max_limit);
  int AddToFailTopBottom(int min_limit, int max_limit, int number_to_add);
  int AddToFailRandom(int min_limit, int max_limit, int number_to_add);

	TypeId m_tid;         

  uint32_t m_packetSize;
  //DataRate m_sendRate;
  //uint32_t m_nPackets;
  double m_duration;
  //uint32_t m_nFlows;
  uint32_t m_packetIndex;
  Address m_dstAddr;
  
  uint32_t m_dropFlows;

  //Ptr<Packet> * m_packets = NULL;
  Ptr<NetDevice> m_device;
  EventId m_sendEvent;
  Time m_nextTx;
  std::string m_outFile;
  std::string m_topFile;
  std::string m_allowedToFailFile;

  /* New attributes */
  uint32_t m_numTopEntries;
  uint32_t m_topDrops;
  uint32_t m_bottomDrops;
  std::string m_topFailType;
  std::string m_bottomFailType;

  std::vector<packet> m_binary_packets;
  std::string m_inFile;
  uint32_t m_sentPackets = 0;
  std::string m_scaling;
  double m_parsed_scaling;
  std::unordered_set<uint32_t> prefixes_to_fail;
  std::unordered_set<uint32_t> prefixes_in_trace;
  std::vector<uint32_t> top_prefixes;
  std::vector<uint32_t> allowed_prefixes;
  std::vector<uint32_t> failable_prefixes;
  
};

} // namespace ns3

#endif /* CAIDA_SEND_APPLICATION_H */
