/*
 * Author: Edgar costa <cedgar@ethz.ch>
 * 
 */

#include "net-seer-header.h"
#include "ns3/log.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("NetSeerHeader");
NS_OBJECT_ENSURE_REGISTERED (NetSeerHeader);

NetSeerHeader::NetSeerHeader ()
  : m_action (0),
    m_seq1 (0),
    m_seq2 (0),
    m_nextHeader (0)
{

}

NetSeerHeader::~NetSeerHeader ()
{

}


void NetSeerHeader::SetAction (uint8_t action)
{
  m_action = action;
}

void NetSeerHeader::ResetActionField(void)
{
  m_action = 0;
}

void NetSeerHeader::SetSeq1 (uint32_t seq)
{
  m_seq1 = seq;
}

void NetSeerHeader::SetSeq2 (uint32_t seq)
{
  m_seq2 = seq;
}

void NetSeerHeader::SetNextHeader (uint16_t nextHeader)
{
  m_nextHeader = nextHeader;
}

uint8_t NetSeerHeader::GetAction (void) const
{
  return m_action;
}

uint32_t NetSeerHeader::GetSeq1 (void) const
{
  return m_seq1;
}

uint32_t NetSeerHeader::GetSeq2 (void) const
{
  return m_seq2;
}

uint16_t NetSeerHeader::GetNextHeader (void) const
{
  return m_nextHeader;
}

TypeId NetSeerHeader::GetTypeId (void)
{

  static TypeId tid = TypeId("ns3::NetSeerHeader")
    .SetParent<Header> ()
    .SetGroupName("Switch")
    .AddConstructor<NetSeerHeader> ()
  ;
  return tid;

}
TypeId NetSeerHeader::GetInstanceTypeId (void) const
{
  return GetTypeId();
}

void NetSeerHeader::Print (std::ostream &os) const
{
  os << " action: " << m_action << " seq1: " << m_seq1 << " seq2: " << m_seq2;
}
uint32_t NetSeerHeader::GetSerializedSize (void) const
{
  return 11;
}
void NetSeerHeader::Serialize (Buffer::Iterator start) const 
{
    start.WriteU8 (m_action);
    start.WriteHtonU32(m_seq1);
    start.WriteHtonU32(m_seq2);
    start.WriteHtonU16 (m_nextHeader);
}

uint32_t NetSeerHeader::Deserialize (Buffer::Iterator start)
{

  m_action = start.ReadU8 ();
  m_seq1 = start.ReadNtohU32 ();
  m_seq2 = start.ReadNtohU32 ();
  m_nextHeader = start.ReadNtohU16 ();

  return GetSerializedSize();
}

} // namespace ns3

