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
 */

#include <fstream>
#include <sstream>
#include <iostream>
#include <vector>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/my-onoff-application-helper.h"
#include "ns3/flow-monitor-helper.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("ScriptExample");

// ===========================================================================
//
//            gw0
//   +----------------+
//   |    ns-3 TCP    |
//   +----------------+         net1
//   |    10.1.1.1    |+---------------------+
//   +----------------+      8 Mbps, 2 ms    |
//   | point-to-point |                   router0                              server0
//   +----------------+             +----------------+                   +----------------+
//                                  |    ns-3 TCP    |                   |    ns-3 TCP    |
//                                  +----------------+   10 Mbps, 5 ms   +----------------+
//                                  |                |+-----------------+|    10.1.3.2    |
//            gw1                   +----------------+       net2        +----------------+
//    +----------------+            | point-to-point |                   | point-to-point |
//    |    ns-3 TCP    |            +----------------+                   +----------------+
//    +----------------+         net1        |
//    |    10.1.2.1    |+--------------------+
//    +----------------+     8 Mbps, 2 ms 
//    | point-to-point |
//    +----------------+
//
//    実験用に変更を加えたOnOffApplicationを利用してパケットの送信を行う．
//    パケットの送信をポアソン分布に従って行う．
//    App DataRate 10Mbps, PacketSize 5200bytes
// ===========================================================================


static bool firstRtt = true;//to write the first rtt


static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_INFO(Simulator::Now().GetSeconds() << " " << newCwnd);
  *stream->GetStream() << Simulator::Now().GetSeconds() << " " << oldCwnd << " " << newCwnd << std::endl;
}

static void
RttTracer(Ptr<OutputStreamWrapper> rttStream, Time oldval, Time newval)
{
  if(firstRtt)
    {
      *rttStream->GetStream() << "0.0 " << oldval.GetSeconds() << std::endl;
      firstRtt = false;
    }
  *rttStream->GetStream() << Simulator::Now().GetSeconds() << " " << newval.GetSeconds() << std::endl;
}

static void
PhyTxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << " " << packet->GetSize() << std::endl;
}

static void
RxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet, const Address& address)
{
  *stream->GetStream() << InetSocketAddress::ConvertFrom(address).GetIpv4() << " " << Simulator::Now().GetSeconds() << " " << packet->GetSize() << std::endl;
}

static void
TxTracer(Ptr<OutputStreamWrapper> stream, Ptr<const Packet> packet)
{
  *stream->GetStream() << Simulator::Now().GetSeconds() << " " << packet->GetSize() << std::endl;
}


int
main(int argc, char *argv[])
{
  const double SIM_TIME = 360.0;
  const int NODE_NUM = 10;
  const int MTU = 1500;

  CommandLine cmd;
  cmd.Parse(argc, argv);

  NS_LOG_INFO("Creating Topology");
  
  Header* temp_header = new Ipv4Header ();
  uint32_t ip_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("IP Header size is: " << ip_header);
  delete temp_header;
  temp_header = new TcpHeader ();
  uint32_t tcp_header = temp_header->GetSerializedSize ();
  NS_LOG_LOGIC ("TCP Header size is: " << tcp_header);
  delete temp_header;
  const int MSS = MTU - (ip_header + tcp_header);

  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue(MSS));

  NodeContainer gws;
  gws.Create(NODE_NUM);

  NodeContainer router;
  router.Create(1);

  NodeContainer server;
  server.Create(1);

  std::vector<NodeContainer> net1;
  for(int i=0;i<NODE_NUM;i++){
    NodeContainer net;
    net.Add(gws.Get(i));
    net.Add(router.Get(0));
    net1.push_back(net);
  }

  NodeContainer net2;
  net2.Add(router.Get(0));
  net2.Add(server.Get(0));

  InternetStackHelper internet;
  internet.InstallAll();

  PointToPointHelper p2p1;
  p2p1.SetDeviceAttribute("DataRate", StringValue("8Mbps"));
  p2p1.SetChannelAttribute("Delay", StringValue("2ms"));

  PointToPointHelper p2p2;
  p2p2.SetDeviceAttribute("DataRate", StringValue("10Mbps"));
  p2p2.SetChannelAttribute("Delay", StringValue("5ms"));

  std::vector<NetDeviceContainer> devices1;
  for(auto i: net1){
    NetDeviceContainer device;
    device = p2p1.Install(i);
    devices1.push_back(device);
  }

  NetDeviceContainer devices2;
  devices2 = p2p1.Install(net2);

  /*
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(0.00001));
  for(auto i: devices1){
    i.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  }
  devices2.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));
  */

  NS_LOG_INFO("Assigning adress");
  Ipv4AddressHelper address;
  std::vector<Ipv4InterfaceContainer> interfaces1;
  for(unsigned int i=0;i<devices1.size();i++){
    std::stringstream ss;
    ss << "10.1." << i <<".0";
    address.SetBase(ss.str().c_str(), "255.255.255.252");
    Ipv4InterfaceContainer interface = address.Assign(devices1[i]);
    interfaces1.push_back(interface);
  }
  address.SetBase("10.2.1.0", "255.255.255.252");
  Ipv4InterfaceContainer interfaces2 = address.Assign(devices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  // server port
  uint16_t sinkPort = 8080;

  NS_LOG_INFO("Creating Socket");
  // GetAny() = 0.0.0.0
  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
  ApplicationContainer sinkApps = packetSinkHelper.Install(server.Get(0));
  sinkApps.Start(Seconds(0.1));
  sinkApps.Stop(Seconds(SIM_TIME+5));

  MyOnOffHelper clientHelper("ns3::TcpSocketFactory", Address());
  AddressValue remoteAddress(InetSocketAddress(interfaces2.GetAddress(1), sinkPort));
  clientHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
  clientHelper.SetAttribute("OffTime", StringValue("ns3::ExponentialRandomVariable[Mean=1]"));
  clientHelper.SetAttribute("PacketSize", UintegerValue(5096));
  clientHelper.SetAttribute("DataRate", DataRateValue(DataRate("8Mb/s")));
  clientHelper.SetAttribute("Remote",remoteAddress);


  std::vector<Ptr<Socket>> ns3TcpSockets;//vector化
  std::vector<ApplicationContainer> clientApps;
  for(int i=0;i<NODE_NUM;i++){
    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(gws.Get(i), TcpSocketFactory::GetTypeId());
    ApplicationContainer clientApp;
    clientApp.Add(clientHelper.Install(gws.Get(0)));
    clientApp.Get(0)->GetObject<MyOnOffApplication>()->SetSocket(ns3TcpSocket);
    clientApp.Start(Seconds(1.0));
    clientApp.Stop(Seconds(SIM_TIME));
    ns3TcpSockets.push_back(ns3TcpSocket);
    clientApps.push_back(clientApp);
 }

  AsciiTraceHelper asciiTraceHelper;

  int streamN = 0;
  for(auto i: ns3TcpSockets){
    std::stringstream ss;
    ss << "myExample-Socket-" << streamN << ".cwnd";
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream(ss.str().c_str());
    i->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream));
    ss.str("");
    ss.clear();
    ss << "myExample-Rtt-" << streamN << ".cwnd";
    Ptr<OutputStreamWrapper> rttStream = asciiTraceHelper.CreateFileStream(ss.str().c_str());
    i->TraceConnectWithoutContext("RTT", MakeBoundCallback(&RttTracer, rttStream));
    streamN++;
  }

  streamN = 0;
  for(auto i: clientApps){
    std::stringstream ss;
    ss << "myExample-Tx-" << streamN << ".cwnd";
    Ptr<OutputStreamWrapper> txStream = asciiTraceHelper.CreateFileStream(ss.str().c_str());
    i.Get(0)->GetObject<MyOnOffApplication>()->TraceConnectWithoutContext("Tx", MakeBoundCallback(&TxTracer, txStream));
    streamN++;
  }

  streamN = 0;
  for(auto i: devices1){
    std::stringstream ss;
    ss << "myExample-PhyTx-" << streamN << ".cwnd";
    Ptr<OutputStreamWrapper> phyTxStream = asciiTraceHelper.CreateFileStream(ss.str().c_str());
    i.Get(0)->TraceConnectWithoutContext("PhyTxBegin", MakeBoundCallback(&PhyTxTracer, phyTxStream));
    streamN++;
  }

  Ptr<OutputStreamWrapper> rxStream = asciiTraceHelper.CreateFileStream("myExample-Rx.cwnd");
  sinkApps.Get(0)->GetObject<PacketSink>()->TraceConnectWithoutContext("Rx", MakeBoundCallback(&RxTracer, rxStream));

  //FlowMonitorHelper flowHelper;
  //flowHelper.InstallAll();

  Simulator::Stop(Seconds(SIM_TIME+5));
  Simulator::Run();
  //flowHelper.SerializeToXmlFile("myExample-FM.fm",true,true);
  Simulator::Destroy();

  return 0;
}
