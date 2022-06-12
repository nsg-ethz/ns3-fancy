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
#include "custom-bulk-application.h"
#include "ns3/utils-module.h"

namespace ns3 {

NS_LOG_COMPONENT_DEFINE ("custom-bulk-app");

NS_OBJECT_ENSURE_REGISTERED (CustomBulkApplication);


TypeId
CustomBulkApplication::GetTypeId (void)
{
  static TypeId tid = TypeId ("ns3::CustomBulkApplication")
    .SetParent<Application> ()
    .SetGroupName("Applications") 
    .AddConstructor<CustomBulkApplication> ()
    .AddAttribute ("SendSize", "The amount of data to send each time.",
                   UintegerValue (512),
                   MakeUintegerAccessor (&CustomBulkApplication::m_sendSize),
                   MakeUintegerChecker<uint32_t> (1))
    .AddAttribute ("Remote", "The address of the destination",
                   AddressValue (),
                   MakeAddressAccessor (&CustomBulkApplication::m_peer),
                   MakeAddressChecker ())
    .AddAttribute ("MaxBytes",
                   "The total number of bytes to send. "
                   "Once these bytes are sent, "
                   "no data  is sent again. The value zero means "
                   "that there is no limit.",
                   UintegerValue (0),
                   MakeUintegerAccessor (&CustomBulkApplication::m_maxBytes),
                   MakeUintegerChecker<uint64_t> ())
		.AddAttribute ("FlowId",
		               "Flow id "
		               "Once these bytes are sent",
			             UintegerValue (0),
			             MakeUintegerAccessor (&CustomBulkApplication::m_flowId),
			             MakeUintegerChecker<uint64_t> ())
    .AddAttribute ("Protocol", "The type of protocol to use.",
                   TypeIdValue (TcpSocketFactory::GetTypeId ()),
                   MakeTypeIdAccessor (&CustomBulkApplication::m_tid),
                   MakeTypeIdChecker ())
    .AddTraceSource ("Tx", "A new packet is created and is sent",
                     MakeTraceSourceAccessor (&CustomBulkApplication::m_txTrace),
                     "ns3::Packet::TracedCallback")
  ;
  return tid;
}


CustomBulkApplication::CustomBulkApplication ()
  : m_socket (0),
    m_connected (false),
    m_totBytes (0),
		m_startTime(0),
		m_flowId(0),
		m_started(false),
	  m_outputFile(NULL),
	  m_counterFile(NULL),
	  m_flowsFile(NULL),
		m_recordingTime(-1),
		m_insideIntervalFlow(false)
{
  NS_LOG_FUNCTION (this);
}

CustomBulkApplication::~CustomBulkApplication ()
{
  NS_LOG_FUNCTION (this);
}

void
CustomBulkApplication::SetMaxBytes (uint64_t maxBytes)
{
  NS_LOG_FUNCTION (this << maxBytes);
  m_maxBytes = maxBytes;
}

Ptr<Socket>
CustomBulkApplication::GetSocket (void) const
{
  NS_LOG_FUNCTION (this);
  return m_socket;
}

void
CustomBulkApplication::DoDispose (void)
{
  NS_LOG_FUNCTION (this);

  m_socket = 0;
  // chain up
  Application::DoDispose ();
}

uint32_t CustomBulkApplication::GetTxBufferSize(void){
  NS_LOG_FUNCTION (this);

  return DynamicCast<TcpSocketBase>(m_socket)->GetTxBuffer()->Size();
}

void CustomBulkApplication::SetOutputFile(Ptr<OutputStreamWrapper> file){
	m_outputFile = file;
}

void CustomBulkApplication::SetCounterFile(Ptr<OutputStreamWrapper> file){
	m_counterFile = file;
}

void CustomBulkApplication::SetFlowsFile(Ptr<OutputStreamWrapper> file){
	m_flowsFile = file;
}

void CustomBulkApplication::SetStartRecordingTime(double * startTime){
	m_startRecordingTime =  startTime;
}

void CustomBulkApplication::SetRecordedFlowsCounter(uint64_t * recordedFlowsCounter){
	m_recordedFlowsCounter =  recordedFlowsCounter;
}

void CustomBulkApplication::SetRecordingTime(double recordingTime){
	m_recordingTime = recordingTime;
}


void CustomBulkApplication::SetSocket(Ptr<Socket> s){
	NS_LOG_FUNCTION(this);
	m_socket = s;
}


// Application Methods
void CustomBulkApplication::StartApplication (void) // Called at time specified by Start
{
  NS_LOG_FUNCTION (this);

  // Create the socket if not already
  if (!m_socket)//(!m_socket)
    {
  		//  		m_started=true;
      m_socket = Socket::CreateSocket (GetNode (), m_tid);

      // Fatal error if socket type is not NS3_SOCK_STREAM or NS3_SOCK_SEQPACKET
      if (m_socket->GetSocketType () != Socket::NS3_SOCK_STREAM &&
          m_socket->GetSocketType () != Socket::NS3_SOCK_SEQPACKET)
        {
          NS_FATAL_ERROR ("Using BulkSend with an incompatible socket type. "
                          "BulkSend requires SOCK_STREAM or SOCK_SEQPACKET. "
                          "In other words, use TCP instead of UDP.");
        }

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

//      if recording time was set
      if (m_startRecordingTime != NULL){
				if (*m_startRecordingTime > 0 && m_recordingTime != -1){

					//Check if the flow is contained in the period we are interested in
					if (m_startTime >= *m_startRecordingTime && m_startTime <= (*m_startRecordingTime + m_recordingTime)){
						//Increase counter by one
						*m_recordedFlowsCounter +=1;
						m_insideIntervalFlow = true;
					}

				}
      }

      m_socket->Connect (m_peer);
      m_socket->ShutdownRecv ();

      m_socket->SetConnectCallback (
        MakeCallback (&CustomBulkApplication::ConnectionSucceeded, this),
        MakeCallback (&CustomBulkApplication::ConnectionFailed, this));
      m_socket->SetSendCallback (
        MakeCallback (&CustomBulkApplication::DataSend, this));

      //Close Callback to measure FCT
      m_socket->SetCloseCallbacks(
      		MakeCallback(&CustomBulkApplication::SocketNormalClose, this),
      		MakeCallback(&CustomBulkApplication::SocketErrorClose, this)
      );



      //Get Flow 5 tuple
      m_flow_tuple.srcAddr = GetNodeIp(m_socket->GetNode());
      InetSocketAddress inetDstAddr = InetSocketAddress::ConvertFrom(this->m_peer);
      m_flow_tuple.dstAdrr = inetDstAddr.GetIpv4();
      m_flow_tuple.srcPort = DynamicCast<TcpSocketBase>(m_socket)->GetLocalPort();
      m_flow_tuple.dstPort = inetDstAddr.GetPort();

      //Store flows info
      if (m_flowsFile != NULL)
      {

				*(m_flowsFile->GetStream ()) << m_startTime << " " << m_flow_tuple.srcAddr << " " << m_flow_tuple.dstAdrr << " " << m_flow_tuple.srcPort << " " <<
						m_flow_tuple.dstPort << " " << m_maxBytes << " " <<  m_flowId  <<  "\n";

				(m_flowsFile->GetStream())->flush();
      }

    }

  if (m_connected)
    {
      SendData ();
    }
}

void CustomBulkApplication::StopApplication (void) // Called at time specified by Stop
{
  NS_LOG_FUNCTION (this);

  if (m_socket != 0)
    {
      m_socket->Close ();
      m_connected = false;
    }
  else
    {
      NS_LOG_WARN ("CustomBulkApplication found null socket to close in StopApplication");
    }
}


// Private helpers

void CustomBulkApplication::SendData (void)
{
  NS_LOG_FUNCTION (this);
  while (m_maxBytes == 0 || m_totBytes < m_maxBytes)
    { // Time to send more

      // uint64_t to allow the comparison later.
      // the result is in a uint32_t range anyway, because
      // m_sendSize is uint32_t.
      uint64_t toSend = m_sendSize;
      // Make sure we don't send too many
      if (m_maxBytes > 0)
        {
          toSend = std::min (toSend, m_maxBytes - m_totBytes);
        }

      NS_LOG_LOGIC ("sending packet at " << Simulator::Now ());
      Ptr<Packet> packet = Create<Packet> (toSend);
      int actual = m_socket->Send (packet);
      if (actual > 0)
        {
          m_totBytes += actual;
          m_txTrace (packet);
        }
      // We exit this loop when actual < toSend as the send side
      // buffer is full. The "DataSent" callback will pop when
      // some buffer space has freed up.
//  		NS_LOG_UNCOND(GetNodeName(GetNode()) <<"  " << Simulator::Now().GetSeconds());

      if ((unsigned)actual != toSend)
        {
          break;
        }
    }



  // Check if time to close (all sent)
  if (m_totBytes == m_maxBytes && m_connected) //&& (GetTxBufferSize() == 0))
    {
   		m_socket->Close ();
      m_connected = false;
    }
}

void CustomBulkApplication::ConnectionSucceeded (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("CustomBulkApplication Connection succeeded");
  m_connected = true;
  SendData ();
}

void CustomBulkApplication::ConnectionFailed (Ptr<Socket> socket)
{
  NS_LOG_FUNCTION (this << socket);
  NS_LOG_LOGIC ("CustomBulkApplication, Connection Failed");
}

void CustomBulkApplication::DataSend (Ptr<Socket>, uint32_t)
{
  NS_LOG_FUNCTION (this);

  if (m_connected)
    {
  	// Only send new data if the connection has completed

      SendData ();
    }
}

void CustomBulkApplication::SocketNormalClose (Ptr<Socket>)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_DEBUG("Connection closed normally");

  double endTime  = Simulator::Now().GetSeconds();


  if (m_insideIntervalFlow){
  	*m_recordedFlowsCounter = (*m_recordedFlowsCounter) -1;

  	//Write in file
  	*(m_counterFile->GetStream()) << *m_recordedFlowsCounter << "\n";
  	m_counterFile->GetStream()->flush();

//      	NS_LOG_UNCOND("Counter value at: "<< *m_recordedFlowsCounter);

					//create 5 tuple
		std::ostringstream flowIdentification;

		flowIdentification << Ipv4AddressToString(m_flow_tuple.srcAddr)<< ":" << m_flow_tuple.srcPort  << "_" << m_flow_tuple.dstAdrr
				<< ":" << m_flow_tuple.dstPort << "_" << m_flowId;

		*(m_outputFile->GetStream ()) << flowIdentification.str() << " " << m_maxBytes << " "
				<< (endTime-m_startTime) << " " << m_startTime << " " << endTime << "\n";

		(m_outputFile->GetStream())->flush();

		//If flows counter is 0 we will decide to stop the simulation, but only if the flow has finished outside the simulation window
		//IMPORTANT there is a small provability of never terminate the simulation
		if (*m_recordedFlowsCounter == 0 && endTime > (*m_startRecordingTime + m_recordingTime)){
			//Stop simulation
			NS_LOG_UNCOND("Flows inside the period finished, stopping simulation...");
			Simulator::Stop(Seconds(0));

  }
  }
  //In this case we record all flows

  else if(m_recordingTime == -1){

		//create 5 tuple
		std::ostringstream flowIdentification;

		flowIdentification << Ipv4AddressToString(m_flow_tuple.srcAddr)<< ":" << m_flow_tuple.srcPort  << "_" << m_flow_tuple.dstAdrr
				<< ":" << m_flow_tuple.dstPort << "_" << m_flowId;

		if (m_outputFile != NULL)
		{
			*(m_outputFile->GetStream ()) << flowIdentification.str() << " " << m_maxBytes << " "
					<< (endTime-m_startTime) << " " << m_startTime << " " << endTime << "\n";

			(m_outputFile->GetStream())->flush();
		}
  }

}

void CustomBulkApplication::SocketErrorClose (Ptr<Socket>)
{
  NS_LOG_FUNCTION (this);

  NS_LOG_ERROR("Connection closed by an error");


}



} // Namespace ns3
