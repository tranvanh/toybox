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
        auto copy = std::make_shared<T>(*mData.load());
        mutateFn(*copy);
        mData.store(std::move(copy));
    }
};

TOYBOX_NAMESPACE_END
