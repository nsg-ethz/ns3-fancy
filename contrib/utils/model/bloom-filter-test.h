
#include "ns3/hash-utils.h"
#include <stdint.h>
#include <vector>
#include "ns3/random-variable-stream.h"
#include <cstring>

namespace ns3{

void bloom_filter_test(uint32_t filter_size, uint32_t hash_functions, uint32_t elements, uint32_t n_tests);

}