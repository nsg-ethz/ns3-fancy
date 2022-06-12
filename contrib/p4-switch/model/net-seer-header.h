/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: Edgar costa <cedgar@ethz.ch>
 * 
 */

#ifndef NET_SEER_HEADER_H
#define NET_SEER_HEADER_H

#include <stdint.h>
#include "ns3/header.h"
#include "ns3/buffer.h"

namespace ns3 {
/**
 * \ingroup netseer
 * \brief Packet header for Net Seer Packets
 *
 *
 *
 *
 */
class NetSeerHeader : public Header 
{
public:

  /**
   * \brief Constructor
   *
   * Creates a null header
   */
  NetSeerHeader ();
  ~NetSeerHeader ();

  void SetAction (uint8_t action);
  void SetSeq1 (uint32_t seq);
  void SetSeq2 (uint32_t seq);
  void SetNextHeader (uint16_t nextHeader);
  void ResetActionField(void);

  uint8_t GetAction (void) const;
  uint32_t GetSeq1 (void) const;
  uint32_t GetSeq2 (void) const;
  uint16_t GetNextHeader (void) const;


  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  virtual TypeId GetInstanceTypeId (void) const;
  virtual void Print (std::ostream &os) const;
  virtual uint32_t GetSerializedSize (void) const;
  virtual void Serialize (Buffer::Iterator start) const;
  virtual uint32_t Deserialize (Buffer::Iterator start);

private:

  uint8_t m_action;
  uint32_t m_seq1;
  uint32_t m_seq2;
  uint16_t m_nextHeader;

};

} // namespace ns3

#endif /* NET_SEER_HEADER */
