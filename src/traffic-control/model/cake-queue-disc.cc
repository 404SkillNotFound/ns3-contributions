/*
 * Copyright (c) 2026 Shivang Upadhyay
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "cake-queue-disc.h"

#include "ns3/drop-tail-queue.h"
#include "ns3/log.h"
#include "ns3/object-factory.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/uinteger.h"

#include <algorithm>

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("CakeQueueDisc");
NS_OBJECT_ENSURE_REGISTERED(CakeQueueDisc);

/** Drop reason used when the internal queue is full. */
static constexpr const char* LIMIT_EXCEEDED_DROP = "Queue disc limit exceeded";

TypeId
CakeQueueDisc::GetTypeId()
{
    static TypeId tid =
        TypeId("ns3::CakeQueueDisc")
            .SetParent<QueueDisc>()
            .SetGroupName("TrafficControl")
            .AddConstructor<CakeQueueDisc>()
            .AddAttribute("Bandwidth",
                          "Target shaper bandwidth (0 = disabled).",
                          DataRateValue(DataRate(0)),
                          MakeDataRateAccessor(&CakeQueueDisc::m_bandwidth),
                          MakeDataRateChecker())
            .AddAttribute("Target",
                          "AQM target sojourn time.",
                          TimeValue(MicroSeconds(5000)),
                          MakeTimeAccessor(&CakeQueueDisc::m_target),
                          MakeTimeChecker())
            .AddAttribute("Interval",
                          "AQM control interval.",
                          TimeValue(MilliSeconds(100)),
                          MakeTimeAccessor(&CakeQueueDisc::m_interval),
                          MakeTimeChecker())
            .AddAttribute("Flows",
                          "Hash table size — must be a power of 2 and >= 8.",
                          UintegerValue(1024),
                          MakeUintegerAccessor(&CakeQueueDisc::m_flows),
                          MakeUintegerChecker<uint32_t>(8, 65536))
            .AddAttribute("Overhead",
                          "Per-packet overhead bytes added for framing compensation.",
                          UintegerValue(0),
                          MakeUintegerAccessor(&CakeQueueDisc::m_overhead),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("DiffServMode",
                          "DiffServ preset: 0=besteffort 3=diffserv3 4=diffserv4.",
                          UintegerValue(static_cast<uint32_t>(DIFFSERV_BESTEFFORT)),
                          MakeUintegerAccessor(&CakeQueueDisc::m_diffServMode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("IsolationMode",
                          "Host isolation: 0=none 1=src 2=dst 3=triple.",
                          UintegerValue(static_cast<uint32_t>(ISOLATION_NONE)),
                          MakeUintegerAccessor(&CakeQueueDisc::m_isolationMode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("AckFilterMode",
                          "TCP ACK filter: 0=none 1=conservative 2=aggressive.",
                          UintegerValue(static_cast<uint32_t>(ACK_FILTER_NONE)),
                          MakeUintegerAccessor(&CakeQueueDisc::m_ackFilterMode),
                          MakeUintegerChecker<uint32_t>())
            .AddAttribute("Perturbation",
                          "Hash perturbation seed (0 = auto-randomise).",
                          UintegerValue(0),
                          MakeUintegerAccessor(&CakeQueueDisc::m_perturbation),
                          MakeUintegerChecker<uint32_t>());
    return tid;
}

CakeQueueDisc::CakeQueueDisc()
    : m_flows(1024),
      m_overhead(0),
      m_diffServMode(static_cast<uint32_t>(DIFFSERV_BESTEFFORT)),
      m_isolationMode(static_cast<uint32_t>(ISOLATION_NONE)),
      m_ackFilterMode(static_cast<uint32_t>(ACK_FILTER_NONE)),
      m_perturbation(0),
      m_numTins(1),
      m_tNext(Seconds(0)),
      m_shaperScheduled(false)
{
    NS_LOG_FUNCTION(this);
    m_uv = CreateObject<UniformRandomVariable>();
}

CakeQueueDisc::~CakeQueueDisc()
{
    NS_LOG_FUNCTION(this);
}

uint32_t
CakeQueueDisc::FlowHash(Ptr<const QueueDiscItem> item) const
{
    return item->Hash(m_perturbation) % m_flows;
}

uint32_t
CakeQueueDisc::HostHash(uint32_t val) const
{
    uint32_t h = m_perturbation ^ val;
    h ^= h >> 16;
    h *= 0x45d9f3b;
    h ^= h >> 16;
    return h % m_flows;
}

uint32_t
CakeQueueDisc::SetAssocHashLookup(uint32_t flowHash,
                                  uint32_t srcHash,
                                  uint32_t dstHash,
                                  uint8_t tin)
{
    uint32_t setBase = (flowHash / CAKE_SET_WAYS) * CAKE_SET_WAYS;
    if (setBase + CAKE_SET_WAYS > m_flows)
    {
        setBase = m_flows - CAKE_SET_WAYS;
    }

    for (uint32_t w = 0; w < CAKE_SET_WAYS; ++w)
    {
        uint32_t idx = setBase + w;
        if (m_flowBuckets[idx].active && m_flowBuckets[idx].flowHash == flowHash)
        {
            return idx;
        }
    }

    for (uint32_t w = 0; w < CAKE_SET_WAYS; ++w)
    {
        uint32_t idx = setBase + w;
        if (!m_flowBuckets[idx].active)
        {
            InitFlowSlot(idx, flowHash, srcHash, dstHash, tin);
            return idx;
        }
    }

    uint32_t evict = setBase;
    uint32_t minBytes = m_flowBuckets[setBase].backlogBytes;
    for (uint32_t w = 1; w < CAKE_SET_WAYS; ++w)
    {
        uint32_t idx = setBase + w;
        if (m_flowBuckets[idx].backlogBytes < minBytes)
        {
            minBytes = m_flowBuckets[idx].backlogBytes;
            evict = idx;
        }
    }

    ReleaseHostRefs(evict);
    InitFlowSlot(evict, flowHash, srcHash, dstHash, tin);
    return evict;
}

void
CakeQueueDisc::InitFlowSlot(uint32_t idx,
                            uint32_t flowHash,
                            uint32_t srcHash,
                            uint32_t dstHash,
                            uint8_t tin)
{
    CakeFlow& f = m_flowBuckets[idx];
    f.flowHash = flowHash;
    f.srcHash = srcHash;
    f.dstHash = dstHash;
    f.tinIndex = tin;
    f.deficit = 0;
    f.backlogBytes = 0;
    f.active = false;
    f.quantum = m_tins[tin].quantum;
    f.sojournTime = Seconds(0);

    if (m_isolationMode != static_cast<uint32_t>(ISOLATION_NONE))
    {
        m_srcHosts[srcHash].refcntSrc++;
        m_dstHosts[dstHash].refcntDst++;
    }
}

uint8_t
CakeQueueDisc::DscpToTin(uint8_t dscp) const
{
    if (m_diffServMode == static_cast<uint32_t>(DIFFSERV_BESTEFFORT))
    {
        return 0;
    }
    if (m_diffServMode == static_cast<uint32_t>(DIFFSERV_DIFFSERV3))
    {
        if (dscp == 8)
        {
            return 0;
        }
        if (dscp == 46 || dscp == 44 || dscp == 48 || dscp == 56)
        {
            return 2;
        }
        return 1;
    }
    if (m_diffServMode == static_cast<uint32_t>(DIFFSERV_DIFFSERV4))
    {
        if (dscp == 8 || dscp == 0)
        {
            return 0;
        }
        if (dscp == 46 || dscp == 44 || dscp == 48 || dscp == 56)
        {
            return 3;
        }
        if (dscp >= 26 && dscp <= 38)
        {
            return 2;
        }
        return 1;
    }
    return 0;
}

uint32_t
CakeQueueDisc::GetEffectiveQuantum(uint32_t flowIdx) const
{
    if (m_isolationMode == static_cast<uint32_t>(ISOLATION_NONE))
    {
        return m_flowBuckets[flowIdx].quantum;
    }

    const CakeFlow& f = m_flowBuckets[flowIdx];
    uint32_t hostLoad = 1;

    if (m_isolationMode == static_cast<uint32_t>(ISOLATION_SRC))
    {
        hostLoad = std::max(m_srcHosts[f.srcHash].refcntSrc, 1u);
    }
    else if (m_isolationMode == static_cast<uint32_t>(ISOLATION_DST))
    {
        hostLoad = std::max(m_dstHosts[f.dstHash].refcntDst, 1u);
    }
    else
    {
        hostLoad = std::max({m_srcHosts[f.srcHash].refcntSrc, m_dstHosts[f.dstHash].refcntDst, 1u});
    }

    uint32_t q = f.quantum / hostLoad;
    return (q == 0) ? 1 : q;
}

void
CakeQueueDisc::SetupTins()
{
    m_tins.assign(m_numTins, CakeTin());

    if (m_numTins == 1)
    {
        m_tins[0].targetRate = m_bandwidth;
        m_tins[0].quantum = 1514;
        return;
    }

    auto bw = static_cast<double>(m_bandwidth.GetBitRate());

    if (m_numTins == 3)
    {
        m_tins[0].targetRate = DataRate(static_cast<uint64_t>(bw / 16.0));
        m_tins[2].targetRate = DataRate(static_cast<uint64_t>(bw / 4.0));
        m_tins[1].targetRate = DataRate(static_cast<uint64_t>(
            bw - m_tins[0].targetRate.GetBitRate() - m_tins[2].targetRate.GetBitRate()));
        m_tins[2].cobaltTarget = MicroSeconds(1000);
    }
    else if (m_numTins == 4)
    {
        m_tins[0].targetRate = DataRate(static_cast<uint64_t>(bw / 16.0));
        m_tins[1].targetRate = DataRate(static_cast<uint64_t>(bw * 0.5));
        m_tins[2].targetRate = DataRate(static_cast<uint64_t>(bw * 0.25));
        m_tins[3].targetRate = DataRate(static_cast<uint64_t>(bw * 0.1875));
        m_tins[3].cobaltTarget = MicroSeconds(500);
    }

    for (auto& t : m_tins)
    {
        t.quantum = 1514;
    }
}

bool
CakeQueueDisc::CheckConfig()
{
    NS_LOG_FUNCTION(this);

    if (m_target >= m_interval)
    {
        NS_LOG_ERROR("CakeQueueDisc: Target must be < Interval.");
        return false;
    }

    if (m_flows < CAKE_SET_WAYS || (m_flows & (m_flows - 1)) != 0)
    {
        NS_LOG_ERROR("CakeQueueDisc: Flows must be a power of 2 and >= " << CAKE_SET_WAYS);
        return false;
    }

    uint32_t dsm = m_diffServMode;
    if (dsm != 0 && dsm != 3 && dsm != 4)
    {
        NS_LOG_ERROR("CakeQueueDisc: DiffServMode must be 0, 3, or 4.");
        return false;
    }

    return true;
}

void
CakeQueueDisc::InitializeParams()
{
    NS_LOG_FUNCTION(this);

    switch (m_diffServMode)
    {
    case DIFFSERV_BESTEFFORT:
        m_numTins = 1;
        break;
    case DIFFSERV_DIFFSERV3:
        m_numTins = 3;
        break;
    case DIFFSERV_DIFFSERV4:
        m_numTins = 4;
        break;
    default:
        m_numTins = 1;
        break;
    }

    SetupTins();

    m_flowBuckets.assign(m_flows, CakeFlow());
    m_srcHosts.assign(m_flows, CakeHostBucket());
    m_dstHosts.assign(m_flows, CakeHostBucket());
    m_newFlows.assign(m_numTins, std::list<uint32_t>());
    m_oldFlows.assign(m_numTins, std::list<uint32_t>());

    ObjectFactory factory;
    factory.SetTypeId("ns3::DropTailQueue<QueueDiscItem>");
    factory.Set("MaxSize", StringValue("1000p"));
    for (uint32_t i = 0; i < m_flows; ++i)
    {
        AddInternalQueue(factory.Create<QueueDisc::InternalQueue>());
    }

    m_tNext = Simulator::Now();
    m_shaperScheduled = false;

    if (m_perturbation == 0)
    {
        m_perturbation = static_cast<uint32_t>(m_uv->GetInteger(1, 0xffffffff));
    }
}

bool
CakeQueueDisc::DoEnqueue(Ptr<QueueDiscItem> item)
{
    NS_LOG_FUNCTION(this << item);

    // Stamp enqueue time for COBALT sojourn measurement.
    CakeSojournTag sojournTag;
    sojournTag.SetEnqueueTime(Simulator::Now());
    item->GetPacket()->AddPacketTag(sojournTag);
    NS_LOG_DEBUG("CakeQueueDisc::DoEnqueue stamped enqueueTime="
                 << Simulator::Now().GetNanoSeconds() << "ns");

    uint8_t tin = (m_numTins > 1) ? 1 : 0;
    int32_t filterResult = Classify(item);
    if (filterResult != PacketFilter::PF_NO_MATCH)
    {
        auto candidate = static_cast<uint8_t>(filterResult);
        if (candidate < m_numTins)
        {
            tin = candidate;
        }
    }

    uint32_t flowH = FlowHash(item);
    uint32_t srcH = HostHash(flowH);
    uint32_t dstH = HostHash(~flowH);
    uint32_t bucketIdx = SetAssocHashLookup(flowH, srcH, dstH, tin);
    CakeFlow& flow = m_flowBuckets[bucketIdx];

    if (!GetInternalQueue(bucketIdx)->Enqueue(item))
    {
        DropBeforeEnqueue(item, LIMIT_EXCEEDED_DROP);
        return false;
    }

    uint32_t pktSize = item->GetSize();
    flow.backlogBytes += pktSize;
    m_tins[tin].backlogBytes += pktSize;

    if (!flow.active)
    {
        flow.active = true;
        flow.tinIndex = tin;
        flow.deficit = static_cast<int32_t>(GetEffectiveQuantum(bucketIdx));
        m_newFlows[tin].push_back(bucketIdx);
    }

    if (m_bandwidth.GetBitRate() > 0 && Simulator::Now() < m_tNext && !m_shaperScheduled)
    {
        m_shaperScheduled = true;
        Simulator::Schedule(m_tNext - Simulator::Now(), &CakeQueueDisc::ShaperWakeup, this);
    }

    return true;
}

Ptr<QueueDiscItem>
CakeQueueDisc::DoDequeue()
{
    NS_LOG_FUNCTION(this);

    if (m_bandwidth.GetBitRate() > 0 && Simulator::Now() < m_tNext)
    {
        if (!m_shaperScheduled)
        {
            m_shaperScheduled = true;
            Simulator::Schedule(m_tNext - Simulator::Now(), &CakeQueueDisc::ShaperWakeup, this);
        }
        return nullptr;
    }

    for (auto t = static_cast<int>(m_numTins) - 1; t >= 0; --t)
    {
        auto tin = static_cast<uint32_t>(t);
        if (m_tins[tin].backlogBytes == 0)
        {
            continue;
        }

        std::list<uint32_t>* lists[2] = {&m_newFlows[tin], &m_oldFlows[tin]};
        for (auto* fl : lists)
        {
            for (auto it = fl->begin(); it != fl->end();)
            {
                uint32_t fi = *it;
                CakeFlow& flow = m_flowBuckets[fi];

                if (flow.deficit <= 0)
                {
                    flow.deficit += static_cast<int32_t>(GetEffectiveQuantum(fi));
                    m_oldFlows[tin].splice(m_oldFlows[tin].end(), *fl, it++);
                    continue;
                }

                Ptr<QueueDiscItem> pkt = GetInternalQueue(fi)->Dequeue();
                if (!pkt)
                {
                    flow.active = false;
                    it = fl->erase(it);
                    ReleaseHostRefs(fi);
                    continue;
                }

                // Compute per-packet sojourn time and store in flow state.
                CakeSojournTag sojournTag;
                if (pkt->GetPacket()->PeekPacketTag(sojournTag))
                {
                    Time sojourn = Simulator::Now() - sojournTag.GetEnqueueTime();
                    if (sojourn < Seconds(0))
                    {
                        sojourn = Seconds(0);
                    }
                    flow.sojournTime = sojourn;
                    NS_LOG_DEBUG("CakeQueueDisc::DoDequeue flow="
                                 << fi << " sojourn=" << sojourn.GetNanoSeconds() << "ns  target="
                                 << m_tins[tin].cobaltTarget.GetNanoSeconds() << "ns");
                }

                uint32_t sz = pkt->GetSize();
                flow.deficit -= static_cast<int32_t>(sz);
                flow.backlogBytes = (flow.backlogBytes >= sz) ? flow.backlogBytes - sz : 0;
                m_tins[tin].backlogBytes =
                    (m_tins[tin].backlogBytes >= sz) ? m_tins[tin].backlogBytes - sz : 0;

                if (flow.backlogBytes == 0)
                {
                    flow.active = false;
                    it = fl->erase(it);
                    ReleaseHostRefs(fi);
                }

                if (m_bandwidth.GetBitRate() > 0)
                {
                    auto adjBytes = static_cast<uint64_t>(sz + m_overhead);
                    auto delaySec = static_cast<double>(adjBytes * 8) /
                                    static_cast<double>(m_bandwidth.GetBitRate());
                    m_tNext = Simulator::Now() + Seconds(delaySec);
                }

                return pkt;
            }
        }
    }

    return nullptr;
}

void
CakeQueueDisc::ReleaseHostRefs(uint32_t flowIdx)
{
    if (m_isolationMode == static_cast<uint32_t>(ISOLATION_NONE))
    {
        return;
    }

    const CakeFlow& f = m_flowBuckets[flowIdx];
    if (m_srcHosts[f.srcHash].refcntSrc > 0)
    {
        m_srcHosts[f.srcHash].refcntSrc--;
    }
    if (m_dstHosts[f.dstHash].refcntDst > 0)
    {
        m_dstHosts[f.dstHash].refcntDst--;
    }
}

void
CakeQueueDisc::ShaperWakeup()
{
    NS_LOG_FUNCTION(this);
    m_shaperScheduled = false;
    Run();
}

} // namespace ns3
