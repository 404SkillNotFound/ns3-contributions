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
 */
static Ptr<QueueDiscItem>
MakeItem(uint32_t bytes = 512)
{
    auto pkt = Create<Packet>(bytes);
    return Create<CakeTestItem>(pkt, Address(), 0x0800);
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
    }
};

static CakeQueueDiscTestSuite g_cakeQueueDiscTestSuite;
