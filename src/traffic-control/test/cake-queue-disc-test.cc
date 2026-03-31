/*
 * Copyright (c) 2026 Shivang Upadhyay
 *
 * SPDX-License-Identifier: GPL-2.0-only
 */

#include "ns3/cake-queue-disc.h"
#include "ns3/data-rate.h"
#include "ns3/packet.h"
#include "ns3/queue-disc.h"
#include "ns3/simulator.h"
#include "ns3/string.h"
#include "ns3/test.h"
#include "ns3/uinteger.h"

using namespace ns3;

/**
 * @ingroup traffic-control-test
 * @brief Minimal QueueDiscItem that lives entirely in the test binary.
 */
class CakeTestItem : public QueueDiscItem
{
  public:
    /**
     * @brief Construct a test item.
     * @param p        Packet payload.
     * @param addr     Destination address.
     * @param protocol EtherType / protocol number.
     */
    CakeTestItem(Ptr<Packet> p, const Address& addr, uint16_t protocol)
        : QueueDiscItem(p, addr, protocol)
    {
    }

    void AddHeader() override
    {
    }

    bool Mark() override
    {
        return false;
    }
};

/**
 * @brief Helper that creates a CakeTestItem of the given size.
 * @param bytes The size of the packet to create.
 * @return A pointer to the created QueueDiscItem.
 */
static Ptr<QueueDiscItem>
MakeItem(uint32_t bytes = 512)
{
    auto pkt = Create<Packet>(bytes);
    return Create<CakeTestItem>(pkt, Address(), 0x0800);
}

/**
 * @brief A QueueDiscItem whose Mark() returns true, for ECN marking tests.
 */
class CakeEcnTestItem : public QueueDiscItem
{
  public:
    /**
     * @brief Construct an ECN test item.
     * @param p        Packet payload.
     * @param addr     Destination address.
     * @param protocol EtherType / protocol number.
     */
    CakeEcnTestItem(Ptr<Packet> p, const Address& addr, uint16_t protocol)
        : QueueDiscItem(p, addr, protocol)
    {
    }

    void AddHeader() override
    {
    }

    bool Mark() override
    {
        return true;
    }
};

/**
 * @brief Helper that creates a CakeEcnTestItem of the given size.
 * @param bytes The size of the packet to create.
 * @return A pointer to the created QueueDiscItem with ECN support.
 */
static Ptr<QueueDiscItem>
MakeEcnItem(uint32_t bytes = 512)
{
    auto pkt = Create<Packet>(bytes);
    return Create<CakeEcnTestItem>(pkt, Address(), 0x0800);
}

/**
 * @brief Verify Initialize() succeeds and creates the right number of queues.
 */
class CakeCheckConfigTest : public TestCase
{
  public:
    CakeCheckConfigTest()
        : TestCase("Verify CakeQueueDisc attribute validation")
    {
    }

  private:
    void DoRun() override
    {
        auto qd = CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth",
                                                            DataRateValue(DataRate("10Mbps")),
                                                            "DiffServMode",
                                                            UintegerValue(3));
        qd->Initialize();
        NS_TEST_EXPECT_MSG_EQ(qd->GetNInternalQueues(),
                              1024,
                              "Should create 1024 internal queues by default");
        Simulator::Destroy();
    }
};

/**
 * @brief Verify basic enqueue / dequeue with shaper disabled (Bandwidth = 0).
 */
class CakeBasicEnqueueDequeueTest : public TestCase
{
  public:
    CakeBasicEnqueueDequeueTest()
        : TestCase("Verify basic enqueue and dequeue cycle")
    {
    }

  private:
    void DoRun() override
    {
        auto qd =
            CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth", DataRateValue(DataRate(0)));
        qd->Initialize();

        NS_TEST_EXPECT_MSG_EQ(qd->Enqueue(MakeItem(512)), true, "Enqueue should succeed");
        NS_TEST_EXPECT_MSG_NE(qd->Dequeue(), nullptr, "Dequeue should return the packet");

        Simulator::Destroy();
    }
};

/**
 * @brief Verify DiffServ presets accept packets with shaper disabled.
 *
 * Bandwidth is set to 0 so no ShaperWakeup event is ever scheduled,
 * which means Simulator::Run() is never needed and Run() (which requires
 * a NetDevice send callback) is never called.
 */
class CakeDiffServModeTest : public TestCase
{
  public:
    CakeDiffServModeTest()
        : TestCase("Verify DiffServ mode configures correct number of tins")
    {
    }

  private:
    void DoRun() override
    {
        auto qd3 = CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth",
                                                             DataRateValue(DataRate(0)),
                                                             "DiffServMode",
                                                             UintegerValue(3));
        qd3->Initialize();
        NS_TEST_EXPECT_MSG_EQ(qd3->Enqueue(MakeItem()), true, "diffserv3 enqueue should succeed");
        NS_TEST_EXPECT_MSG_NE(qd3->Dequeue(), nullptr, "diffserv3 dequeue should succeed");
        Simulator::Destroy();

        auto qd4 = CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth",
                                                             DataRateValue(DataRate(0)),
                                                             "DiffServMode",
                                                             UintegerValue(4));
        qd4->Initialize();
        NS_TEST_EXPECT_MSG_EQ(qd4->Enqueue(MakeItem()), true, "diffserv4 enqueue should succeed");
        NS_TEST_EXPECT_MSG_NE(qd4->Dequeue(), nullptr, "diffserv4 dequeue should succeed");
        Simulator::Destroy();
    }
};

/**
 * @ingroup traffic-control-test
 * @brief Verify that the shaper timing gate blocks dequeue until the virtual clock allows.
 *
 * Exercises the timing gate by checking that the first packet is served at t=0 while
 * subsequent packets are blocked. End-to-end wakeup is validated in cake-fig3.
 */
class CakeShaperTest : public TestCase
{
  public:
    CakeShaperTest()
        : TestCase("Verify shaper virtual clock timing")
    {
    }

  private:
    void DoRun() override
    {
        // 1 Mbps → a 512-byte packet takes ~4 ms to drain; m_tNext will be
        // set to roughly t=0 + 4 ms after the first dequeue.
        auto qd = CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth",
                                                            DataRateValue(DataRate("1Mbps")));
        qd->Initialize();

        // Gate is open at t = 0: first packet must be served.
        qd->Enqueue(MakeItem(512));
        NS_TEST_EXPECT_MSG_NE(qd->Dequeue(), nullptr, "First packet dequeues at t=0");

        // Gate is now closed (m_tNext ≈ +4 ms): second packet must be held.
        // DoEnqueue will schedule a ShaperWakeup, but because we never call
        // Simulator::Run() that event is never dispatched and Run() is never
        // called on an unattached qdisc.
        qd->Enqueue(MakeItem(512));
        NS_TEST_EXPECT_MSG_EQ(qd->Dequeue(),
                              nullptr,
                              "Shaper must block dequeue immediately after first packet");

        // Destroy cancels any pending simulator events (including the
        // ShaperWakeup), so nothing fires after this point.
        Simulator::Destroy();
    }
};

/**
 * @brief Verify COBALT issues a hard drop when sojourn exceeds the control interval threshold.
 *
 * Two packets enqueued at t=0. Dequeue at t=200ms primes the CoDel firstAboveTime
 * to 300ms. Dequeue at t=400ms satisfies now >= firstAboveTime and cobaltDropNext,
 * entering dropping state. Mark() returns false so CobaltShouldDrop returns true.
 */
class CakeCobaltDropTest : public TestCase
{
  public:
    CakeCobaltDropTest()
        : TestCase("COBALT drops packet when Mark() unavailable and sojourn exceeds threshold")
    {
    }

  private:
    Ptr<CakeQueueDisc> m_qd; ///< The CakeQueueDisc instance to test.

    /** @brief Enqueue a 512-byte packet. */
    void DoEnqueue()
    {
        m_qd->Enqueue(MakeItem(512));
    }

    /** @brief Dequeue a packet to prime the COBALT interval. */
    void CheckPrime()
    {
        // sojourn=200ms > cobaltTarget=5ms; sets firstAboveTime=300ms, no drop yet.
        NS_TEST_EXPECT_MSG_NE(m_qd->Dequeue(), nullptr, "Prime dequeue should succeed");
    }

    /** @brief Dequeue a packet and verify it is dropped by COBALT. */
    void CheckDrop()
    {
        // sojourn=400ms, now >= firstAboveTime=300ms, cobaltDropNext=0: enters dropping state.
        // Mark() returns false so CobaltShouldDrop returns true — hard drop.
        NS_TEST_EXPECT_MSG_EQ(m_qd->Dequeue(), nullptr, "COBALT should drop and return nullptr");
        NS_TEST_EXPECT_MSG_EQ(m_qd->GetStats().nTotalDroppedPacketsAfterDequeue,
                              1,
                              "One post-dequeue drop expected");
    }

    void DoRun() override
    {
        m_qd = CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth", DataRateValue(DataRate(0)));
        m_qd->Initialize();

        Simulator::ScheduleNow(&CakeCobaltDropTest::DoEnqueue, this);
        Simulator::ScheduleNow(&CakeCobaltDropTest::DoEnqueue, this);
        Simulator::Schedule(MilliSeconds(200), &CakeCobaltDropTest::CheckPrime, this);
        Simulator::Schedule(MilliSeconds(400), &CakeCobaltDropTest::CheckDrop, this);

        Simulator::Run();
        Simulator::Destroy();
    }
};

/**
 * @brief Verify COBALT prefers ECN marking over dropping when Mark() succeeds.
 *
 * Mirrors CakeCobaltDropTest but the second packet is a CakeEcnTestItem.
 * At t=400ms COBALT fires, Mark() returns true, so CobaltShouldDrop returns false
 * and the packet is returned rather than dropped.
 */
class CakeCobaltEcnMarkTest : public TestCase
{
  public:
    CakeCobaltEcnMarkTest()
        : TestCase("COBALT marks via ECN instead of dropping when Mark() succeeds")
    {
    }

  private:
    Ptr<CakeQueueDisc> m_qd; ///< The CakeQueueDisc instance to test.

    /** @brief Enqueue a standard 512-byte packet. */
    void EnqueuePlain()
    {
        m_qd->Enqueue(MakeItem(512));
    }

    /** @brief Enqueue a 512-byte packet with ECN capability. */
    void EnqueueEcn()
    {
        m_qd->Enqueue(MakeEcnItem(512));
    }

    /** @brief Dequeue a packet to prime the COBALT interval. */
    void CheckPrime()
    {
        NS_TEST_EXPECT_MSG_NE(m_qd->Dequeue(), nullptr, "Prime dequeue should succeed");
    }

    /** @brief Dequeue a packet and verify it is ECN-marked, not dropped. */
    void CheckMark()
    {
        // COBALT fires but Mark() returns true: CobaltShouldDrop returns false.
        NS_TEST_EXPECT_MSG_NE(m_qd->Dequeue(), nullptr, "ECN mark should prevent drop");
        NS_TEST_EXPECT_MSG_EQ(m_qd->GetStats().nTotalDroppedPacketsAfterDequeue,
                              0,
                              "No drops expected when ECN marking succeeds");
    }

    void DoRun() override
    {
        m_qd = CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth", DataRateValue(DataRate(0)));
        m_qd->Initialize();

        Simulator::ScheduleNow(&CakeCobaltEcnMarkTest::EnqueuePlain, this);
        Simulator::ScheduleNow(&CakeCobaltEcnMarkTest::EnqueueEcn, this);
        Simulator::Schedule(MilliSeconds(200), &CakeCobaltEcnMarkTest::CheckPrime, this);
        Simulator::Schedule(MilliSeconds(400), &CakeCobaltEcnMarkTest::CheckMark, this);

        Simulator::Run();
        Simulator::Destroy();
    }
};

/**
 * @brief Verify BLUE probability accumulates across successive COBALT drop events.
 *
 * Two drop cycles are driven more than cobaltInterval apart so the BLUE timer
 * fires on each, incrementing blueProb by 0.0025 per event.
 *
 * Cycle 1 — t=0 to t=400ms:
 *   pkt1 dequeued at t=200ms primes firstAboveTime=300ms (no drop).
 *   pkt2 dequeued at t=400ms: now >= firstAboveTime and cobaltDropNext=0;
 *   enters dropping state, cobaltCount=1, cobaltDropNext=500ms, blueProb=0.0025.
 *
 * Cycle 2 — t=500ms to t=600ms:
 *   pkt3 enqueued at t=500ms (sojourn=100ms at dequeue).
 *   pkt3 dequeued at t=600ms: cobaltDropping=true, now >= cobaltDropNext=500ms;
 *   cobaltCount=2, blueProb=0.005.
 *
 * Both drops are hard (Mark()=false). Stats must reflect exactly two drops.
 */
class CakeCobaltBlueTest : public TestCase
{
  public:
    CakeCobaltBlueTest()
        : TestCase("BLUE probability accumulates across multiple COBALT drop events")
    {
    }

  private:
    Ptr<CakeQueueDisc> m_qd; ///< The CakeQueueDisc instance to test.

    /** @brief Enqueue a 512-byte packet. */
    void DoEnqueue()
    {
        m_qd->Enqueue(MakeItem(512));
    }

    /** @brief Dequeue a packet to prime the COBALT interval. */
    void CheckPrime()
    {
        NS_TEST_EXPECT_MSG_NE(m_qd->Dequeue(), nullptr, "Prime dequeue should succeed");
    }

    /** @brief Verify the first COBALT drop event occurs. */
    void CheckDrop1()
    {
        // drop#1: cobaltDropNext=0 reached; enters dropping state, blueProb=0.0025.
        NS_TEST_EXPECT_MSG_EQ(m_qd->Dequeue(), nullptr, "First COBALT drop expected");
        NS_TEST_EXPECT_MSG_EQ(m_qd->GetStats().nTotalDroppedPacketsAfterDequeue,
                              1,
                              "One drop after first drop event");
    }

    /** @brief Verify the second COBALT drop event occurs and accumulates BLUE probability. */
    void CheckDrop2()
    {
        // drop#2: now(600ms) >= cobaltDropNext(500ms); cobaltCount=2, blueProb=0.005.
        NS_TEST_EXPECT_MSG_EQ(m_qd->Dequeue(), nullptr, "Second COBALT drop expected");
        NS_TEST_EXPECT_MSG_EQ(m_qd->GetStats().nTotalDroppedPacketsAfterDequeue,
                              2,
                              "Two drops after second drop event");
    }

    void DoRun() override
    {
        m_qd = CreateObjectWithAttributes<CakeQueueDisc>("Bandwidth", DataRateValue(DataRate(0)));
        m_qd->Initialize();

        // Cycle 1: pkt1 (prime) and pkt2 (first drop target) at t=0.
        Simulator::ScheduleNow(&CakeCobaltBlueTest::DoEnqueue, this);
        Simulator::ScheduleNow(&CakeCobaltBlueTest::DoEnqueue, this);
        Simulator::Schedule(MilliSeconds(200), &CakeCobaltBlueTest::CheckPrime, this);
        Simulator::Schedule(MilliSeconds(400), &CakeCobaltBlueTest::CheckDrop1, this);

        // Cycle 2: pkt3 enqueued after cobaltDropNext=500ms for a second drop event.
        Simulator::Schedule(MilliSeconds(500), &CakeCobaltBlueTest::DoEnqueue, this);
        Simulator::Schedule(MilliSeconds(600), &CakeCobaltBlueTest::CheckDrop2, this);

        Simulator::Run();
        Simulator::Destroy();
    }
};

/** @brief Test suite registering all CakeQueueDisc unit tests. */
class CakeQueueDiscTestSuite : public TestSuite
{
  public:
    CakeQueueDiscTestSuite()
        : TestSuite("cake-queue-disc", Type::UNIT)
    {
        AddTestCase(new CakeCheckConfigTest, Duration::QUICK);
        AddTestCase(new CakeBasicEnqueueDequeueTest, Duration::QUICK);
        AddTestCase(new CakeDiffServModeTest, Duration::QUICK);
        AddTestCase(new CakeShaperTest, Duration::QUICK);
        AddTestCase(new CakeCobaltDropTest, Duration::QUICK);
        AddTestCase(new CakeCobaltEcnMarkTest, Duration::QUICK);
        AddTestCase(new CakeCobaltBlueTest, Duration::QUICK);
    }
};

static CakeQueueDiscTestSuite g_cakeQueueDiscTestSuite;
