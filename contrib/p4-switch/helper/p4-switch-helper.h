/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2008 INRIA
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Mathieu Lacage <mathieu.lacage@sophia.inria.fr>
 * Author: Gustavo Carneiro <gjc@inescporto.pt>
 */
#ifndef P4_SWITCH_HELPER_H
#define P4_SWITCH_HELPER_H

#include "ns3/net-device-container.h"
#include "ns3/object-factory.h"
#include <string>
#include "ns3/p4-switch-net-device.h"

namespace ns3 {

class Node;
class AttributeValue;

/**
 * \ingroup switch
 * \brief Add capability to switch multiple LAN segments (IEEE 802.1D bridging)
 */
class P4SwitchHelper
{
public:
  /*
   * Construct a P4SwitchHelper
   */
  P4SwitchHelper (std::string switchType);
  /**
   * Set an attribute on each ns3::SwitchNetDevice created by
   * P4SwitchHelper::Install
   *
   * \param n1 the name of the attribute to set
   * \param v1 the value of the attribute to set
   */
  void SetDeviceAttribute (std::string n1, const AttributeValue &v1);
  /**
   * This method creates an ns3::SwitchNetDevice with the attributes
   * configured by P4SwitchHelper::SetDeviceAttribute, adds the device
   * to the node, and attaches the given NetDevices as ports of the
   * switch.
   *
   * \param node The node to install the device in
   * \param c Container of NetDevices to add as switch ports
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (Ptr<Node> node, NetDeviceContainer c);
  /**
   * This method creates an ns3::SwitchNetDevice with the attributes
   * configured by P4SwitchHelper::SetDeviceAttribute, adds the device
   * to the node, and attaches the given NetDevices as ports of the
   * switch.
   *
   * \param nodeName The name of the node to install the device in
   * \param c Container of NetDevices to add as switch ports
   * \returns A container holding the added net device.
   */
  NetDeviceContainer Install (std::string nodeName, NetDeviceContainer c);

  template <typename T>
  NetDeviceContainer  Install (Ptr<Node> node, NetDeviceContainer c)
  {
    m_dev = m_deviceFactory.Create<T> ();
    return Install(node, c);
  }

private:
  ObjectFactory m_deviceFactory; //!< Object factory
  Ptr<P4SwitchNetDevice> m_dev;
};

} // namespace ns3


#endif /* P4_SWITCH_HELPER_H */
