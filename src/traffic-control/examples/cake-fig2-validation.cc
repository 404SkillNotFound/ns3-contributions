/*
 * Copyright (c) 2026 Shivang Upadhyay
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Validation script for CAKE Figure 2 (Latency under load).
 * Based on: arXiv:1804.07617 (Høiland-Jørgensen et al.)
 *
 * Topology:
 *   src0 ──[100 Mbps / 1 ms]──┐
 *                           R0 ──[10 Mbps / 5 ms]──> R1 ──[100 Mbps / 1 ms]──> sink
 *   src1 ──[100 Mbps / 1 ms]──┘
 *
 * src0: greedy TCP Cubic (saturates bottleneck)
 * src1: 1ms UDP probes, RTT/2 used as one-way latency proxy
 * sink: PacketSink + UdpEchoServer
 *
 * Run 1: CAKE on R0 egress
 * Run 2: DropTail (180p) on R0 egress, no AQM
 *
 * Note: The paper uses FQ-CoDel as baseline but that does not work in ns-3
 * because the perfect 5-tuple hash always puts the UDP probe in its own
 * empty queue, so it never sees congestion. RED+ECN was also tried but it
 * stabilises the queue instead of producing a sawtooth. DropTail with a
 * sized buffer reproduces the bufferbloat behaviour the paper shows.
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <cstring>
#include <fstream>
#include <iomanip>
#include <map>
#include <string>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CakeFig2");

static const uint16_t ECHO_PORT = 9;
static const uint16_t SINK_PORT = 5001;
static const uint32_t PROBE_PAYLOAD = 64; ///< 4-byte uid + 64-byte pad = 68 B total
static const Time PROBE_INTERVAL = MilliSeconds(1);

static std::map<uint32_t, Time> g_probeTx; ///< uid → departure timestamp
static uint32_t g_uid = 0;
static std::ofstream g_latFile;
static Ptr<Socket> g_probeSock;
static Time g_probeStop;

/**
 * @brief Send one uid-tagged UDP probe and reschedule for the next interval.
 *
 * The first 4 bytes carry a big-endian uid so the matching echo reply can be
 * correlated with its exact departure timestamp.
 */
static void
SendProbe()
{
    if (Simulator::Now() >= g_probeStop)
    {
        return;
    }

    uint8_t buf[4 + PROBE_PAYLOAD];
    buf[0] = (g_uid >> 24) & 0xff;
    buf[1] = (g_uid >> 16) & 0xff;
    buf[2] = (g_uid >> 8) & 0xff;
    buf[3] = g_uid & 0xff;
    std::memset(buf + 4, 0xab, PROBE_PAYLOAD);

    g_probeTx[g_uid] = Simulator::Now();
    ++g_uid;

    g_probeSock->Send(Create<Packet>(buf, 4 + PROBE_PAYLOAD));
    Simulator::Schedule(PROBE_INTERVAL, &SendProbe);
}

/**
 * @brief Receive callback: correlate echo uid, compute RTT/2, write to file.
 *
 * The reverse path carries no bulk traffic, so RTT/2 = 7 ms + Q_delay/2 and
 * faithfully tracks the forward queuing depth.
 *
 * @param sock  The probe client socket (src1).
 */
static void
RecvEcho(Ptr<Socket> sock)
{
    Ptr<Packet> pkt;
    Address from;
    while ((pkt = sock->RecvFrom(from)) != nullptr)
    {
        if (pkt->GetSize() < 4)
        {
            continue;
        }

        uint8_t hdr[4];
        pkt->CopyData(hdr, 4);
        uint32_t uid = (static_cast<uint32_t>(hdr[0]) << 24) |
                       (static_cast<uint32_t>(hdr[1]) << 16) |
                       (static_cast<uint32_t>(hdr[2]) << 8) | static_cast<uint32_t>(hdr[3]);

        auto it = g_probeTx.find(uid);
        if (it == g_probeTx.end())
        {
            continue;
        }

        double latMs = (Simulator::Now() - it->second).GetSeconds() * 500.0;
        g_latFile << std::fixed << std::setprecision(3) << Simulator::Now().GetSeconds() << "\t"
                  << std::setprecision(3) << latMs << "\n";
        g_probeTx.erase(it);
    }
}

/**
 * @brief Create the probe UDP socket on probeNode and start SendProbe().
 *
 * @param probeNode  src1 node.
 * @param sinkAddr   IPv4 address of the UdpEchoServer.
 */
static void
OpenProbeSocket(Ptr<Node> probeNode, Ipv4Address sinkAddr)
{
    g_probeSock = Socket::CreateSocket(probeNode, TypeId::LookupByName("ns3::UdpSocketFactory"));
    g_probeSock->Connect(InetSocketAddress(sinkAddr, ECHO_PORT));
    g_probeSock->SetRecvCallback(MakeCallback(&RecvEcho));
    SendProbe();
}

/**
 * @brief Build the topology, install the chosen queue policy, and run.
 *
 * @param useCake    true  → CakeQueueDisc (shaper + COBALT).
 *                   false → DropTailQueue (180p) on the bottleneck device.
 * @param outFile    Destination .dat file for RTT/2 samples.
 * @param bwStr      Bottleneck DataRate string (e.g. "10Mbps").
 * @param cakeBwStr  CAKE shaper bandwidth (must equal bwStr).
 * @param delayStr   Bottleneck one-way propagation delay (e.g. "5ms").
 * @param simStop    Simulation stop time.
 */
static void
RunScenario(bool useCake,
            const std::string& outFile,
            const std::string& bwStr,
            const std::string& cakeBwStr,
            const std::string& delayStr,
            Time simStop)
{
    NS_LOG_INFO((useCake ? "CAKE: " : "DropTail baseline (180p): "));

    g_probeTx.clear();
    g_uid = 0;
    g_probeStop = simStop - Seconds(0.5);

    g_latFile.open(outFile);
    NS_ABORT_MSG_IF(!g_latFile.is_open(), "Cannot open " << outFile);
    g_latFile << "# Time_s\tRTT_half_ms\n";

    // Nodes
    NodeContainer sources;
    sources.Create(2); // src0 = TCP, src1 = probe
    NodeContainer routers;
    routers.Create(2); // R0, R1
    NodeContainer sink;
    sink.Create(1);

    // Access links: 100 Mbps / 1 ms
    PointToPointHelper access;
    access.SetDeviceAttribute("DataRate", DataRateValue(DataRate("100Mbps")));
    access.SetChannelAttribute("Delay", TimeValue(MilliSeconds(1)));
    access.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("1000p"));

    NetDeviceContainer dev0 = access.Install(sources.Get(0), routers.Get(0));
    NetDeviceContainer dev1 = access.Install(sources.Get(1), routers.Get(0));
    NetDeviceContainer egressDevs = access.Install(routers.Get(1), sink.Get(0));

    // Bottleneck link: bwStr / delayStr
    // Device queue is 1p so all buffering sits in the QueueDisc layer
    // (CAKE for run 1, or the resized device queue for run 2).
    PointToPointHelper bn;
    bn.SetDeviceAttribute("DataRate", DataRateValue(DataRate(bwStr)));
    bn.SetChannelAttribute("Delay", TimeValue(Time(delayStr)));
    bn.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("1p"));
    NetDeviceContainer bnDevs = bn.Install(routers.Get(0), routers.Get(1));

    // Internet stack and IP addresses
    InternetStackHelper internet;
    internet.Install(sources);
    internet.Install(routers);
    internet.Install(sink);

    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.252");
    ipv4.Assign(dev0);
    ipv4.SetBase("10.1.2.0", "255.255.255.252");
    ipv4.Assign(dev1);
    ipv4.SetBase("10.2.0.0", "255.255.255.252");
    ipv4.Assign(bnDevs);
    ipv4.SetBase("10.3.0.0", "255.255.255.252");
    Ipv4InterfaceContainer egressIfaces = ipv4.Assign(egressDevs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ipv4Address sinkAddr = egressIfaces.GetAddress(1);

    // Remove the pfifo_fast that ns-3 auto-installs on the bottleneck device
    TrafficControlHelper tchClean;
    tchClean.Uninstall(bnDevs);

    if (useCake)
    {
        TrafficControlHelper tch;
        tch.SetRootQueueDisc("ns3::CakeQueueDisc",
                             "Bandwidth",
                             DataRateValue(DataRate(cakeBwStr)),
                             "DiffServMode",
                             UintegerValue(0));
        tch.Install(bnDevs.Get(0));
    }
    else
    {
        // No AQM -- plain DropTail on the device queue.
        // pfifo_fast is already gone from the tchClean.Uninstall above, so we
        // just resize the device queue to 180p. TCP Cubic fills it, hits the
        // limit, backs off, drains, and refills, producing the sawtooth.
        Ptr<PointToPointNetDevice> p2pDev = DynamicCast<PointToPointNetDevice>(bnDevs.Get(0));
        p2pDev->GetQueue()->SetAttribute("MaxSize", QueueSizeValue(QueueSize("180p")));
    }

    // Applications
    PacketSinkHelper tcpSink("ns3::TcpSocketFactory",
                             InetSocketAddress(Ipv4Address::GetAny(), SINK_PORT));
    ApplicationContainer tcpSinkApp = tcpSink.Install(sink.Get(0));
    tcpSinkApp.Start(Seconds(0.0));
    tcpSinkApp.Stop(simStop);

    BulkSendHelper bulk("ns3::TcpSocketFactory", InetSocketAddress(sinkAddr, SINK_PORT));
    bulk.SetAttribute("MaxBytes", UintegerValue(0));
    bulk.SetAttribute("SendSize", UintegerValue(1448));
    ApplicationContainer bulkApp = bulk.Install(sources.Get(0));
    bulkApp.Start(Seconds(1.0));
    bulkApp.Stop(simStop);

    UdpEchoServerHelper echoServer(ECHO_PORT);
    ApplicationContainer echoApp = echoServer.Install(sink.Get(0));
    echoApp.Start(Seconds(0.0));
    echoApp.Stop(simStop);

    Simulator::Schedule(Seconds(1.0), &OpenProbeSocket, sources.Get(1), sinkAddr);

    Simulator::Stop(simStop);
    Simulator::Run();
    Simulator::Destroy();

    g_latFile.close();
    NS_LOG_INFO((useCake ? "CAKE" : "DropTail baseline") << " results written to " << outFile);
}

/**
 * @brief Entry point for the CAKE Figure 2 validation simulation.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success.
 */
int
main(int argc, char* argv[])
{
    std::string cakeOut = "cake-fig2-cake.dat";
    std::string noAqmOut = "cake-fig2-noaqm.dat";
    std::string bwStr = "10Mbps";
    std::string cakeBwStr = "10Mbps";
    std::string delayStr = "5ms";
    Time simStop = Seconds(62);

    CommandLine cmd(__FILE__);
    cmd.AddValue("cakeOut", "CAKE output .dat file", cakeOut);
    cmd.AddValue("noAqmOut", "DropTail baseline output .dat file", noAqmOut);
    cmd.AddValue("bottleneckRate", "Bottleneck link rate", bwStr);
    cmd.AddValue("cakeBandwidth", "CAKE shaper bandwidth (0bps=disabled)", cakeBwStr);
    cmd.AddValue("delay", "Bottleneck one-way propagation delay", delayStr);
    cmd.AddValue("simStop", "Simulation stop time", simStop);
    cmd.Parse(argc, argv);

    LogComponentEnable("CakeFig2", LOG_LEVEL_INFO);

    Config::SetDefault("ns3::TcpL4Protocol::SocketType",
                       TypeIdValue(TypeId::LookupByName("ns3::TcpCubic")));
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(1448));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(10));
    Config::SetDefault("ns3::TcpSocketBase::UseEcn", StringValue("On"));

    RunScenario(true, cakeOut, bwStr, cakeBwStr, delayStr, simStop);
    RunScenario(false, noAqmOut, bwStr, cakeBwStr, delayStr, simStop);

    return 0;
}
