  /* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef HASH_UTILS_H
#define HASH_UTILS_H

#include <string.h>
#include <stdint.h>

#include "ns3/log.h"
#include "ns3/hash.h"

namespace ns3 {

const uint32_t HASH_SEEDS[] = {
  0x8BADF00D, 0x7BADF01D, 0x6BADF02D, 0x5BADF03D, 0x4BADF04D, 0x3BADF05D,
  0x8BADFe0D, 0x7BADFe1D, 0x6BADFe2D, 0x5BADFe3D, 0x4BADFe4D, 0x3BADFe5D,
  0x8BEDF00D, 0x7BEDF01D, 0x6BEDF02D, 0x5BEDF03D, 0x4BEDF04D, 0x3BEDF05D,
  0x8BADFFFD, 0x7BADFFFD, 0x6BADFFFD, 0x5BADFFFD, 0x4BADFFFD, 0x3BADFFFD,
  0x8BADF00E, 0x7BADF01E, 0x6BADF02E, 0x5BADF03E, 0x4BADF04E, 0x3BADF05E,
  0x8BADFe0E, 0x7BADFe1E, 0x6BADFe2E, 0x5BADFe3E, 0x4BADFe4E, 0x3BADFe5E,
  0x8BEDF00E, 0x7BEDF01E, 0x6BEDF02E, 0x5BEDF03E, 0x4BEDF04E, 0x3BEDF05E,
  0x8BADFFFE, 0x7BADFFFE, 0x6BADFFFE, 0x5BADFFFE, 0x4BADFFFE, 0x3BADFFFE,
};

class HashUtils
{
  public:
    HashUtils ();
    HashUtils(uint32_t num);
    HashUtils(const HashUtils &x);
    ~HashUtils ();

    void SetHashes (uint32_t num);
    void SetHashes (void);
    void Clean (void);
    uint32_t GetHash(char data[], std::size_t size, int hash_index, uint32_t modulo = ((uint32_t) - 1));
    uint32_t GetHash(std::string data , int hash_index, uint32_t modulo = ((uint32_t) - 1));
    
  protected:

  private:
    Hasher ** m_hashes = NULL; 
    void AllocateHashes(uint32_t num);
};

}

#endif /* HASH_UTILS_H */

