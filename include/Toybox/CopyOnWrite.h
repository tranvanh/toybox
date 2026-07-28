#pragma once

#include "Common.h"
#include <memory>
#include <functional>
#include <atomic>
#include <concepts>

TOYBOX_NAMESPACE_BEGIN

/// Atomic copy-on-write wrapper for cheaply publishing immutable snapshots.
///
/// Readers get a shared_ptr to the current immutable value. Writers copy the
/// current value, mutate the copy, and atomically publish it.
template<std::copy_constructible T>
class CopyOnWrite {
    std::atomic<std::shared_ptr<const T>> mData;
public:
    template <typename... Args>
    explicit CopyOnWrite(Args&&... args) : mData(std::make_shared<const T>(std::forward<Args>(args)...)) {}

    CopyOnWrite(const CopyOnWrite&) = delete;
    CopyOnWrite& operator=(const CopyOnWrite&) = delete;

    CopyOnWrite(CopyOnWrite&& other) noexcept : mData(other.mData.exchange(nullptr)) {}

    /// Returns the currently published snapshot.
    std::shared_ptr<const T> get() const { return mData.load(); }

    /// Applies mutateFn to a private copy and retries if another writer wins.
    void write(const std::function<void(T&)>& mutateFn) {
        auto dataPtr = mData.load();
        auto copy = std::make_shared<T>(*dataPtr);
        mutateFn(*copy);

        while (!mData.compare_exchange_weak(dataPtr, copy, std::memory_order_release, std::memory_order_acquire)) {
            ASSERT(dataPtr != nullptr, "Invalid data");
            // dataPtr is updated by compare_exchange_weak on failure; rebuild
            // the candidate from the latest published snapshot before retrying.
            *copy = *dataPtr;
            mutateFn(*copy);
        }
    }
};

TOYBOX_NAMESPACE_END
