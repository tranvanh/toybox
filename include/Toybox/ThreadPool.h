#pragma once
#include "Toybox/ThreadSafeQueue.h"
#include <atomic>
#include <functional>
#include <vector>

TOYBOX_NAMESPACE_BEGIN

/// Fixed-size worker pool for void tasks.
///
/// Tasks are consumed from an internal ThreadSafeQueue. stop() joins workers and
/// then runs any remaining queued tasks on the stopping thread.
class ThreadPool {
    ThreadSafeQueue<std::function<void()>> mTasksQueue;
    std::deque<std::thread>                mWorkers;
    const int                              mWorkersCount;
    std::atomic_bool                       mIsRunning;

public:
    ThreadPool(const int workersCount);
    ~ThreadPool();

    /// Queues a copy of task for execution by a worker.
    void addTask(const std::function<void()>& task);

    /// Queues task by move for execution by a worker.
    void addTask(std::function<void()>&& task);

    /// Starts worker threads; callers should call this once before adding work.
    void run();

    /// Stops workers, joins them, and drains any tasks left in the queue.
    void stop();
};

TOYBOX_NAMESPACE_END
