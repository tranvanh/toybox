#include "BenchmarkUtils.h"
#include "Toybox/LockFreeRingQueue.h"
#include "Toybox/ThreadSafeQueue.h"
#include <atomic>
#include <thread>
#include <vector>

TOYBOX_NAMESPACE_BEGIN

static constexpr int NUM_ITEMS = 100'000;

// --- LockFreeRingQueue ---
// Non-blocking: producers spin on try_push, consumers spin on try_pop.
// Queue is reused across iterations (sequence numbers stay consistent).

static void BM_LockFreeRingQueue_MPMC(benchmark::State& state) {
    const int numProducers = static_cast<int>(state.range(0));
    const int numConsumers = static_cast<int>(state.range(1));
    LockFreeRingQueue<int, 1024> q;
    PerfCounters perf;

    uint64_t totalMisses = 0;
    uint64_t totalRefs   = 0;

    for (auto _ : state) {
        std::atomic<int>         produced{0};
        std::atomic<int>         consumed{0};
        std::vector<std::thread> threads;
        threads.reserve(numProducers + numConsumers);

        perf.reset();
        perf.enable();

        for (int i = 0; i < numProducers; ++i) {
            threads.emplace_back([&]() {
                int item;
                while ((item = produced.fetch_add(1, std::memory_order_relaxed)) < NUM_ITEMS)
                    while (!q.try_push(item)) {}
            });
        }

        for (int i = 0; i < numConsumers; ++i) {
            threads.emplace_back([&]() {
                while (consumed.load(std::memory_order_relaxed) < NUM_ITEMS) {
                    if (q.try_pop())
                        consumed.fetch_add(1, std::memory_order_relaxed);
                }
            });
        }

        for (auto& t : threads) t.join();
        perf.disable();

        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();
    }

    state.SetItemsProcessed(state.iterations() * NUM_ITEMS);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * NUM_ITEMS);
}

BENCHMARK(BM_LockFreeRingQueue_MPMC)
    ->Args({1, 1})
    ->Args({2, 2})
    ->Args({4, 4})
    ->UseRealTime();

// --- ThreadSafeQueue ---
// Blocking: consumers sleep on pop() until an item arrives or the queue is
// stopped. The last producer to finish calls stop(), which wakes all consumers.
// After stop(), pop() drains remaining items before returning nullopt.
// A fresh queue is created each iteration since stop() is not reversible.

static void BM_ThreadSafeQueue_MPMC(benchmark::State& state) {
    const int numProducers = static_cast<int>(state.range(0));
    const int numConsumers = static_cast<int>(state.range(1));
    PerfCounters perf;

    uint64_t totalMisses = 0;
    uint64_t totalRefs   = 0;

    for (auto _ : state) {
        ThreadSafeQueue<int>     q;
        std::atomic<int>         produced{0};
        std::atomic<int>         producersDone{0};
        std::vector<std::thread> producers, consumers;
        producers.reserve(numProducers);
        consumers.reserve(numConsumers);

        perf.reset();
        perf.enable();

        // Start consumers first so they are ready to receive immediately.
        for (int i = 0; i < numConsumers; ++i) {
            consumers.emplace_back([&]() {
                while (q.pop()) {}
            });
        }

        for (int i = 0; i < numProducers; ++i) {
            producers.emplace_back([&]() {
                int item;
                while ((item = produced.fetch_add(1, std::memory_order_relaxed)) < NUM_ITEMS)
                    q.push(item);
                // Last producer to finish signals consumers to exit.
                if (producersDone.fetch_add(1, std::memory_order_acq_rel) + 1 == numProducers)
                    q.stop();
            });
        }

        for (auto& t : producers) t.join();
        for (auto& t : consumers) t.join();
        perf.disable();

        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();
    }

    state.SetItemsProcessed(state.iterations() * NUM_ITEMS);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * NUM_ITEMS);
}

BENCHMARK(BM_ThreadSafeQueue_MPMC)
    ->Args({1, 1})
    ->Args({2, 2})
    ->Args({4, 4})
    ->UseRealTime();

TOYBOX_NAMESPACE_END
