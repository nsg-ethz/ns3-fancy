
#include "bloom-filter-test.h"

namespace ns3{

void bloom_filter_test(uint32_t filter_size, uint32_t hash_functions, uint32_t elements, uint32_t n_tests)
{

  std::vector<uint8_t> bloom_filter;
  /* Initialize the BF */
  for (int i = 0; i < filter_size; i++)
  {
    bloom_filter.push_back(0);
  }

  HashUtils hash = HashUtils(hash_functions);

  Ptr<UniformRandomVariable> random_generator = CreateObject<UniformRandomVariable>();

  /* Insert Elements */
  for (int i = 0; i < elements; i++)
  {
    uint32_t random_value = random_generator->GetInteger(0, ((uint32_t)-1));
    char s[4];
    char * ptr = s;
    std::memcpy(ptr, &random_value, 4);
    for (int k= 0; k < hash_functions; k++)
    {
      uint32_t hash_index = hash.GetHash(s, 4, k, filter_size);
      bloom_filter[hash_index] = 1;
    }
  }

  /* Check error rate */
  uint32_t collisions = 0;

  for (int i = 0; i < n_tests; i++)
  {
    uint32_t random_value = random_generator->GetInteger(0, ((uint32_t)-1));
    char s[4];
    char * ptr = s;
    std::memcpy(ptr, &random_value, 4);
    bool test = true;
    for (int k= 0; k < hash_functions; k++)
    {
      uint32_t hash_index = hash.GetHash(s, 4, k, filter_size);
      test = test & bloom_filter[hash_index];
    }

    if (test)
    {
      collisions++;
    }

  }

  std::cout << "Error rate: 1 out of " << 1/(double(collisions)/double(n_tests)) << std::endl;

}

}