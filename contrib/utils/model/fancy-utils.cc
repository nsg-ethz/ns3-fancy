/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */

//#include <ns3/custom-utils.h>
#include "fancy-utils.h"
#include "utils.h"
#include "custom-utils.h"

NS_LOG_COMPONENT_DEFINE ("fancy-utils");


namespace ns3 {

std::vector<double> GetRTTs(std::string rttsFile) {
  return ReadLines<double>(rttsFile, 0);
}

}

