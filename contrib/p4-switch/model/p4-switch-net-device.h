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
 * Author: Edgar Costa <cedgar@ethz.ch>
 */
#ifndef P4_SWITCH_NET_DEVICE_H
#define P4_SWITCH_NET_DEVICE_H

#include "ns3/net-device.h"
#include "ns3/mac48-address.h"
#include "ns3/nstime.h"
#include "ns3/p4-switch-channel.h"
#include "ns3/p4-switch-utils.h"
#include "ns3/event-id.h"
#include <stdint.h>
#include <string>
#include <map>
#include <vector>
#include <unordered_map>
#include <any>
#include <algorithm>
#include <unordered_set>

namespace ns3 {

  class Node;

  struct PortInfo
  {
    Ptr<NetDevice> portDevice;
    Ptr<NetDevice> otherPortDevice;
    bool switchPort = false;
  };

  /**
   * \defgroup switch Switch Network Device
   *
   * \brief a virtual net device that switchs multiple LAN segments
   *
   * The P4SwitchNetDevice object is a "virtual" netdevice that aggregates
   * multiple "real" netdevices and implements the data plane forwarding
   * part of IEEE 802.1D.  By adding a P4SwitchNetDevice to a Node, it
   * will act as a "switch", or "switch", to multiple LAN segments.
   *
   * By default the switch netdevice implements a "learning switch"
   * algorithm (see 802.1D), where incoming unicast frames from one port
   * may occasionally be forwarded throughout all other ports, but
   * usually they are forwarded only to a single correct output port.
   *
   * \attention The Spanning Tree Protocol part of 802.1D is not
   * implemented.  Therefore, you have to be careful not to create
   * bridging loops, or else the network will collapse.
   *
   * \attention Bridging is designed to work only with NetDevices
   * modelling IEEE 802-style technologies, such as CsmaNetDevice and
   * WifiNetDevice.
   *
   * \attention If including a WifiNetDevice in a switch, the wifi
   * device must be in Access Point mode.  Adhoc mode is not supported
   * with bridging.
   */

   /**
    * \ingroup switch
    * \brief a virtual net device that switchs multiple LAN segments
    */

  class P4SwitchNetDevice : public NetDevice
  {
  public:

    /* Data types */
    enum ForwardingType
    {
      PORT_FORWARDING,
      L3_SPECIAL_FORWARDING,
      L2_FORWARDING,
    };

    struct pkt_info
    {
      // Meta fields
      Ptr<NetDevice> inPort;
      Ptr<NetDevice> outPort = NULL;
      PacketType packetType = PACKET_OTHERHOST; // Host bla bla
      uint16_t instanceType = 0; // Clone type

      /* flow info this can be used to not have to get the flow info all the time*/
      ip_five_tuple flow;
      /* flow in string form, mainly used as key for maps */
      std::string str_flow = "";

      /* this packet has been tagged as a gray drop candidate
       * Being true does not mean this packet will be dropped,
       * other conditions like probabilities might play a role here
      * */
      bool gray_drop = false;

      /* These fields should not be copied to packet replicas */

      /* Flag that tells the TM to drop the packet and replicas */
      bool drop_flag = false;
      /* Indicates if the packet needs to be cloned */
      uint16_t cloneType = 0;
      /* Port where to clone the packet */
      Ptr<NetDevice> cloneOutPort = NULL;
      /* Indicates if a packet has to be broadcasted to
       * all ports but the incoming one
       */
      bool broadcast_flag = false;

      // The packet has been generated inside the switch now
      bool internalPacket = false;

      /* State machine id */
      uint32_t id = 0;

      // Ethernet Info 
      Address src;
      Address dst;
      uint16_t protocol;

      // Headers
      std::unordered_map<std::string, std::any> headers;
      pkt_info(Address const& a1, Address const& a2, uint16_t a3) : src(a1), dst(a2), protocol(a3) {}
    };

    enum CloneType {
      I2E,
      E2E
    };

    /**
     * \brief Get the type ID.
     * \return the object TypeId
     */
    static TypeId GetTypeId(void);
    P4SwitchNetDevice();
    virtual ~P4SwitchNetDevice();

    /**
     * \brief Add a 'port' to a switch device
     * \param switchPort the NetDevice to add
     *
     * This method adds a new switch port to a P4SwitchNetDevice, so that
     * the new switch port NetDevice becomes part of the switch and L2
     * frames start being forwarded to/from this NetDevice.
     *
     * \param switchPort NetDevice
     * \attention The netdevice that is being added as switch port must
     * _not_ have an IP address.  In order to add IP connectivity to a
     * bridging node you must enable IP on the P4SwitchNetDevice itself,
     * never on its port netdevices.
     */
    virtual void AddSwitchPort(Ptr<NetDevice> switchPort);

    /**
     * \brief Gets the number of switchd 'ports', i.e., the NetDevices currently switchd.
     *
     * \return the number of switchd ports.
     */
    uint32_t GetNSwitchPorts(void) const;

    /**
     * \brief Gets the n-th switchd port.
     * \param n the port index
     * \return the n-th switchd NetDevice
     */
    Ptr<NetDevice> GetSwitchPort(uint32_t n) const;

    virtual void Init(void);

    // inherited from NetDevice base class.
    virtual void SetIfIndex(const uint32_t index);
    virtual uint32_t GetIfIndex(void) const;
    virtual Ptr<Channel> GetChannel(void) const;
    virtual void SetAddress(Address address);
    virtual Address GetAddress(void) const;
    virtual bool SetMtu(const uint16_t mtu);
    virtual uint16_t GetMtu(void) const;
    virtual bool IsLinkUp(void) const;
    virtual void AddLinkChangeCallback(Callback<void> callback);
    virtual bool IsBroadcast(void) const;
    virtual Address GetBroadcast(void) const;
    virtual bool IsMulticast(void) const;
    virtual Address GetMulticast(Ipv4Address multicastGroup) const;
    virtual bool IsPointToPoint(void) const;
    virtual bool IsBridge(void) const;
    virtual bool Send(Ptr<Packet> packet, const Address& dest, uint16_t protocolNumber);
    virtual bool SendFrom(Ptr<Packet> packet, const Address& source, const Address& dest, uint16_t protocolNumber);
    virtual Ptr<Node> GetNode(void) const;
    virtual void SetNode(Ptr<Node> node);
    virtual bool NeedsArp(void) const;
    virtual void SetReceiveCallback(NetDevice::ReceiveCallback cb);
    virtual void SetPromiscReceiveCallback(NetDevice::PromiscReceiveCallback cb);
    virtual bool SupportsSendFrom() const;
    virtual Address GetMulticast(Ipv6Address addr) const;
    void SetSwitchId(uint8_t id);
    uint8_t GetSwitchId(void) const;
    void SetDebug(bool state);
    bool m_enableDebug;
    bool m_enableDebugGlobal;

    void EnableOutBandwidthPrint(Ptr<NetDevice> port);
    void EnableInBandwidthPrint(Ptr<NetDevice> port);
    void SetBandwidthPrintInterval(double delay);
    void PrintBandwidth(double delay, uint32_t intf, uint64_t prev_counter, bool out);
    void StartBandwidthMeasurament(void);
    void FillTables(std::string conf_path = "");
    void L3SpecialForwardingSetFailures(std::vector<std::pair<uint32_t, Ptr<NetDevice>>> prefixes);
    void L3SpecialForwardingRemoveFailures(std::vector<std::pair<uint32_t, Ptr<NetDevice>>> prefixes);
    void SaveDrops(std::string outFile);

  protected:
    virtual void DoDispose(void);

    /**
     * \brief Receives a packet from one switchd port.
     * \param device the originating port
     * \param packet the received packet
     * \param protocol the packet protocol (e.g., Ethertype)
     * \param source the packet source
     * \param destination the packet destination
     * \param packetType the packet type (e.g., host, broadcast, etc.)
     */
    void ReceiveFromDevice(Ptr<NetDevice> device, Ptr<const Packet> packet, uint16_t protocol,
      Address const& source, Address const& destination, PacketType packetType);

    /**
     * \brief Runs the L2 (mac) learning algorithm at the switch
     * \param device the originating port
     * \param packet the received packet
     * \param protocol the packet protocol (e.g., Ethertype)
     * \param source the packet source
     * \param destination the packet destination
     * \param packetType the packet type (e.g., host, broadcast, etc.)
     */
    void L2Learning(Ptr<const Packet> packet, pkt_info& meta);

    /* Added functions once finished we need to comment them better */

    // Main Pipeline
    void VerifyChecksums(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoVerifyChecksums(Ptr<const Packet> packet, pkt_info& meta);
    void Parser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol);
    virtual void DoParser(Ptr<Packet> packet, pkt_info& meta, uint16_t protocol);
    void Ingress(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoIngress(Ptr<const Packet> packet, pkt_info& meta);
    void TrafficManager(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoTrafficManager(Ptr<const Packet> packet, pkt_info& meta);
    void Egress(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoEgress(Ptr<const Packet> packet, pkt_info& meta);
    void UpdateChecksums(Ptr<const Packet> packet, pkt_info& meta);
    virtual void DoUpdateChecksums(Ptr<const Packet> packet, pkt_info& meta);
    void Deparser(Ptr<Packet> packet, pkt_info& meta);
    virtual void DoDeparser(Ptr<Packet> packet, pkt_info& meta);
    void SendOutFrom(Ptr<Packet> packet, Ptr<NetDevice> outPort, const Address& src,
      const Address& dst, uint16_t protocolNumber);
    void EgressTrafficManager(Ptr<Packet> packet, pkt_info& meta);
    virtual void DoEgressTrafficManager(Ptr<Packet> packet, pkt_info& meta);

    // Primitives
    void Clone(Ptr<const Packet> packet, Ptr<NetDevice> outPort, CloneType cloneType, pkt_info& meta);
    pkt_info CopyMeta(pkt_info& meta);

    ip_five_tuple GetFlowFiveTuple(pkt_info& meta);

    /*
    L2 Forwarding
    */
    void L2ForwardingFillTable(std::string table_path);
    void L2Forwarding(pkt_info& meta);

    /* Port Forwarding */
    void PortForwardingFillTable(std::string table_path);
    void PortForwarding(pkt_info& meta);

    void L3SpecialForwardingFillTable();
    void L3SpecialForwarding(pkt_info& meta);

    /* End of added */
    /**
     * \brief Forwards a unicast packet
     * \param incomingPort the packet incoming port
     * \param packet the packet
     * \param protocol the packet protocol (e.g., Ethertype)
     * \param src the packet source
     * \param dst the packet destination
     */
    void ForwardUnicast(Ptr<NetDevice> incomingPort, Ptr<const Packet> packet,
      uint16_t protocol, Mac48Address src, Mac48Address dst, pkt_info& meta);

    /**
     * \brief Forwards a broadcast or a multicast packet
     * \param incomingPort the packet incoming port
     * \param packet the packet
     * \param protocol the packet protocol (e.g., Ethertype)
     * \param src the packet source
     * \param dst the packet destination
     */
    void ForwardBroadcast(Ptr<NetDevice> incomingPort, Mac48Address src, pkt_info& meta);

    /**
     * \brief Broadcasts a packet to all the ports but the incoming one
     * \param incomingPort the packet incoming port
     * \param packet the packet
     * \param protocol the packet protocol (e.g., Ethertype)
     * \param src the packet source
     * \param dst the packet destination
     */
    void Broadcast(pkt_info& meta);

    /**
     * \brief Learns the port a MAC address is sending from
     * \param source source address
     * \param port the port the source is sending from
     */
    void Learn(Mac48Address source, Ptr<NetDevice> port);

    /**
     * \brief Gets the port associated to a source address
     * \param source the source address
     * \returns the port the source is associated to, or NULL if no association is known.
     */
    Ptr<NetDevice> GetLearnedState(Mac48Address source);


    uint8_t m_switchId;
    std::unordered_map<uint32_t, PortInfo> m_portsInfo;
    Mac48Address m_address; //!< MAC address of the NetDevice
    Ptr<Node> m_node; //!< node owning this NetDevice
    std::string m_name;
    Ptr<P4SwitchChannel> m_channel; //!< virtual switchd channel
    std::vector< Ptr<NetDevice> > m_ports; //!< switchd ports
    ForwardingType m_forwardingType = ForwardingType::PORT_FORWARDING;

    // drop states
    std::vector<ns3::Time> m_drop_times;

  private:
    /**
     * \brief Copy constructor
     *
     * Defined and unimplemented to avoid misuse
     */
    P4SwitchNetDevice(const P4SwitchNetDevice&);

    /**
     * \brief Copy constructor
     *
     * Defined and unimplemented to avoid misuse
     * \returns
     */
    P4SwitchNetDevice& operator = (const P4SwitchNetDevice&);

    NetDevice::ReceiveCallback m_rxCallback; //!< receive callback
    NetDevice::PromiscReceiveCallback m_promiscRxCallback; //!< promiscuous receive callback

    uint32_t m_ifIndex; //!< Interface index
    uint16_t m_mtu; //!< MTU of the switchd NetDevice

    /*
    L2 Learning state and attributes
    */
    Time m_expirationTime;  //!< time it takes for learned MAC state to expire
    /**
     * \ingroup switch
     * Structure holding the status of an address
     */
    struct LearnedState
    {
      Ptr<NetDevice> associatedPort; //!< port associated with the address
      Time expirationTime;  //!< time it takes for learned MAC state to expire
    };
    std::map<Mac48Address, LearnedState> m_learnState; //!< Container for known address statuses
    bool m_enableLearning; //!< true if the switch will learn the node status

    /*
    L2 Forwarding & Port Forwarding
    */
    std::map<Mac48Address, Ptr<NetDevice>> m_L2Table;
    std::map<uint32_t, Ptr<NetDevice>> m_PortTable;

    //std::unordered_map<uint32_t, Ptr<NetDevice>> m_L3Table;
    /* contains a pair of dst to -> net device and fail status of a prefix */
    std::unordered_map<uint32_t, std::pair<Ptr<NetDevice>, bool>> m_L3Table;

    Ptr<NetDevice> m_L3Gateway;

    bool m_enableL2Forwarding;

    /* Our Own custom stats collector */
    std::unordered_map<uint32_t, uint64_t> m_monitorOutBw;
    std::unordered_map<uint32_t, uint64_t> m_monitorInBw;
    std::vector<uint32_t> m_enableMonitorOutPorts;
    std::vector<uint32_t> m_enableMonitorInPorts;
    double m_bwPrintFrequency = 0.01;

    uint32_t m_input_count = 0;
    Time m_time_difference;
    clock_t m_real_time_difference = 0;

  };

} // namespace ns3

#endif /* SWITCH_NET_DEVICE_H */
