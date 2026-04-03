#include "BenchmarkUtils.h"
#include "Toybox/Common.h"
#include "Toybox/FlatMap.h"
#include <map>
#include <vector>

TOYBOX_NAMESPACE_BEGIN

// --- std::vector ---

static void BM_StdVector_LinearRead(benchmark::State& state) {
    const int            n = static_cast<int>(state.range(0));
    std::vector<int>     testVector(n, 0);
    PerfCounters         perf;
    uint64_t totalMisses = 0, totalRefs = 0;

    for (auto _ : state) {
        perf.reset();
        perf.enable();
        for (const auto& item : testVector) {
            [[maybe_unused]] int read = item;
            benchmark::DoNotOptimize(read);
        }
        benchmark::ClobberMemory();
        perf.disable();
        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();
    }

    state.SetItemsProcessed(state.iterations() * n);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * n);
}
BENCHMARK(BM_StdVector_LinearRead)->Arg(10000);

// --- std::map ---

static void BM_StdMap_LinearInsert(benchmark::State& state) {
    const int        n = static_cast<int>(state.range(0));
    std::map<int, int> testMap;
    PerfCounters         perf;
    uint64_t totalMisses = 0, totalRefs = 0;

    for (auto _ : state) {
        perf.reset();
        perf.enable();
        for (int i = 0; i < n; ++i)
            testMap.insert({i, 0});
        benchmark::ClobberMemory();
        perf.disable();
        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();

        state.PauseTiming();
        testMap.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * n);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * n);
}
BENCHMARK(BM_StdMap_LinearInsert)->Arg(10000);

static void BM_StdMap_LinearReverseInsert(benchmark::State& state) {
    const int          n = static_cast<int>(state.range(0));
    std::map<int, int> testMap;
    PerfCounters         perf;
    uint64_t totalMisses = 0, totalRefs = 0;

    for (auto _ : state) {
        perf.reset();
        perf.enable();
        for (int i = n; i >= 0; --i)
            testMap.insert({i, 0});
        benchmark::ClobberMemory();
        perf.disable();
        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();

        state.PauseTiming();
        testMap.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * n);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * n);
}
BENCHMARK(BM_StdMap_LinearReverseInsert)->Arg(1000);

static void BM_StdMap_LinearRead(benchmark::State& state) {
    const int          n = static_cast<int>(state.range(0));
    std::map<int, int> testMap;
    for (int i = n; i >= 0; --i)
        testMap.insert({i, 0});
    PerfCounters         perf;
    uint64_t totalMisses = 0, totalRefs = 0;

    for (auto _ : state) {
        perf.reset();
        perf.enable();
        for (const auto& item : testMap) {
            [[maybe_unused]] int read = item.second;
            benchmark::DoNotOptimize(read);
        }
        benchmark::ClobberMemory();
        perf.disable();
        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();
    }

    state.SetItemsProcessed(state.iterations() * n);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * n);
}
BENCHMARK(BM_StdMap_LinearRead)->Arg(10000);

// --- FlatMap ---

static void BM_FlatMap_LinearInsert(benchmark::State& state) {
    const int         n = static_cast<int>(state.range(0));
    FlatMap<int, int> testMap;
    PerfCounters         perf;
    uint64_t totalMisses = 0, totalRefs = 0;

    for (auto _ : state) {
        perf.reset();
        perf.enable();
        for (int i = 0; i < n; ++i)
            testMap.insert({i, 0});
        benchmark::ClobberMemory();
        perf.disable();
        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();

        state.PauseTiming();
        testMap.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * n);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * n);
}
BENCHMARK(BM_FlatMap_LinearInsert)->Arg(10000);

static void BM_FlatMap_LinearReverseInsert(benchmark::State& state) {
    const int         n = static_cast<int>(state.range(0));
    FlatMap<int, int> testMap;
    PerfCounters         perf;
    uint64_t totalMisses = 0, totalRefs = 0;

    for (auto _ : state) {
        perf.reset();
        perf.enable();
        for (int i = n; i >= 0; --i)
            testMap.insert({i, 0});
        benchmark::ClobberMemory();
        perf.disable();
        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();

        state.PauseTiming();
        testMap.clear();
        state.ResumeTiming();
    }

    state.SetItemsProcessed(state.iterations() * n);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * n);
}
BENCHMARK(BM_FlatMap_LinearReverseInsert)->Arg(1000);

static void BM_FlatMap_LinearRead(benchmark::State& state) {
    const int         n = static_cast<int>(state.range(0));
    FlatMap<int, int> testMap;
    for (int i = n; i >= 0; --i)
        testMap.insert({i, 0});
    PerfCounters         perf;
    uint64_t totalMisses = 0, totalRefs = 0;

    for (auto _ : state) {
        perf.reset();
        perf.enable();
        for (const auto& item : testMap) {
            auto read = item;
            benchmark::DoNotOptimize(read.first);
            benchmark::DoNotOptimize(read.second);
        }
        benchmark::ClobberMemory();
        perf.disable();
        totalMisses += perf.readMisses();
        totalRefs   += perf.readRefs();
    }

    state.SetItemsProcessed(state.iterations() * n);
    reportPerfCounters(state, perf, totalMisses, totalRefs, state.iterations() * n);
}
BENCHMARK(BM_FlatMap_LinearRead)->Arg(10000);

TOYBOX_NAMESPACE_END
