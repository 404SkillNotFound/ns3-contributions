/*
 * Copyright (c) 2026 Shivang Upadhyay
 *
 * SPDX-License-Identifier: GPL-2.0-only
 *
 * Validation script for CAKE Figure 3 (Flow Fairness).
 * Based on: arXiv:1804.07617 (Høiland-Jørgensen et al.)
 */

#include "ns3/applications-module.h"
#include "ns3/core-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/internet-module.h"
#include "ns3/network-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/traffic-control-module.h"

#include <fstream>
#include <iomanip>
#include <map>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("CakeFig3");

static Ptr<FlowMonitor> g_monitor;
static FlowMonitorHelper g_flowMonHelper;
static Ptr<Ipv4FlowClassifier> g_classifier;
static std::ofstream g_outFile;

// Cumulative rx bytes at the previous sample epoch, keyed by FlowId.
static std::map<FlowId, uint64_t> g_prevBytes;

// Tracks whether a flow has been seen before (to skip the cold-start delta).
static std::map<FlowId, bool> g_seenFlow;

static Time g_sampleInterval = Seconds(1.0);
static uint32_t g_numFlows = 3;

// Only count data flows destined to this port (not reverse-direction ACKs).
static const uint16_t SINK_PORT = 5000;

/**
 * @brief Samples per-flow throughput and writes results to g_outFile.
 *
 * Data flows are ordered by start time to maintain consistent column mapping.
 * The first sample per flow is used as a baseline to avoid startup spikes.
 *
 * @param stopTime Simulation stop time.
 */
static void
SampleThroughput(Time stopTime)
{
    Time now = Simulator::Now();
    g_monitor->CheckForLostPackets();
    const FlowMonitor::FlowStatsContainer& stats = g_monitor->GetFlowStats();

    // Collect data flows only (destination port == SINK_PORT), ordered by
    // first-transmission time so columns match the staggered start order.
    std::map<Time, std::pair<FlowId, double>> ordered;

    for (const auto& kv : stats)
    {
        FlowId fid = kv.first;
        Ipv4FlowClassifier::FiveTuple tuple = g_classifier->FindFlow(fid);

        if (tuple.destinationPort != SINK_PORT)
        {
            continue;
        }

        uint64_t curBytes = kv.second.rxBytes;
        uint64_t prevBytes = 0;

        auto pit = g_prevBytes.find(fid);
        if (pit != g_prevBytes.end())
        {
            prevBytes = pit->second;
        }
        g_prevBytes[fid] = curBytes;

        double tput = 0.0;
        if (g_seenFlow.count(fid) == 0)
        {
            // First encounter: record baseline only, report 0 for this sample.
            g_seenFlow[fid] = true;
        }
        else
        {
            tput = static_cast<double>((curBytes - prevBytes) * 8) / g_sampleInterval.GetSeconds() /
                   1e6; // Mbps
        }

        ordered[kv.second.timeFirstTxPacket] = {fid, tput};
    }

    // Write one line: timestamp + g_numFlows throughput columns.
    g_outFile << std::fixed << std::setprecision(3) << now.GetSeconds();

    uint32_t col = 0;
    for (const auto& entry : ordered)
    {
        g_outFile << "\t" << std::setprecision(4) << entry.second.second;
        ++col;
    }
    while (col < g_numFlows)
    {
        g_outFile << "\t0.0000";
        ++col;
    }
    g_outFile << "\n";

    if (now + g_sampleInterval <= stopTime)
    {
        Simulator::Schedule(g_sampleInterval, &SampleThroughput, stopTime);
    }
}

/**
 * @brief Entry point for the CAKE Figure 3 validation simulation.
 * @param argc Argument count.
 * @param argv Argument vector.
 * @return 0 on success.
 */
int
main(int argc, char* argv[])
{
    std::string outputFile = "cake-validation.dat";
    DataRate bottleneckRate("10Mbps");
    // Bandwidth=0 disables CAKE's software shaper.  The PointToPoint link
    // at bottleneckRate is the physical bottleneck; CAKE's DRR scheduler
    // alone enforces per-flow fairness, which is what Figure 3 validates.
    DataRate cakeBandwidth("0bps");
    Time bottleneckDelay("5ms");
    Time accessDelay("1ms");
    DataRate accessRate("100Mbps");
    Time simStop = Seconds(62);
    uint32_t segSize = 1448;

    CommandLine cmd(__FILE__);
    cmd.AddValue("output", "Output .dat filename", outputFile);
    cmd.AddValue("bottleneckRate", "Bottleneck link rate", bottleneckRate);
    cmd.AddValue("cakeBandwidth", "CAKE shaper bandwidth (0 = disabled)", cakeBandwidth);
    cmd.AddValue("simStop", "Simulation stop time", simStop);
    cmd.Parse(argc, argv);

    LogComponentEnable("CakeFig3", LOG_LEVEL_INFO);

    NodeContainer sources;
    sources.Create(3);
    NodeContainer routers;
    routers.Create(2);
    NodeContainer sink;
    sink.Create(1);

    PointToPointHelper accessHelper;
    accessHelper.SetDeviceAttribute("DataRate", DataRateValue(accessRate));
    accessHelper.SetChannelAttribute("Delay", TimeValue(accessDelay));
    accessHelper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("100p"));

    std::vector<NetDeviceContainer> accessDevs(3);
    for (uint32_t i = 0; i < 3; ++i)
    {
        accessDevs[i] = accessHelper.Install(sources.Get(i), routers.Get(0));
    }

    PointToPointHelper bottleneckHelper;
    bottleneckHelper.SetDeviceAttribute("DataRate", DataRateValue(bottleneckRate));
    bottleneckHelper.SetChannelAttribute("Delay", TimeValue(bottleneckDelay));
    // Small device queue so CakeQueueDisc is the dominant buffer.
    bottleneckHelper.SetQueue("ns3::DropTailQueue<Packet>", "MaxSize", StringValue("100p"));

    NetDeviceContainer bottleneckDevs = bottleneckHelper.Install(routers.Get(0), routers.Get(1));

    NetDeviceContainer egressDevs = accessHelper.Install(routers.Get(1), sink.Get(0));

    InternetStackHelper internet;
    internet.Install(sources);
    internet.Install(routers);
    internet.Install(sink);

    TrafficControlHelper tch;
    tch.SetRootQueueDisc("ns3::CakeQueueDisc",
                         "Bandwidth",
                         DataRateValue(cakeBandwidth),
                         "DiffServMode",
                         UintegerValue(0)); // besteffort = single tin
    tch.Install(bottleneckDevs.Get(0));

    Ipv4AddressHelper ipv4;

    for (uint32_t i = 0; i < 3; ++i)
    {
        std::ostringstream base;
        base << "10.1." << (i + 1) << ".0";
        ipv4.SetBase(base.str().c_str(), "255.255.255.252");
        ipv4.Assign(accessDevs[i]);
    }

    ipv4.SetBase("10.2.0.0", "255.255.255.252");
    ipv4.Assign(bottleneckDevs);

    ipv4.SetBase("10.3.0.0", "255.255.255.252");
    Ipv4InterfaceContainer egressIfaces = ipv4.Assign(egressDevs);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Address sinkLocalAddr(InetSocketAddress(Ipv4Address::GetAny(), SINK_PORT));
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", sinkLocalAddr);
    ApplicationContainer sinkApp = sinkHelper.Install(sink.Get(0));
    sinkApp.Start(Seconds(0.0));
    sinkApp.Stop(simStop);

    Ipv4Address sinkAddr = egressIfaces.GetAddress(1);

    /** @brief Staggered flow start/stop specification. */
    struct FlowSpec
    {
        uint32_t srcNode; //!< Source node index.
        Time start;       //!< Application start time.
        Time stop;        //!< Application stop time.
    };

    const std::vector<FlowSpec> flows = {
        {0, Seconds(0.0), Seconds(60.0)},
        {1, Seconds(10.0), Seconds(60.0)},
        {2, Seconds(20.0), Seconds(60.0)},
    };

    for (const auto& fs : flows)
    {
        BulkSendHelper bulkHelper("ns3::TcpSocketFactory", InetSocketAddress(sinkAddr, SINK_PORT));
        bulkHelper.SetAttribute("MaxBytes", UintegerValue(0));
        bulkHelper.SetAttribute("SendSize", UintegerValue(segSize));
        ApplicationContainer app = bulkHelper.Install(sources.Get(fs.srcNode));
        app.Start(fs.start);
        app.Stop(fs.stop);
    }

    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(segSize));
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(1 << 20));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(1 << 20));

    g_monitor = g_flowMonHelper.InstallAll();
    g_classifier = DynamicCast<Ipv4FlowClassifier>(g_flowMonHelper.GetClassifier());

    g_outFile.open(outputFile);
    if (!g_outFile.is_open())
    {
        NS_FATAL_ERROR("Cannot open output file: " << outputFile);
    }
    g_outFile << "# Time_s\tFlow1_Mbps\tFlow2_Mbps\tFlow3_Mbps\n";

    // Start sampling at 2s to avoid TCP startup noise.
    // First sample sets baseline; subsequent samples give clean 1s throughput.
    Simulator::Schedule(Seconds(2.0), &SampleThroughput, simStop);

    Simulator::Stop(simStop);
    NS_LOG_INFO("Starting validation simulation …");
    Simulator::Run();

    g_monitor->SerializeToXmlFile("cake-validation-flowmon.xml", true, true);
    NS_LOG_INFO("Results written to " << outputFile);
    g_outFile.close();
    Simulator::Destroy();

    return 0;
}
