#pragma once

#include "Common.h"
#include <memory>
#include <functional>
#include <atomic>
#include <concepts>

TOYBOX_NAMESPACE_BEGIN

template<std::copy_constructible T>
class CopyOnWrite {
    std::atomic<std::shared_ptr<const T>> mData;
public:
    template <typename... Args>
    explicit CopyOnWrite(Args&&... args) : mData(std::make_shared<const T>(std::forward<Args>(args)...)) {}

    CopyOnWrite(const CopyOnWrite&) = delete;
    CopyOnWrite& operator=(const CopyOnWrite&) = delete;

    CopyOnWrite(CopyOnWrite&& other) noexcept : mData(other.mData.exchange(nullptr)) {}

    std::shared_ptr<const T> get() const { return mData.load(); }

    void write(const std::function<void(T&)>& mutateFn) {
        auto dataPtr = mData.load();
        auto copy = std::make_shared<T>(*dataPtr);
        mutateFn(*copy);

        while (!mData.compare_exchange_weak(dataPtr, copy, std::memory_order_release, std::memory_order_acquire)) {
            ASSERT(dataPtr != nullptr, "Invalid data");
            *copy = *dataPtr;
            mutateFn(*copy);
        }
    }
};

TOYBOX_NAMESPACE_END
