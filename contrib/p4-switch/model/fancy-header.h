/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Author: Edgar costa <cedgar@ethz.ch>
 * 
 */

#ifndef FANCY_HEADER_H
#define FANCY_HEADER_H

#include <stdint.h>
#include "ns3/header.h"
#include "ns3/buffer.h"

namespace ns3 {
/**
 * \ingroup fancy
 * \brief Packet header for FANCY Packets
 *
 *
 *
 *
 */
class FancyHeader : public Header 
{
public:

  /**
   * \brief Constructor
   *
   * Creates a null header
   */
  FancyHeader ();
  ~FancyHeader ();

  //void SetSrcId (uint8_t type);
  //void SetDstId (uint8_t type);
  void SetId(uint32_t id);
  void SetAction (uint8_t action);
  void SetCountFlag (bool flag);
  void SetAckFlag (bool flag);
  void SetFSMFlag (bool flag);
  void SetSeq (uint16_t seq);
  void SetCounter (uint64_t counter);
  void SetNextHeader (uint16_t nextHeader);
  /* Nodes in the tree*/
  void SetDataSize(uint16_t dataSize);
  /* Counters per node*/
  void SetDataWidth(uint8_t dataWidth);
  uint8_t IncrementDataSize(void);
  void SetDataCounter(uint32_t * counters, uint16_t index);
  void SetDataCounter(std::vector<uint32_t> &counters, uint16_t index);
  void SetDataMaximums(uint8_t * maximums, uint16_t index);
  void SetDataMaximums(std::vector<uint8_t> &maximums,  uint16_t index);
  void ResetActionField(void);
  //uint8_t GetSrcId (void) const;
  //uint8_t GetDstId (void) const;
  uint32_t GetId (void) const;
  uint8_t GetAction (void) const;
  bool GetCountFlag (void) const;
  bool GetAckFlag (void) const;
  bool GetFSMFlag (void) const;
  uint16_t GetSeq (void) const;
  uint64_t GetCounter (void) const;
  uint16_t GetNextHeader (void) const;
  uint16_t GetDataSize (void) const;
  uint8_t GetDataWidth (void) const;
  Buffer::Iterator GetData(void) const;
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

  uint32_t m_id;  // state machine ID
  uint8_t m_action;
  uint16_t m_seq;
  uint64_t m_counter;   
  uint16_t m_nextHeader;

  /* Only if we are sending extra data */
  uint16_t m_dataSize;
  uint8_t m_dataWidth;
  Buffer  m_data; 

  /* Used to compute the real size */
  uint32_t m_size;

};

} // namespace ns3

#endif /* FANCY_HEADER */
