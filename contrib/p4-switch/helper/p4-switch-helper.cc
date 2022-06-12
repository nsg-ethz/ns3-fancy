/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c)
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
 */

#include "p4-switch-helper.h"
#include "ns3/log.h"
#include "ns3/p4-switch-net-device.h"
#include "ns3/node.h"
#include "ns3/names.h"
#include "ns3/global-value.h"
#include "ns3/uinteger.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("P4SwitchHelper");

P4SwitchHelper::P4SwitchHelper (std::string switchType)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.SetTypeId (switchType);
}

void 
P4SwitchHelper::SetDeviceAttribute (std::string n1, const AttributeValue &v1)
{
  NS_LOG_FUNCTION_NOARGS ();
  m_deviceFactory.Set (n1, v1);
}

NetDeviceContainer
P4SwitchHelper::Install (Ptr<Node> node, NetDeviceContainer c)
{
  NS_LOG_FUNCTION_NOARGS ();
  NS_LOG_LOGIC ("**** Install switch device on node " << node->GetId ());

  NetDeviceContainer devs;
  //Ptr<P4SwitchNetDevice> dev = m_deviceFactory.Create<P4SwitchNetDevice> ();

  //Set Device ID (unique the order in which switches are created is important)
  UintegerValue switchId;
  GlobalValue::GetValueByName("switchId", switchId);
  uint8_t id = switchId.Get();
  m_dev->SetSwitchId(id);
  switchId.Set(id+1);
  GlobalValue::Bind("switchId", switchId);

  devs.Add (m_dev);
  node->AddDevice (m_dev);

  for (NetDeviceContainer::Iterator i = c.Begin (); i != c.End (); ++i)
    {
      NS_LOG_LOGIC ("**** Add SwitchPort "<< *i);
      m_dev->AddSwitchPort (*i);
    }
  
  m_dev->Init ();

  return devs;
}

NetDeviceContainer
P4SwitchHelper::Install (std::string nodeName, NetDeviceContainer c)
{
  NS_LOG_FUNCTION_NOARGS ();
  Ptr<Node> node = Names::Find<Node> (nodeName);
  return Install (node, c);
}

} // namespace ns3
