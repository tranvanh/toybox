#pragma once

#include <benchmark/benchmark.h>

#ifdef __linux__
#include <linux/perf_event.h>
#include <sys/ioctl.h>
#include <sys/syscall.h>
#include <unistd.h>
#endif

// RAII wrapper around Linux perf_event_open.
// Tracks LLC cache-misses and cache-references for the calling thread and all
// threads it spawns (inherit=1). Silently becomes a no-op if hardware PMU
// counters are unavailable (common in VMs and WSL2 — counters will read zero).
// On non-Linux platforms the entire implementation is stubbed out.
struct PerfCounters {
#ifdef __linux__
    int fdMisses = -1;
    int fdRefs   = -1;

    PerfCounters() {
        fdMisses = openEvent(PERF_COUNT_HW_CACHE_MISSES);
        fdRefs   = openEvent(PERF_COUNT_HW_CACHE_REFERENCES);
    }

    ~PerfCounters() {
        if (fdMisses >= 0) ::close(fdMisses);
        if (fdRefs   >= 0) ::close(fdRefs);
    }

    bool available() const { return fdMisses >= 0 && fdRefs >= 0; }

    void reset()   { ctrl(PERF_EVENT_IOC_RESET);   }
    void enable()  { ctrl(PERF_EVENT_IOC_ENABLE);  }
    void disable() { ctrl(PERF_EVENT_IOC_DISABLE); }

    uint64_t readMisses() { return readFd(fdMisses); }
    uint64_t readRefs()   { return readFd(fdRefs);   }

private:
    static int openEvent(uint64_t config) {
        perf_event_attr attr{};
        attr.type           = PERF_TYPE_HARDWARE;
        attr.size           = sizeof(attr);
        attr.config         = config;
        attr.disabled       = 1;
        attr.exclude_kernel = 1;
        attr.exclude_hv     = 1;
        attr.inherit        = 1; // follow into spawned threads
        return static_cast<int>(
            syscall(__NR_perf_event_open, &attr, 0 /*this thread*/, -1 /*any cpu*/, -1, 0));
    }

    void ctrl(unsigned long req) {
        if (fdMisses >= 0) ioctl(fdMisses, req, 0);
        if (fdRefs   >= 0) ioctl(fdRefs,   req, 0);
    }

    static uint64_t readFd(int fd) {
        uint64_t val = 0;
        if (fd >= 0) {
            [[maybe_unused]] auto r = ::read(fd, &val, sizeof(val));
        }
        return val;
    }
#else
    bool     available()    const { return false; }
    void     reset()              {}
    void     enable()             {}
    void     disable()            {}
    uint64_t readMisses()         { return 0; }
    uint64_t readRefs()           { return 0; }
#endif
};

// Reports LLC miss/ref counters as per-item rates.
// totalItems is the grand total across all iterations (iterations * items_per_iteration).
inline void reportPerfCounters(benchmark::State& state, const PerfCounters& perf,
                                uint64_t totalMisses, uint64_t totalRefs,
                                int64_t totalItems) {
    if (!perf.available()) return;
    const double perItem = static_cast<double>(totalItems);
    state.counters["LLC_miss/item"] = totalMisses / perItem;
    state.counters["LLC_ref/item"]  = totalRefs   / perItem;
    if (totalRefs > 0)
        state.counters["LLC_miss%"] = 100.0 * totalMisses / totalRefs;
}
