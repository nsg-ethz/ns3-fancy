/*
 * Author: Edgar costa <cedgar@ethz.ch>
 * 
 */

#include "fancy-header.h"
#include "ns3/log.h"

namespace ns3 {

const int COUNTER_WIDTH_BYTES = 4;
const int LAYER_SPLIT_BYTES = 1;
const int NODE_INDEX_BYTES = 2;

NS_LOG_COMPONENT_DEFINE ("FancyHeader");
NS_OBJECT_ENSURE_REGISTERED (FancyHeader);

FancyHeader::FancyHeader ()
  : m_id (0),
    m_action (0),
    m_seq (0),
    m_counter (0),
    m_nextHeader (0),
    m_dataSize (0),
    m_dataWidth (0),
    m_size (0)
{

}

FancyHeader::~FancyHeader ()
{

}

//void FancyHeader::SetSrcId (uint8_t id)
//{
//  m_srcId = id;
//}
//
//void FancyHeader::SetDstId (uint8_t id)
//{
//  m_dstId = id;
//}

void FancyHeader::SetId (uint32_t id)
{
  m_id = id;
}

void FancyHeader::SetAction (uint8_t action)
{
  // Just add the last 5 bits to the action
  m_action |= (0x1f & action);
}

void FancyHeader::SetCountFlag (bool flag)
{
  if (flag)
    m_action |= (0x80);
  else
    m_action &= (0x7f);
}

void FancyHeader::SetAckFlag (bool flag)
{
  if (flag)
    m_action |= (0x40);
  else
    m_action &= (0xbf);
}

void FancyHeader::SetFSMFlag (bool flag)
{
  if (flag)
    m_action |= (0x20);
  else
    m_action &= (0xdf);
}

void FancyHeader::ResetActionField(void)
{
  m_action = 0;
}

void FancyHeader::SetSeq (uint16_t seq)
{
  m_seq = seq;
}

void FancyHeader::SetCounter (uint64_t counter)
{
  m_counter = counter;
}

void FancyHeader::SetNextHeader (uint16_t nextHeader)
{
  m_nextHeader = nextHeader;
}

void FancyHeader::SetDataSize(uint16_t dataSize)
{
  m_dataSize = dataSize;
}

void FancyHeader::SetDataWidth(uint8_t dataWidth)
{
  m_dataWidth = dataWidth;
}

uint8_t FancyHeader::IncrementDataSize(void)
{
  m_dataSize++;
  return m_dataSize;
}

void FancyHeader::SetDataCounter(uint32_t * counters, uint16_t index)
{
  /* Size is the number of counters in the counter array */
  /* I do this trick of adding at the end and then moving the
   * buffer pointer back so we can write and read the counters in 
   * the order we added them.
   */
  m_data.AddAtEnd(m_dataWidth*COUNTER_WIDTH_BYTES + NODE_INDEX_BYTES);
  Buffer::Iterator i = m_data.End ();
  i.Prev(m_dataWidth*COUNTER_WIDTH_BYTES + NODE_INDEX_BYTES);
  i.WriteU16(index);
  for (int a = 0; a < m_dataWidth; a++, ++counters)
  {
    i.WriteU32(*counters);
  }
  IncrementDataSize(); 
}

void FancyHeader::SetDataCounter(std::vector<uint32_t> &counters, uint16_t index)
{
  /* Size is the number of counters in the counter array */
  /* I do this trick of adding at the end and then moving the
   * buffer pointer back so we can write and read the counters in 
   * the order we added them.
   */
  m_data.AddAtEnd(m_dataWidth*COUNTER_WIDTH_BYTES + NODE_INDEX_BYTES);
  Buffer::Iterator i = m_data.End ();
  i.Prev(m_dataWidth*COUNTER_WIDTH_BYTES + NODE_INDEX_BYTES);
  i.WriteU16(index);
  for (int a = 0; a < m_dataWidth; a++)
  {
    i.WriteU32(counters[a]);
  }
  IncrementDataSize(); 
}

void FancyHeader::SetDataMaximums(uint8_t * maximums, uint16_t index)
{
  m_data.AddAtEnd(m_dataWidth*LAYER_SPLIT_BYTES + NODE_INDEX_BYTES);
  Buffer::Iterator i = m_data.End ();
  i.Prev(m_dataWidth*LAYER_SPLIT_BYTES + NODE_INDEX_BYTES);

  i.WriteU16(index);
  for (int a = 0; a < m_dataWidth; a++, ++maximums)
  {
    i.WriteU8(*maximums);
  }
  
  IncrementDataSize();
}

void FancyHeader::SetDataMaximums(std::vector<uint8_t> &maximums,  uint16_t index)
{
  m_data.AddAtEnd(m_dataWidth*LAYER_SPLIT_BYTES + NODE_INDEX_BYTES);
  Buffer::Iterator i = m_data.End ();
  i.Prev(m_dataWidth*LAYER_SPLIT_BYTES + NODE_INDEX_BYTES);
  i.WriteU16(index);
  for (int a = 0; a < m_dataWidth; a++)
  {
    i.WriteU8(maximums[a]);
  }
  
  IncrementDataSize();
}

//uint8_t FancyHeader::GetSrcId (void) const
//{
//  return m_srcId;
//}
//
//uint8_t FancyHeader::GetDstId (void) const
//{
//  return m_dstId;
//}

uint32_t FancyHeader::GetId (void) const
{
  return m_id;
}

bool FancyHeader::GetCountFlag (void) const
{
  return (m_action >> 7) & 0x1;
}

bool FancyHeader::GetAckFlag (void) const
{
  return (m_action >> 6) & 0x1;
}

bool FancyHeader::GetFSMFlag (void) const
{
  return (m_action >> 5) & 0x1;
}

uint8_t FancyHeader::GetAction (void) const
{
  return m_action & 0x1f;
}

uint16_t FancyHeader::GetSeq (void) const
{
  return m_seq;
}

uint64_t FancyHeader::GetCounter (void) const
{
  return m_counter;
}

uint16_t FancyHeader::GetNextHeader (void) const
{
  return m_nextHeader;
}

uint16_t FancyHeader::GetDataSize (void) const
{
  return m_dataSize;
}

uint8_t FancyHeader::GetDataWidth (void) const
{
  return m_dataWidth;
}

Buffer::Iterator FancyHeader::GetData(void) const
{
  return m_data.Begin ();
}

TypeId FancyHeader::GetTypeId (void)
{

  static TypeId tid = TypeId("ns3::FancyHeader")
    .SetParent<Header> ()
    .SetGroupName("Switch")
    .AddConstructor<FancyHeader> ()
  ;
  return tid;

}
TypeId FancyHeader::GetInstanceTypeId (void) const
{
  return GetTypeId();
}

void FancyHeader::Print (std::ostream &os) const
{
  os << "id: " << m_id << " action: " << m_action << " counter: " << m_counter;
}
uint32_t FancyHeader::GetSerializedSize (void) const
{
  
  if (m_dataSize == 0)
  {
    return 17;
  }
  else
  {
    /* Counters in the data */
    if ((m_action >> 4) & 0x1)
    {
      return 20 + m_dataSize * (COUNTER_WIDTH_BYTES*m_dataWidth+NODE_INDEX_BYTES);
    }

    /* Max indexes in the data*/
    if ((m_action >> 3) & 0x1)
    {
      return 20 + m_dataSize * (LAYER_SPLIT_BYTES*m_dataWidth+NODE_INDEX_BYTES);
    }
    
  }
  return 17;
}
void FancyHeader::Serialize (Buffer::Iterator start) const 
{
    //start.WriteU8 (m_srcId);
    //start.WriteU8 (m_dstId);
    start.WriteHtonU32(m_id);
    start.WriteU8 (m_action);
    start.WriteHtonU16 (m_seq);
    start.WriteHtonU64 (m_counter);
    start.WriteHtonU16 (m_nextHeader);

   if (((m_action >> 3) & 0x3) != 0)
    {
      if (m_dataSize > 0)
      {
        start.WriteHtonU16(m_dataSize);
        start.WriteU8(m_dataWidth);
        start.Write (m_data.Begin (), m_data.End ());      
      }
    }

}
uint32_t FancyHeader::Deserialize (Buffer::Iterator start)
{
  //m_srcId = start.ReadU8 ();
  //m_dstId = start.ReadU8 ();
  m_id = start.ReadNtohU32 ();
  m_action = start.ReadU8 ();
  m_seq = start.ReadNtohU16 ();
  m_counter = start.ReadNtohU64 ();
  m_nextHeader = start.ReadNtohU16 ();

  /* Counters in the data */
  if ((m_action >> 4) & 0x1)
  {
    m_dataSize = start.ReadNtohU16 ();
    m_dataWidth = start.ReadU8 ();
    m_size = m_dataSize * (COUNTER_WIDTH_BYTES*m_dataWidth+NODE_INDEX_BYTES);
  }

  /* Max indexes in the data*/
  if ((m_action >> 3) & 0x1)
  {
    m_dataSize = start.ReadNtohU16 ();
    m_dataWidth = start.ReadU8 ();
    m_size = m_dataSize * (LAYER_SPLIT_BYTES*m_dataWidth+NODE_INDEX_BYTES);
  }

  /* Deserialize the data */
  if (m_size > 0)
  {
    m_data = Buffer ();
    m_data.AddAtEnd (m_size);
    Buffer::Iterator dataStart = start;
    start.Next (m_size);
    Buffer::Iterator dataEnd = start;
    m_data.Begin ().Write (dataStart, dataEnd);

  }

  return GetSerializedSize();
}

} // namespace ns3

