#pragma once
#include "Toybox/Common.h"
#include <condition_variable>
#include <deque>
#include <map>
#include <optional>
#include <thread>

TOYBOX_NAMESPACE_BEGIN

/// Mutex-protected FIFO queue with blocking and non-blocking pop operations.
///
/// stop() wakes all blocked consumers. Once stopped, pop operations return
/// std::nullopt when no queued values remain.
template <typename Type>
class ThreadSafeQueue {
    std::mutex              m;
    std::condition_variable cv;
    std::deque<Type>        mQueue;
    bool                    mStop = false;


public:
    ~ThreadSafeQueue() { stop(); }

    /// Appends a copy and wakes one blocked consumer.
    void push(const Type& value);

    /// Appends by move and wakes one blocked consumer.
    void push(Type&& value);

    /// Blocks until a value is available or the queue has been stopped.
    std::optional<Type> pop();

    /// Attempts to pop immediately without waiting.
    std::optional<Type> try_pop();

    /// Returns true when there are no queued values at the instant checked.
    bool                empty();

    /// Prevents future blocking waits and wakes all current waiters.
    void stop();
};

template <typename Type>
void ThreadSafeQueue<Type>::push(const Type& value) {
    {
        std::lock_guard<std::mutex> lock(m);
        mQueue.emplace_back(value);
    }
    cv.notify_one();
}

template <typename Type>
void ThreadSafeQueue<Type>::push(Type&& value) {
    {
        std::lock_guard<std::mutex> lock(m);
        mQueue.emplace_back(std::forward<Type>(value));
    }
    cv.notify_one();
}

template <typename Type>
std::optional<Type> ThreadSafeQueue<Type>::pop() {
    std::unique_lock<std::mutex> lock(m);
    cv.wait(lock, [this]() {
        return !mQueue.empty() || mStop;
    });
    if (mStop && mQueue.empty()) {
        return std::nullopt;
    }
    Type value = mQueue.front();
    mQueue.pop_front();
    return value;
}

template <typename Type>
std::optional<Type> ThreadSafeQueue<Type>::try_pop() {
    std::unique_lock<std::mutex> lock(m);
    if (mQueue.empty() || mStop) {
        return std::nullopt;
    }
    Type value = mQueue.front();
    mQueue.pop_front();
    return value;
}

template <typename Type>
bool               ThreadSafeQueue<Type>:: empty(){
    std::lock_guard<std::mutex> lock(m);
    return mQueue.empty();
}

template <typename Type>
void ThreadSafeQueue<Type>::stop() {
    {
        std::lock_guard<std::mutex> lock(m);
        mStop = true;
    }
    cv.notify_all();
}

TOYBOX_NAMESPACE_END
