#pragma once
#include "Toybox/Common.h"
#include <functional>

TOYBOX_NAMESPACE_BEGIN

/// Move-only RAII token that unregisters a callback when destroyed.
///
/// Callback registration APIs return this object to make ownership explicit:
/// keep the token alive while the callback should remain registered.
class CallbackLifetime {
    std::function<void()> mRemoveCallback;

public:
    // Disallow copy construction
    CallbackLifetime(const CallbackLifetime&) = delete;
    CallbackLifetime(CallbackLifetime&& other) { mRemoveCallback = std::move(other.mRemoveCallback); }
    CallbackLifetime(const std::function<void()>& removeCallback)
        : mRemoveCallback(std::move(removeCallback)) {}
    ~CallbackLifetime() {
        // Moved-from handles have no cleanup responsibility.
        if (mRemoveCallback) {
            mRemoveCallback();
            mRemoveCallback = nullptr;
        }
    }
};

TOYBOX_NAMESPACE_END
