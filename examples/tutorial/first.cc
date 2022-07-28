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

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"

// Default Network Topology
//
//       10.1.1.0
// n0 -------------- n1
//    point-to-point
//

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("FirstScriptExample");

int
main (int argc, char *argv[])
{
  CommandLine cmd (__FILE__);
  cmd.Parse (argc, argv);

  Time::SetResolution (Time::NS);
  LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
  LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);

  NodeContainer nodes; // NodeContainer 是一个包含节点的容器
  nodes.Create (2); // NodeContainer 能够通过调用 Create 方法创建节点，传入节点数量即可

  PointToPointHelper pointToPoint; // 准备使用 PointToPointHelper 来配置信道属性
  pointToPoint.SetDeviceAttribute ("DataRate",
                                   StringValue ("5Mbps")); // 设置信道的 DataRate 数据率属性
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms")); // 设置信道的 Delay 延迟属性

  NetDeviceContainer devices;
  devices = pointToPoint.Install (nodes); // 具体看Install方法的注释

  // 为节点安装TCP/IP协议的助手类是InternetStackHelper
  InternetStackHelper stack;
  // stack 对象调用 Install 方法之后就为我们的节点安装了协议栈
  stack.Install (nodes);
  // 光安装了协议栈的节点还不能够进行直接的通信，还需要为网络设备分配IP地址
  Ipv4AddressHelper address;
  // 这个SetBase方法不止分配了一个地址，而是分配了一段地址
  address.SetBase ("10.1.1.0", "255.255.255.0");
  // Ipv4AddressHelper为我们的devices上的接口进行地址的分配，然后返回接口
  Ipv4InterfaceContainer interfaces = address.Assign (devices);
  // 创建一个回显应用层服务器，监听9号端口，并从1s后开始进行监听，10s结束
  // ------------------------------------------------------------------
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (nodes.Get (1));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));
  // ------------------------------------------------------------------

  // 创建一个回显应用层客户端，目地地址为1号节点接口的地址，目的端口为9
  UdpEchoClientHelper echoClient (interfaces.GetAddress (1), 9);
  // 回显服务器进行属性的设置，包括最大包数量,分组发送间隔，分组大小，从2s开始发送，10s停止
  // ------------------------------------------------------------------
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (nodes.Get (0));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));
  // ------------------------------------------------------------------

  // 进行之前步骤定义的所有操作
  Simulator::Run ();
  // Destroy执行清除的操作
  Simulator::Destroy ();
  // C++模拟脚本本质上是一个main函数，需要返回0告诉操作系统程序执行成功
  return 0;
}
