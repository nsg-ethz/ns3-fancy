/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef FANCY_UTILS_H
#define FANCY_UTILS_H

#include <vector>
#include <string>
#include <unordered_map>
#include "ns3/core-module.h"
#include "ns3/network-module.h"

namespace ns3 {

  std::vector<double> GetRTTs(std::string rttsFile);

}

#endif /* FANCY_UTILS_H */

