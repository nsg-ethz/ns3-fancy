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
#ifndef P4_SWITCH_CHANNEL_H
#define P4_SWITCH_CHANNEL_H

#include "ns3/net-device.h"
#include "ns3/channel.h"
#include <vector>

namespace ns3 {

/**
 * \ingroup switch
 * 
 * \brief Virtual channel implementation for switchs (SwitchNetDevice).
 *
 * Just like SwitchNetDevice aggregates multiple NetDevices,
 * P4SwitchChannel aggregates multiple channels and make them appear as
 * a single channel to upper layers.
 */
class P4SwitchChannel : public Channel
{
public:
  /**
   * \brief Get the type ID.
   * \return the object TypeId
   */
  static TypeId GetTypeId (void);
  P4SwitchChannel ();
  virtual ~P4SwitchChannel ();

  /**
   * Adds a channel to the switchd pool
   * \param switchdChannel  the channel to add to the pool
   */
  void AddChannel (Ptr<Channel> switchdChannel);

  // virtual methods implementation, from Channel
  virtual std::size_t GetNDevices (void) const;
  virtual Ptr<NetDevice> GetDevice (std::size_t i) const;

private:

  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   */
  P4SwitchChannel (const P4SwitchChannel &);

  /**
   * \brief Copy constructor
   *
   * Defined and unimplemented to avoid misuse
   * \returns
   */
  P4SwitchChannel &operator = (const P4SwitchChannel &);

  std::vector< Ptr<Channel> > m_switchdChannels; //!< pool of switchd channels

};

} // namespace ns3

#endif /* P4_SWITCH_CHANNEL_H */
