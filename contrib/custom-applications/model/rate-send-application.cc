/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2010 Georgia Institute of Technology
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
 * Author: George F. Riley <riley@ece.gatech.edu>
 */

#include "ns3/log.h"
#include "ns3/address.h"
#include "ns3/node.h"
#include "ns3/nstime.h"
#include "ns3/socket.h"
#include "ns3/simulator.h"
#include "ns3/socket-factory.h"
#include "ns3/packet.h"
#include "ns3/uinteger.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/tcp-socket-factory.h"
#include "rate-send-application.h"
#include "ns3/utils-module.h"

#define TCP_HEADER_SIZE = 54;

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("rate-send-app");

NS_OBJECT_ENSURE_REGISTERED (RateSendApplication);

//char *TCPCongestionStateTypes[] =
//    {
//        "CA_OPEN",      /**< Normal state, no dubious events */
//        "CA_DISORDER",  /**< In all the respects it is "Open",
//                    *  but requires a bit more attention. It is entered when
//                    *  we see some SACKs or dupacks. It is split of "Open" */
//        "CA_CWR",       /**< cWnd was reduced due to some Congestion Notification event.
//                    *  It can be ECN, ICMP source quench, local device congestion.
//                    *  Not used in NS-3 right now. */
//        "CA_RECOVERY",  /**< CWND was reduced, we are fast-retransmitting. */
//        "CA_LOSS",      /**< CWND was reduced due to RTO timeout or SACK reneging. */
//        "CA_LAST_STATE" /**< Used only in debug messages */
//    };
//
//void TimeTracer(std::string name, Time oldValue, Time newValue)
//{
//  NS_LOG_UNCOND(name << " " << Simulator::Now().GetSeconds() << " " << newValue.GetSeconds());
//}
//
//void TCPStateTracer(const TcpSocket::TcpStates_t oldValue, const TcpSocket::TcpStates_t newValue)
//{
//  NS_LOG_UNCOND("TCP STATE: " << Simulator::Now().GetSeconds() << " " << newValue);
//}
//
//void TCPCongStateTracer(const TcpSocketState::TcpCongState_t oldValue, const TcpSocketState::TcpCongState_t newValue)
//{
//  NS_LOG_UNCOND("TCP CONGESTION STATE: " << Simulator::Now().GetSeconds() << " " << TCPCongestionStateTypes[newValue]);
//}
//
//void
//TCPCwndTracer(uint32_t oldCwnd, uint32_t newCwnd)
//{
//  NS_LOG_UNCOND ("TCP CONGESTION WINDOW: " << Simulator::Now().GetSeconds() << " " << newCwnd);
//}



TypeId
RateSendApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::RateSendApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications") 
    .AddConstructor<RateSendApplication> ()
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (2048),
                   MakeUintegerAccessor (&RateSendApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&RateSendApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&RateSendApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t> ())
	  .AddAttribute ("BytesPerInterval",
									 "Bytes the app is allowed to send every interval",
									 UintegerValue (0),
									 MakeUintegerAccessor (&RateSendApplication::m_bytesPerInterval),
									 MakeUintegerChecker<uint64_t> ())
		.AddAttribute("IntervalDuration",
									"Filling the bucked periodicity",
									DoubleValue(1),
									MakeDoubleAccessor(&RateSendApplication::m_intervalDuration),
									MakeDoubleChecker<double>())
	 .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&RateSendApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&RateSendApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


RateSendApplication::RateSendApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0),
    m_bytesInBucket(0),
    m_startTime(0),
    m_sendingData(false)
{
  NS_LOG_FUNCTION (this);
}

RateSendApplication::~RateSendApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
RateSendApplication::SetMaxBytes (uint64_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
RateSendApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
RateSendApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

uint32_t RateSendApplication::GetTxBufferSize(void){
  NS_LOG_FUNCTION (this);

  return DynamicCast<TcpSocketBase>(m_socket)->GetTxBuffer()->Size();
}


void RateSendApplication::SetSocket(Ptr<Socket> s){
	NS_LOG_FUNCTION(this);
	m_socket = s;
}


// Application Methods
void RateSendApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  m_initialIntervalDuration = m_intervalDuration;

  // Create the socket if not already
  if (!m_socket)//(!m_socket)
    {
  		//  		m_started=true;
      m_socket = Socket::CreateSocket (GetNode (), m_tid);

//      m_socket->TraceConnectWithoutContext ("RTO", MakeBoundCallback (&TimeTracer, "RTO"));
//      m_socket->TraceConnectWithoutContext ("RTT", MakeBoundCallback (&TimeTracer, "RTT"));
//      m_socket->TraceConnectWithoutContext ("STATE", MakeCallback (&TCPStateTracer));
//      m_socket->TraceConnectWithoutContext ("CongState", MakeCallback (&TCPCongStateTracer));
//      m_socket->TraceConnectWithoutContext ("CongestionWindow", MakeCallback (&TCPCwndTracer));

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      //if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
      //    m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
      //  {
      //    NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
      //                    "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
      //                    "In other words, use TCP instead of UDP.");
      //  }

      if (Inet6SocketAddress::IsMatchingType (m_peer))
        {
          if (m_socket->Bind6 () == -1)
            {
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }
      else if (InetSocketAddress::IsMatchingType (m_peer))
        {
          if (m_socket->Bind () == -1)
            {
          		NS_LOG_UNCOND("Hosts " << GetNodeName(GetNode()) << " failed");
              NS_FATAL_ERROR ("Failed to bind socket");
            }
        }

      //Save Starting time just before connection starts
      m_startTime  = Simulator::Now().GetSeconds();


      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();

      m_socket->SetConnectCallback (
        MakeCallback (&RateSendApplication::ConnectionSucceeded, this),
        MakeCallback (&RateSendApplication::ConnectionFailed, this));

      if (m_socket->GetSocketType () == Socket::NS3_SOCK_STREAM)
      {
        /* This is needed for sending in bulk, we actually remove to avoid
        calling this call back all the time since its a bit expensive */
        //m_socket->SetSendCallback (
        //    MakeCallback (&RateSendApplication::DataSend, this));
      }    
      /* is UDP */
      else {
        ConnectionSucceeded(m_socket);
      }
  
    }
}

void RateSendApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
  		Simulator::Cancel(m_refillEvent);
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("RateSendApplication found null socket to close in StopApplication");
    }
}


// Private helpers

void RateSendApplication::RefillBucket(void){

  NS_LOG_FUNCTION (this);

  //Adds bytes into the bucket.
  m_bytesInBucket +=  std::min(m_bytesPerInterval, (m_maxBytes - m_totBytes));

  //if we reached total bytes to send bucked is not refilled.
  if (m_maxBytes != m_totBytes){

      /* Attempt to slow down the thing, however, I realized that it does
      something weird I dn no understand */
      
      //if (GetTxBufferSize() > m_previousBufferSize)
      /* tries to slow down the thing */
      //std::cout << m_maxBytes << " " << m_totBytes << " " << GetTxBufferSize() << std::endl;
      //uint32_t buffer_size = GetTxBufferSize();
      //if (buffer_size > m_previousBufferSize)
      //{
      //  m_previousBufferSize = buffer_size;
      //  m_previousBufferIncreaseCounter++;
//
      //  if (m_previousBufferIncreaseCounter > 20)
      //    {
      //      m_intervalDuration += m_intervalDuration;
      //    }
      //}
      //else
      //{
      //  m_previousBufferIncreaseCounter = 0;
      //  m_previousBufferSize = buffer_size;
      //  m_intervalDuration = m_initialIntervalDuration;
      //  //m_previousBufferIncreaseCounter = 0;
      //}

    	m_refillEvent = Simulator::Schedule (Seconds(m_intervalDuration), &RateSendApplication::RefillBucket, this);
  }

  /* if we are not sending data we force it? This might work for the UDP also*/
  if (m_sendingData == false){
  	SendData();
  }

}

void RateSendApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);

  /* avoid we call send data twice, its some kind of semaphore? */
  if (m_sendingData == false){
  	m_sendingData = true;
  }
  else{
  	NS_LOG_DEBUG("Trying to run send data when it is already running");
  	return;
  }

  //std::cout << m_maxBytes << " " << m_totBytes << " " << GetTxBufferSize() << std::endl;
  //std::cout << m_maxBytes << " " << m_totBytes << " " << std::endl;
  
  while (m_bytesInBucket > 0 and m_maxBytes != m_totBytes)
    { // Time to send more

      // uint64_t to allow the comparison later.
      // the result is in a uint32_t range anyway, because
      // m_sendSize is uint32_t.
      uint64_t toSend = m_sendSize;
      // Make sure we don't send too many
      toSend = std::min (toSend, m_bytesInBucket);

      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      int actual = m_socket->Send (packet);
      
      if (actual > 0)
        {
          m_totBytes += (actual);
          m_bytesInBucket -= actual;
          m_txTrace (packet);
        }

      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed up.
      if ((unsigned)actual != toSend)
        {
      		m_sendingData = false;
          break;
        }
   }

	// Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && m_connected) //&& (GetTxBufferSize() == 0))
    {
  		StopApplication();
    }

	m_sendingData = false;

}


void RateSendApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("RateSendApplication Connection succeeded");
  m_connected = true;
  RefillBucket();
  SendData ();
}

void RateSendApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("RateSendApplication, Connection Failed");
}

void RateSendApplication::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    { // Only send new data if the connection has completed
      SendData ();
    }
}

void RateSendApplication::SocketNormalClose (Ptr<Socket>)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_DEBUG("Connection closed normally");
}

void RateSendApplication::SocketErrorClose (Ptr<Socket>)
{
  NS_LOG_FUNCTION (this);
  NS_LOG_ERROR("Connection closed by an error");
}
} // Namespace ns3
