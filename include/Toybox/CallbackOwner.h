#pragma once
#include "Toybox/CallbackLifetime.h"
#include "Toybox/Common.h"
#include <forward_list>

TOYBOX_NAMESPACE_BEGIN

/// Convenience owner for multiple callback registrations.
///
/// Useful for classes that subscribe to several CallbackList instances and want
/// all registrations removed automatically with normal object destruction.
class CallbackOwner {
    std::forward_list<CallbackLifetime> mCallbacks;

public:
    CallbackOwner()  = default;
    ~CallbackOwner() = default;

    /// Takes ownership of a registration token.
    void registerCallback(CallbackLifetime&& callback) { mCallbacks.emplace_front(std::move(callback)); }
};

TOYBOX_NAMESPACE_END
