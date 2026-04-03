#include "Toybox/ThreadSafeQueue.h"
#include <forward_list>
#include <gtest/gtest.h>
#include <thread>

TOYBOX_NAMESPACE_BEGIN

TEST(ThreadSafeQueue, non_blocking_pop) {
    ThreadSafeQueue<int> tsQueue;
    tsQueue.try_pop();
    EXPECT_TRUE(true);
}

TEST(ThreadSafeQueue, orderSingleThread) {
    ThreadSafeQueue<int> tsQueue;
    tsQueue.push(1);
    tsQueue.push(2);
    tsQueue.push(3);
    EXPECT_FALSE(tsQueue.empty());
    EXPECT_EQ(tsQueue.pop(), 1);
    EXPECT_EQ(tsQueue.pop(), 2);
    EXPECT_EQ(tsQueue.pop(), 3);
    EXPECT_TRUE(tsQueue.empty());
}

TEST(ThreadSafeQueue, popAfterStop) {
    ThreadSafeQueue<int> tsQueue;
    tsQueue.stop();
    tsQueue.push(1);
    tsQueue.push(2);
    tsQueue.push(3);
    EXPECT_FALSE(tsQueue.empty());
    EXPECT_EQ(*tsQueue.pop(), 1);
    EXPECT_EQ(*tsQueue.pop(), 2);
    EXPECT_EQ(*tsQueue.pop(), 3);
    EXPECT_TRUE(tsQueue.empty());
}

TEST(ThreadSafeQueue, callPopOnEmptyFromDifferentThreads) {
    ThreadSafeQueue<int> tsQueue;
    std::thread          t([&tsQueue]() {
        EXPECT_TRUE(tsQueue.empty());
        EXPECT_TRUE(!tsQueue.pop().has_value());
    });
    std::this_thread::sleep_for(std::chrono::seconds(2));
    tsQueue.stop();
    EXPECT_TRUE(tsQueue.empty());
    EXPECT_TRUE(!tsQueue.pop().has_value());
    t.join();
}

TEST(ThreadSafeQueue, orderMultiThread) {
    ThreadSafeQueue<int> tsQueue;
    std::thread          t([&tsQueue]() {
        std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(2000));
        tsQueue.push(3);
    });
    EXPECT_TRUE(tsQueue.empty());
    tsQueue.push(1);
    tsQueue.push(2);
    EXPECT_FALSE(tsQueue.empty());
    EXPECT_EQ(*tsQueue.pop(), 1);
    EXPECT_EQ(*tsQueue.pop(), 2);
    EXPECT_EQ(*tsQueue.pop(), 3);
    EXPECT_TRUE(tsQueue.empty());
    EXPECT_TRUE(tsQueue.empty());
    t.join();
}

TEST(ThreadSafeQueue, multiConsumerProducer) {
    ThreadSafeQueue<int>           queue;
    std::forward_list<std::thread> threads;
    const int                      numProcuders = 3;
    const int                      numConsumers = 3;
    std::atomic<int>               result(0);
    for (int i = 0; i < numProcuders; ++i) {
        threads.emplace_front([&queue, i]() {
            queue.push(i + 1);
        });
    }
    for (int i = 0; i < numConsumers; ++i) {
        threads.emplace_front([&queue, &result]() {
            result += *queue.pop();
        });
    }
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_TRUE(queue.empty());
    EXPECT_EQ(result, 6);
}

TEST(ThreadSafeQueue, tryPopWithItemsAvailable) {
    ThreadSafeQueue<int> tsQueue;
    tsQueue.push(42);
    auto val = tsQueue.try_pop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, 42);
    EXPECT_TRUE(tsQueue.empty());
}

TEST(ThreadSafeQueue, tryPopStoppedReturnsNullopt) {
    ThreadSafeQueue<int> tsQueue;
    tsQueue.push(1);
    tsQueue.stop();
    // try_pop returns nullopt when stopped, even if items exist
    auto val = tsQueue.try_pop();
    EXPECT_FALSE(val.has_value());
}

TEST(ThreadSafeQueue, pushMoveOverload) {
    ThreadSafeQueue<std::string> tsQueue;
    std::string s = "hello";
    tsQueue.push(std::move(s));
    auto val = tsQueue.pop();
    ASSERT_TRUE(val.has_value());
    EXPECT_EQ(*val, "hello");
}

TEST(ThreadSafeQueue, popDrainedAfterStop) {
    ThreadSafeQueue<int> tsQueue;
    tsQueue.push(1);
    tsQueue.push(2);
    tsQueue.stop();
    // pop should still return items after stop until empty
    EXPECT_EQ(*tsQueue.pop(), 1);
    EXPECT_EQ(*tsQueue.pop(), 2);
    EXPECT_FALSE(tsQueue.pop().has_value());
}

TEST(ThreadSafeQueue, multiProducerMultiConsumerStability) {
    constexpr int N            = 100'000;
    constexpr int numProducers = 4;
    constexpr int numConsumers = 4;

    ThreadSafeQueue<int>     q;
    std::atomic<int>         produced{0};
    std::atomic<int>         consumed{0};
    std::atomic<long long>   sum{0};
    std::vector<std::thread> threads;
    threads.reserve(numProducers + numConsumers);

    for (int i = 0; i < numProducers; ++i) {
        threads.emplace_back([&]() {
            int item;
            while ((item = produced.fetch_add(1, std::memory_order_relaxed)) < N)
                q.push(item);
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

    constexpr long long expected = static_cast<long long>(N) * (N - 1) / 2;
    EXPECT_EQ(sum.load(), expected);
    EXPECT_TRUE(q.empty());
}

TOYBOX_NAMESPACE_END
