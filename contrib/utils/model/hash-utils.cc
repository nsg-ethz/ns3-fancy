/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

#include "hash-utils.h"
#include "ns3/random-variable-stream.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("hash-utils");


HashUtils::HashUtils(void)
  :
  m_hashes(NULL)
{
  ;
}

HashUtils::HashUtils(uint32_t num)
  :
  m_hashes(NULL)
{
  SetHashes(num);
}

HashUtils::~HashUtils(void)
{
  Clean();
}

HashUtils::HashUtils(const HashUtils &x)
{
  m_hashes = x.m_hashes;
}

void 
HashUtils::SetHashes(uint32_t num)
{
  Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable>();
  random_generator->SetStream(5);
  AllocateHashes(num);
  for (int i=0; i < num; i++)
  {
    uint32_t seed = random_generator->GetInteger(0, ((uint32_t)-1));
    Ptr<Hash::Implementation> hp = Create<Hash::Function::Murmur3>(seed);
    Hasher * hash =  new Hasher(hp);
    *(m_hashes+i) = hash;
  }
}

void 
HashUtils::SetHashes(void)
{
  uint32_t hash_seeds_size = sizeof(HASH_SEEDS)/sizeof(HASH_SEEDS[0]);
  AllocateHashes(hash_seeds_size);
  for (int i=0; i < hash_seeds_size; i++)
  {
    Ptr<Hash::Implementation> hp = Create<Hash::Function::Murmur3>(HASH_SEEDS[i]);
    Hasher * hash = new Hasher(hp);
    *(m_hashes+i) = hash;
  }
}

uint32_t
HashUtils::GetHash(char data[], std::size_t size, int hash_index, uint32_t modulo)
{
  (*(m_hashes + hash_index))->clear();
  uint32_t hash = (*(m_hashes + hash_index))->GetHash32(data, size) % modulo;
  return hash;
}

uint32_t
HashUtils::GetHash(std::string data , int hash_index, uint32_t modulo)
{
  (*(m_hashes + hash_index))->clear();
  uint32_t hash = (*(m_hashes + hash_index))->GetHash32(data) % modulo;
  return hash;
}

void 
HashUtils::AllocateHashes(uint32_t num)
{
  Clean();
  m_hashes = new Hasher*[num];
}

void 
HashUtils::Clean(void)
{
  if (m_hashes != NULL)
  {

    delete[] m_hashes;
    m_hashes = NULL;
  }
}

}

