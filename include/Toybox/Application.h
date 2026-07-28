#pragma once
#include "Toybox/Common.h"
#include "Toybox/ThreadPool.h"
#include <atomic>
#include <forward_list>
#include <functional>
#include <thread>

TOYBOX_NAMESPACE_BEGIN

/// Base application lifecycle wrapper with a managed background thread pool.
///
/// Derive from this class when an application needs a simple running flag plus
/// shared worker threads for fire-and-forget background work.
class Application {
    ThreadPool mThreadPool;

public:
    // \todo make the thread count configuration better
    Application(const int threadCount = std::thread::hardware_concurrency())
        : mThreadPool(threadCount) {}
    virtual ~Application();

    /// Marks the application running and starts the worker pool.
    virtual void     run();

    /// Marks the application stopped and drains/stops the worker pool.
    virtual void     stop();

    /// Schedules work on the application thread pool.
    void             runBackgroundTask(std::function<void()> f);

    /// Public state flag for callers that need to observe lifecycle state.
    std::atomic_bool isRunning = false;
};

TOYBOX_NAMESPACE_END
