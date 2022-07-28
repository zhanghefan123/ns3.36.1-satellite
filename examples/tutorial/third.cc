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
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   ================
//                                     LAN 10.1.2.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

/**
 * @brief third脚本的wifi无线网络由一个接入点（Access-Point AP）和 nWifi 个移动节点组成。
 * 其中无线AP也是一个双模节点。他安装有WIFI和PPP两个网络设备。
 * 
 * 在这里除了WifiNetDevice类之外，WIFI网络设备还包含链路层（WifiMac）与物理层（WifiPhy）。这里的
 * WifiNetDevice只是起一个连接上下层协议的作用，没有什么实质性的功能，主要的wifi协议功能都集中在wifimac和
 * wifiphy两个基类以及其各种子类之中实现。这种结构可以将一个复杂的功能模块化，有利于后续的开发和维护，另一方面
 * 这其实更加符合物理网络的层次划分，这种netdevice+mac+phy的3层网络设备结构在ns-3的无线节点中会经常使用到。
 * */

int
main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t nCsma = 3;
  uint32_t nWifi = 3; // 3个移动节点
  bool tracing = false;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc, argv);

  // The underlying restriction of 18 is due to the grid position
  // allocator's configuration; the grid layout will exceed the
  // bounding box if more than 18 nodes are provided.
  if (nWifi > 18)
    {
      std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box"
                << std::endl;
      return 1;
    }

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer csmaNodes;
  csmaNodes.Add (p2pNodes.Get (1));
  csmaNodes.Create (nCsma);

  CsmaHelper csma;
  csma.SetChannelAttribute ("DataRate", StringValue ("100Mbps"));
  csma.SetChannelAttribute ("Delay", TimeValue (NanoSeconds (6560)));

  NetDeviceContainer csmaDevices;
  csmaDevices = csma.Install (csmaNodes);

  NodeContainer wifiStaNodes;
  wifiStaNodes.Create (nWifi);
  NodeContainer wifiApNode = p2pNodes.Get (0);

  // （1） 设置Channel和WifiPhy
  // 这两个类都存在一些重要的参数需要进行配置，例如我们的WifiChanenel需要配置传
  // 播延迟和接收功率。WifiPhy需要配置误码率模型，如果没有特殊的配置要求，我们使用
  // 默认的配置即可。
  // ---------------------------------------------------------------
  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());
  // ---------------------------------------------------------------

  // 设置wifimac 并在节点之中安装NetDevice
  // WifiMacHelper 用于设置链路层的WfifMac类，这里需要设置以下两个重要的参数
  // 参数1： wifimac子类 参数2：服务集标识符ssid
  // 前者决定了这个节点的种类是AP还是移动节点，后者决定了节点所属的服务集。
  // AP与移动节点的服务集一致才能够进行通信。
  // ---------------------------------------------------------------
  WifiMacHelper mac;
  Ssid ssid = Ssid ("ns-3-ssid");

  // 完成wifi信道和网络设备中各个组建的配置之后，就可以使用助手类WifiHelper将wifi设备
  // 安装到指定的节点之中
  WifiHelper wifi;

  // 存储所有的移动设备
  NetDeviceContainer staDevices;
  mac.SetType ("ns3::StaWifiMac", "Ssid", SsidValue (ssid), "ActiveProbing", BooleanValue (false));
  // 将phy,mac进行安装
  staDevices = wifi.Install (phy, mac, wifiStaNodes);

  // 存储所有的AP设备
  NetDeviceContainer apDevices;
  mac.SetType ("ns3::ApWifiMac", "Ssid", SsidValue (ssid));
  // 将phy,mac进行安装
  apDevices = wifi.Install (phy, mac, wifiApNode);
  // ---------------------------------------------------------------

  // 创建移动模型
  MobilityHelper mobility;

  // 移动模型是无线网络中一个必不可少的部分。
  // 在配置wifi channel和wifiphy时，一个
  // 分组在Channel中的传播延迟和接收功率的大
  // 小是由传播延迟模型，损耗模型和移动模型共同决定的。
  // 传播延迟和损耗模型在前面已经配置，而移动模型一般在
  // wifi网络设备安装到结点后配置。这是因为不同wifi结点
  // 类型需要采用不同的移动模型。

  // 看一下移动节点的移动模型的设置
  // 移动节点的移动模型设置分为以下的两个部分：初始位置分布和后续移动轨迹模型
  // 前者定义了一个移动节点的初始坐标，使用的初始位置分布器是我们的GridPositionAllocator,这个分布器按照设定好的行和列参数将节点等距离的放在一个二维笛卡尔坐标系中。
  // 后者定义了节点的移动路径，使用的移动轨迹模型是RandomWalk2MobilityModel。
  // 这个模型中的节点在一个制定大小的长方形区域内按照随机的速度和方向进行移动。
  mobility.SetPositionAllocator ("ns3::GridPositionAllocator", "MinX", DoubleValue (0.0), "MinY",
                                 DoubleValue (0.0), "DeltaX", DoubleValue (5.0), "DeltaY",
                                 DoubleValue (10.0), "GridWidth", UintegerValue (3), "LayoutType",
                                 StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel", "Bounds",
                             RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes);

  // AP节点是固定的节点，此处为其使用的移动模型是固定位置移动模型，这个模型的AP节点二维坐标为(0,0)
  // 即坐标的原点。
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode);

  // 下面进行的是tcp-ip协议簇的安装
  // ------------------------------------------------------------------------------

  InternetStackHelper stack;
  stack.Install (csmaNodes); // 为有线节点进行安装
  stack.Install (wifiApNode); // 为无线AP节点进行安装
  stack.Install (wifiStaNodes); // 为无线移动节点进行安装

  // 使用Ipv4AddressHelper进行地址的设置
  Ipv4AddressHelper address;

  // 两个p2p设备在同一个网段
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  // 将我们的csma设备设置为一个网段
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer csmaInterfaces;
  csmaInterfaces = address.Assign (csmaDevices);

  // 将AP设备和我们的移动节点设置为一个网段
  address.SetBase ("10.1.3.0", "255.255.255.0");
  address.Assign (staDevices);
  address.Assign (apDevices);
  // ------------------------------------------------------------------------------

  // 进行应用程序的安装
  // ------------------------------------------------------------------------------
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get (nCsma));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (wifiStaNodes.Get (nWifi - 1));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));
  // ------------------------------------------------------------------------------

  // 进行路由的设置
  // 当ns3的多个子网之间存在通信的时候，我们就需要在脚本中设置路由协议。例如，前面在创建CSMA网络的
  // 时候就提到一个p2pNodes.Get(1)。这个节点存在PointToPoint和CSMA两个网络设备，分别连接PointToPoint网络
  // 以及我们的CSMA网络。在地址分配之中，这两个网络分属于不同的子网。这就需要连接两个子网的p2pNodes.Get(1)
  // 具有路由的功能，这样才能够正确的转发从PPP子网发送给CSMA子网的分组，反之亦然
  //
  // ns3中最常用的路由协议之一就是全局路由，全局路由通过开放式最短路径优先路由算法计算网络拓扑中每两个节点的最短路径
  // 并且为每个节点生成路由表。对于IPV4协议，全局路由设置只要像如下这样进行调用即可。
  // ------------------------------------------------------------------------------
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  // ------------------------------------------------------------------------------
  Simulator::Stop (Seconds (10.0));

  // 数据追踪
  // 仿真的一个重要目的就是分析网络性能，获取实验数据是分析网络性能的重要前提，ns3为用户提供了丰富的数据追踪和
  // 收集的功能，在ns3模拟脚本之中，收集到的数据经常以pcap或者ascii文本的格式保存在指定的文件之中，这部分代码
  // 往往放在代码的末尾
  // ------------------------------------------------------------------------------
  if (tracing)
    {
      phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      // EnablePcapAll的作用是收集这个信道上所有节点的链路层分组收发记录，记录文件的格式为pcap
      // 这个函数对文件名的命名规则为“前缀名-节点编号-网络设备编号”
      pointToPoint.EnablePcapAll ("third");
      // 打印AP节点中wifi网络设备物理层分组收发记录
      phy.EnablePcap ("third", apDevices.Get (0));
      // 打印一个有线节点中CSMA网络设备分组收发记录
      csma.EnablePcap ("third", csmaDevices.Get (0), true);
      // 注意：除了tcpdump，wireshark软件也可以打开pcap文件
    }
  // ------------------------------------------------------------------------------

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
