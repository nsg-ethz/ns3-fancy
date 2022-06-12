/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 * Author: Edgar Costa Molero <cedgar@ethz.ch>
 */

#include "ns3/log.h"
#include "p4-switch-channel.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("P4SwitchChannel");

NS_OBJECT_ENSURE_REGISTERED (P4SwitchChannel);

TypeId 
P4SwitchChannel::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::P4SwitchChannel")
    .SetParent<Channel> ()
    .SetGroupName("P4-Switch")
    .AddConstructor<P4SwitchChannel> ()
  ;
  return tid;
}

P4SwitchChannel::P4SwitchChannel ()
  : Channel ()
{
  NS_LOG_FUNCTION_NOARGS ();
}

P4SwitchChannel::~P4SwitchChannel ()
{
  NS_LOG_FUNCTION_NOARGS ();

  for (std::vector< Ptr<Channel> >::iterator iter = m_switchdChannels.begin ();
       iter != m_switchdChannels.end (); iter++)
    {
      *iter = 0;
    }
  m_switchdChannels.clear ();
}


void
P4SwitchChannel::AddChannel (Ptr<Channel> switchdChannel)
{
  m_switchdChannels.push_back (switchdChannel);
}

std::size_t
P4SwitchChannel::GetNDevices (void) const
{
  uint32_t ndevices = 0;
  for (std::vector< Ptr<Channel> >::const_iterator iter = m_switchdChannels.begin ();
       iter != m_switchdChannels.end (); iter++)
    {
      ndevices += (*iter)->GetNDevices ();
    }
  return ndevices;
}


Ptr<NetDevice>
P4SwitchChannel::GetDevice (std::size_t i) const
{
  std::size_t ndevices = 0;
  for (std::vector< Ptr<Channel> >::const_iterator iter = m_switchdChannels.begin ();
       iter != m_switchdChannels.end (); iter++)
    {
      if ((i - ndevices) < (*iter)->GetNDevices ())
        {
          return (*iter)->GetDevice (i - ndevices);
        }
      ndevices += (*iter)->GetNDevices ();
    }
  return NULL;
}


} // namespace ns3
