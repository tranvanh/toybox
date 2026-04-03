#include "Toybox/LockFreeRingQueue.h"
#include <atomic>
#include <gtest/gtest.h>
#include <thread>
#include <vector>

TOYBOX_NAMESPACE_BEGIN

TEST(LockFreeRingQueue, tryPopEmpty) {
    LockFreeRingQueue<int, 4> q;
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(LockFreeRingQueue, orderSingleThread) {
    LockFreeRingQueue<int, 4> q;
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    EXPECT_EQ(*q.try_pop(), 1);
    EXPECT_EQ(*q.try_pop(), 2);
    EXPECT_EQ(*q.try_pop(), 3);
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(LockFreeRingQueue, fullQueueReturnsFalse) {
    // Capacity is 4; the 5th push must be rejected without blocking.
    LockFreeRingQueue<int, 4> q;
    EXPECT_TRUE(q.try_push(1));
    EXPECT_TRUE(q.try_push(2));
    EXPECT_TRUE(q.try_push(3));
    EXPECT_TRUE(q.try_push(4));
    EXPECT_FALSE(q.try_push(5));
}

TEST(LockFreeRingQueue, wrapAround) {
    // Each lap advances the head/tail indices by one full capacity (4).
    // After 4 laps the indices have wrapped around the ring once completely,
    // verifying the modulo logic stays correct across multiple wrap-arounds.
    LockFreeRingQueue<int, 4> q;
    for (int lap = 0; lap < 4; ++lap) {
        for (int i = 0; i < 4; ++i)
            EXPECT_TRUE(q.try_push(i));
        for (int i = 0; i < 4; ++i)
            EXPECT_EQ(*q.try_pop(), i);
    }
    EXPECT_FALSE(q.try_pop().has_value());
}

TEST(LockFreeRingQueue, singleProducerSingleConsumer) {
    constexpr int      N = 100'000;
    LockFreeRingQueue<int, 256> q;
    std::atomic<long long>      sum{0};

    std::thread producer([&]() {
        for (int i = 0; i < N; ++i)
            while (!q.try_push(i)) {}
    });

    std::thread consumer([&]() {
        int consumed = 0;
        while (consumed < N) {
            if (auto val = q.try_pop()) {
                sum.fetch_add(*val, std::memory_order_relaxed);
                ++consumed;
            }
        }
    });

    producer.join();
    consumer.join();

    // Sum of 0..N-1 via the closed-form triangular formula: N*(N-1)/2.
    constexpr long long expected = static_cast<long long>(N) * (N - 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}

TEST(LockFreeRingQueue, multiProducerMultiConsumer) {
    constexpr int N            = 100'000;
    constexpr int numProducers = 4;
    constexpr int numConsumers = 4;

    LockFreeRingQueue<int, 256> q;
    std::atomic<int>            produced{0};
    std::atomic<int>            consumed{0};
    std::atomic<long long>      sum{0};
    std::vector<std::thread>    threads;
    threads.reserve(numProducers + numConsumers);

    for (int i = 0; i < numProducers; ++i) {
        threads.emplace_back([&]() {
            int item;
            while ((item = produced.fetch_add(1, std::memory_order_relaxed)) < N)
                while (!q.try_push(item)) {}
        });
    }

    for (int i = 0; i < numConsumers; ++i) {
        threads.emplace_back([&]() {
            while (consumed.load(std::memory_order_relaxed) < N) {
                if (auto val = q.try_pop()) {
                    sum.fetch_add(*val, std::memory_order_relaxed);
                    consumed.fetch_add(1, std::memory_order_relaxed);
                }
            }
        });
    }

    for (auto& t : threads) t.join();

    // Each integer 0..N-1 is produced exactly once (via fetch_add) and
    // consumed exactly once (via the consumed counter), so the total must
    // equal the triangular sum N*(N-1)/2 regardless of interleaving.
    constexpr long long expected = static_cast<long long>(N) * (N - 1) / 2;
    EXPECT_EQ(sum.load(), expected);
}

TOYBOX_NAMESPACE_END
