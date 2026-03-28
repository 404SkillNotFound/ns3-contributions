/*
 * Copyright (c) 2026 Shivang Upadhyay
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#ifndef CAKE_QUEUE_DISC_H
#define CAKE_QUEUE_DISC_H

#include "queue-disc.h"

#include "ns3/data-rate.h"
#include "ns3/nstime.h"
#include "ns3/random-variable-stream.h"
#include "ns3/tag.h"

#include <list>
#include <vector>

namespace ns3
{

/**
 * @brief Packet tag carrying the simulation time at which a packet was enqueued.
 *
 * DoEnqueue() stamps every packet with this tag.  DoDequeue() peeks it,
 * computes sojourn = Now() - enqueueTime, and stores the result in
 * CakeFlow::sojournTime.
 */
class CakeSojournTag : public Tag
{
  public:
    /**
     * @brief Get the TypeId for CakeSojournTag.
     * @return The TypeId.
     */
    static TypeId GetTypeId()
    {
        static TypeId tid = TypeId("ns3::CakeSojournTag")
                                .SetParent<Tag>()
                                .SetGroupName("TrafficControl")
                                .AddConstructor<CakeSojournTag>();
        return tid;
    }

    TypeId GetInstanceTypeId() const override
    {
        return GetTypeId();
    }

    /**
     * @brief Serialised size: one int64 nanosecond timestamp.
     * @return The serialized size in bytes.
     */
    uint32_t GetSerializedSize() const override
    {
        return 8;
    }

    /**
     * @brief Serialise the tag content into a buffer.
     * @param buf The buffer to write to.
     */
    void Serialize(TagBuffer buf) const override
    {
        buf.WriteU64(static_cast<uint64_t>(m_enqueueTime.GetNanoSeconds()));
    }

    /**
     * @brief Deserialise the tag content from a buffer.
     * @param buf The buffer to read from.
     */
    void Deserialize(TagBuffer buf) override
    {
        m_enqueueTime = NanoSeconds(static_cast<int64_t>(buf.ReadU64()));
    }

    /**
     * @brief Print the tag content.
     * @param os The output stream to print to.
     */
    void Print(std::ostream& os) const override
    {
        os << "CakeSojournTag enqueueTime=" << m_enqueueTime.GetNanoSeconds() << "ns";
    }

    /**
     * @brief Set the enqueue timestamp.
     * @param t The simulation time to set as enqueue time.
     */
    void SetEnqueueTime(Time t)
    {
        m_enqueueTime = t;
    }

    /**
     * @brief Retrieve the enqueue timestamp.
     * @return The simulation time when the packet was enqueued.
     */
    Time GetEnqueueTime() const
    {
        return m_enqueueTime;
    }

  private:
    Time m_enqueueTime{Seconds(0)}; //!< Wall-clock time of enqueue.
};

/**
 * @ingroup traffic-control
 * @brief CAKE (Common Applications Kept Enhanced) Queue Discipline.
 *
 * Implements the four framework pillars: rate-based shaping, 8-way flow hashing,
 * per-host DRR fairness, and DiffServ priority tins [cite: 21-25, 94].
 * DSCP-to-tin classification is handled via PacketFilter subclasses[cite: 96].
 */
class CakeQueueDisc : public QueueDisc
{
  public:
    /** Maximum number of DiffServ tins. */
    static const uint32_t CAKE_MAX_TINS = 4;
    /** Number of ways in the set-associative hash table. */
    static const uint32_t CAKE_SET_WAYS = 8;

    /** @brief DiffServ preset modes. */
    enum DiffServMode : uint32_t
    {
        DIFFSERV_BESTEFFORT = 0, //!< Single best-effort tin.
        DIFFSERV_DIFFSERV3 = 3,  //!< Three-tin bulk/BE/latency split.
        DIFFSERV_DIFFSERV4 = 4,  //!< Four-tin 802.11e-inspired split.
    };

    /** @brief Host isolation modes (Algorithm 2 of the paper). */
    enum IsolationMode : uint32_t
    {
        ISOLATION_NONE = 0,   //!< Flow-level fairness only.
        ISOLATION_SRC = 1,    //!< Isolate by source host.
        ISOLATION_DST = 2,    //!< Isolate by destination host.
        ISOLATION_TRIPLE = 3, //!< Isolate by max of src and dst refcount.
    };

    /** @brief TCP ACK filter aggressiveness. Not yet implemented. */
    enum AckFilterMode : uint32_t
    {
        ACK_FILTER_NONE = 0,         //!< No ACK filtering.
        ACK_FILTER_CONSERVATIVE = 1, //!< Keep at least two ACKs per flow.
        ACK_FILTER_AGGRESSIVE = 2,   //!< Keep only the newest ACK per flow.
    };

    /**
     * @brief Per-flow state stored in the 8-way hash table.
     */
    struct CakeFlow
    {
        uint32_t flowHash{0};     //!< Hash of the 5-tuple.
        uint32_t srcHash{0};      //!< Source host bucket index.
        uint32_t dstHash{0};      //!< Destination host bucket index.
        int32_t deficit{0};       //!< DRR deficit counter (signed).
        uint32_t quantum{1514};   //!< DRR quantum in bytes.
        uint32_t backlogBytes{0}; //!< Bytes currently queued for this flow.
        uint8_t tinIndex{0};      //!< DiffServ tin this flow belongs to.
        bool active{false};       //!< Whether the flow has queued packets.

        // Most-recently measured sojourn time for this flow.
        // Populated by DoDequeue() using CakeSojournTag.
        Time sojournTime{Seconds(0)}; //!< Last measured queue sojourn time.
    };

    /**
     * @brief Per-IP reference counts used for host isolation.
     */
    struct CakeHostBucket
    {
        uint32_t refcntSrc{0}; //!< Active flows sourced from this host.
        uint32_t refcntDst{0}; //!< Active flows destined to this host.
    };

    /**
     * @brief One DiffServ priority tier with its own rate and AQM state.
     */
    struct CakeTin
    {
        DataRate targetRate;                    //!< Allocated bandwidth for this tin.
        Time virtualClock{Seconds(0)};          //!< Virtual clock for shaper gate.
        Time cobaltTarget{MicroSeconds(5000)};  //!< COBALT sojourn target.
        Time cobaltInterval{MilliSeconds(100)}; //!< COBALT control interval.
        uint32_t backlogBytes{0}; //!< Total bytes queued across all flows in this tin.
        uint32_t quantum{1514};   //!< DRR quantum for flows in this tin.

        // COBALT (CoDel + BLUE) AQM state — one instance per tin.
        bool cobaltDropping{false}; //!< CoDel dropping state active.
        Time cobaltFirstAboveTime{
            Seconds(0)};                 //!< When sojourn first exceeded target; 0 = not above.
        Time cobaltDropNext{Seconds(0)}; //!< Scheduled time of next CoDel drop.
        uint32_t cobaltCount{0};         //!< Drop count for CoDel control law.
        double blueProb{0.0};            //!< BLUE drop probability in [0, 1].
        Time blueTimer{Seconds(0)};      //!< Last BLUE probability update time.
    };

    /**
     * @brief Get the TypeId for this class.
     * @return The TypeId.
     */
    static TypeId GetTypeId();

    CakeQueueDisc();
    ~CakeQueueDisc() override;

  protected:
    /**
     * @brief Enqueue a packet into the CAKE queue disc.
     * @param item The packet to enqueue.
     * @return True if the packet was enqueued, false otherwise.
     */
    bool DoEnqueue(Ptr<QueueDiscItem> item) override;

    /**
     * @brief Dequeue a packet from the CAKE queue disc.
     * @return The dequeued packet, or nullptr if none available.
     */
    Ptr<QueueDiscItem> DoDequeue() override;

    /**
     * @brief Check the configuration of the CAKE queue disc.
     * @return True if the configuration is valid, false otherwise.
     */
    bool CheckConfig() override;

    /** @brief Initialize the CAKE queue disc parameters. */
    void InitializeParams() override;

  private:
    /**
     * @brief Compute a flow hash using QueueDiscItem::Hash().
     * @param item The packet to hash.
     * @return A flow bucket index in [0, m_flows).
     */
    uint32_t FlowHash(Ptr<const QueueDiscItem> item) const;

    /**
     * @brief Hash a 32-bit value into a host bucket index.
     * @param val Input value.
     * @return A host bucket index in [0, m_flows).
     */
    uint32_t HostHash(uint32_t val) const;

    /**
     * @brief Find or claim a flow slot using 8-way set-associative lookup.
     * @param flowHash Flow hash value.
     * @param srcHash Source host hash.
     * @param dstHash Destination host hash.
     * @param tin Tin index for a newly initialised slot.
     * @return Index into m_flowBuckets.
     */
    uint32_t SetAssocHashLookup(uint32_t flowHash, uint32_t srcHash, uint32_t dstHash, uint8_t tin);

    /**
     * @brief Initialise a flow slot and increment host refcounts.
     * @param idx Slot index.
     * @param flowHash Flow hash.
     * @param srcHash Source host hash.
     * @param dstHash Destination host hash.
     * @param tin Tin index.
     */
    void InitFlowSlot(uint32_t idx,
                      uint32_t flowHash,
                      uint32_t srcHash,
                      uint32_t dstHash,
                      uint8_t tin);

    /**
     * @brief Map a DSCP value to a tin index for the current DiffServMode.
     * @param dscp 6-bit DSCP value.
     * @return Tin index.
     */
    uint8_t DscpToTin(uint8_t dscp) const;

    /**
     * @brief Return the DRR quantum scaled by host load (Algorithm 2).
     * @param flowIdx Flow bucket index.
     * @return Effective quantum in bytes.
     */
    uint32_t GetEffectiveQuantum(uint32_t flowIdx) const;

    /** @brief Configure per-tin bandwidth fractions for the current DiffServMode. */
    void SetupTins();

    /**
     * @brief Decrement host refcounts when a flow becomes idle.
     * @param flowIdx Flow bucket index.
     */
    void ReleaseHostRefs(uint32_t flowIdx);

    /** @brief Called by Simulator::Schedule when the shaper gate opens. */
    void ShaperWakeup();

    /**
     * @brief Compute the next CoDel drop time using the control law.
     * @param t Base time (last drop or now).
     * @param interval COBALT control interval for this tin.
     * @param count Current drop count.
     * @return Scheduled time for the next drop event.
     */
    Time CobaltControlLaw(Time t, Time interval, uint32_t count) const;

    /**
     * @brief Run the COBALT state machine for one dequeued packet.
     *
     * Updates per-tin CoDel and BLUE state.  Attempts ECN marking before
     * dropping.  Must be called after the tin backlog has been decremented.
     *
     * @param tin  Tin index.
     * @param sojourn Measured sojourn time of the packet.
     * @param item The dequeued packet (may be ECN-marked in-place).
     * @return True if the packet should be dropped.
     */
    bool CobaltShouldDrop(uint32_t tin, Time sojourn, Ptr<QueueDiscItem> item);

    DataRate m_bandwidth;     //!< Target shaper rate.
    Time m_target;            //!< AQM sojourn target.
    Time m_interval;          //!< AQM control interval.
    uint32_t m_flows;         //!< Number of flow hash buckets.
    uint32_t m_overhead;      //!< Per-packet framing overhead in bytes.
    uint32_t m_diffServMode;  //!< DiffServ preset (0, 3, or 4).
    uint32_t m_isolationMode; //!< Host isolation mode.
    uint32_t m_ackFilterMode; //!< ACK filter aggressiveness.
    uint32_t m_perturbation;  //!< Hash perturbation seed.

    uint32_t m_numTins;     //!< Number of active tins.
    Time m_tNext;           //!< Earliest time the shaper allows dequeue.
    bool m_shaperScheduled; //!< Whether a ShaperWakeup event is pending.

    std::vector<CakeFlow> m_flowBuckets;         //!< Per-flow state table.
    std::vector<CakeHostBucket> m_srcHosts;      //!< Source host reference counts.
    std::vector<CakeHostBucket> m_dstHosts;      //!< Destination host reference counts.
    std::vector<CakeTin> m_tins;                 //!< Per-tin state.
    std::vector<std::list<uint32_t>> m_newFlows; //!< Sparse-flow lists per tin.
    std::vector<std::list<uint32_t>> m_oldFlows; //!< Heavy-flow lists per tin.

    Ptr<UniformRandomVariable> m_uv; //!< RNG used to auto-generate perturbation seed.
};

} // namespace ns3

#endif /* CAKE_QUEUE_DISC_H */
