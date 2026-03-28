#pragma once
#include "Toybox/CallbackLifetime.h"
#include <memory>
#include <mutex>
#include <unordered_map>

TOYBOX_NAMESPACE_BEGIN

using CallbackId         = std::size_t;
static CallbackId nextId = 0;

template <typename Signature>
class CallbackList;

template <typename Ret, typename... Args>
class CallbackList<Ret(Args...)> {
    using Callback = std::function<Ret(Args...)>;

    struct State {
        std::unordered_map<CallbackId, Callback> list;
        std::mutex                               lock;
    };

    std::shared_ptr<State> mState = std::make_shared<State>();

public:
    void operator()(Args&&... args) {
        std::lock_guard<std::mutex> lock(mState->lock);
        for (const auto& callback : mState->list) {
            callback.second(std::forward<Args>(args)...);
        }
    }

    [[nodiscard]] CallbackLifetime add(Callback callback) {
        CallbackId                   id = nextId++;
        std::unique_lock<std::mutex> lock(mState->lock);
        mState->list.insert({ id, std::move(callback) });
        lock.unlock();
        // weak_ptr instead of shared_ptr for two reasons:
        // 1. A shared_ptr capture would extend the lifetime of State beyond
        //    the CallbackList itself — callbacks and their captured data would
        //    stay allocated as long as any CallbackLifetime exists.
        // 2. It avoids potential reference cycles: if a stored callback
        //    captures the CallbackList (e.g. via a shared_ptr to its owner),
        //    then State → callback → shared_ptr<State> would form a cycle
        //    that neither side can break, leaking both. weak_ptr does not
        //    contribute to the ref-count so no cycle can form.
        std::weak_ptr<State> weak = mState;
        return CallbackLifetime([weak, id]() {
            if (auto state = weak.lock()) {
                std::lock_guard<std::mutex> guard(state->lock);
                state->list.erase(id);
            }
        });
    }
};

TOYBOX_NAMESPACE_END
