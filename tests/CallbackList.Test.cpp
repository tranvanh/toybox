#include "Toybox/CallbackList.h"
#include "Toybox/ThreadSafeQueue.h"
#include <forward_list>
#include <gtest/gtest.h>

TOYBOX_NAMESPACE_BEGIN

TEST(CallbackList, lifetime) {
    CallbackList<void()> callbackList;
    int result = 0;
    CallbackLifetime lifetime1 = callbackList.add([&result](){++result;});
    {
        CallbackLifetime lifetime2 = callbackList.add([&result](){++result;});
        callbackList();
        EXPECT_EQ(result, 2);
    }
    callbackList();
    EXPECT_EQ(result, 3);
}

TEST(CallbackList, addMultiThread) {
    std::forward_list<std::thread> threads;
    CallbackList<void()> callbackList;
    const int                      numThreads = 3;
    std::atomic<int>               result = 0;
    ThreadSafeQueue<CallbackLifetime> lifetimes;
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_front([&result, &callbackList, &lifetimes]() {
           lifetimes.push(callbackList.add([&result](){++result;}));
        });
    }
    std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(2000));
    callbackList();
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(result, 3);
}

TEST(CallbackList, operatorMultiThread) {
    std::forward_list<std::thread> threads;
    CallbackList<void()>           callbackList;
    const int                      numThreads = 3;
    std::atomic<int>               result       = 0;
    CallbackLifetime               lifetime     = callbackList.add([&result]() {
        ++result;
    });
    for (int i = 0; i < numThreads; ++i) {
        threads.emplace_front([&result, &callbackList]() {
           callbackList();
        });
    }
    std::this_thread::sleep_for(std::chrono::duration<int, std::milli>(1000));
    for (auto& t : threads) {
        t.join();
    }
    EXPECT_EQ(result, 3);
}

TEST(CallbackList, emptyListInvocation) {
    CallbackList<void()> callbackList;
    EXPECT_NO_FATAL_FAILURE(callbackList());
}

TEST(CallbackList, withArguments) {
    CallbackList<void(int)> callbackList;
    int result = 0;
    CallbackLifetime lifetime = callbackList.add([&result](int x) { result += x; });
    callbackList(5);
    EXPECT_EQ(result, 5);
    callbackList(3);
    EXPECT_EQ(result, 8);
}

TEST(CallbackList, allLifetimesExpiredBeforeInvoke) {
    CallbackList<void()> callbackList;
    int counter = 0;
    {
        CallbackLifetime l1 = callbackList.add([&counter]() { ++counter; });
        CallbackLifetime l2 = callbackList.add([&counter]() { ++counter; });
    }
    callbackList();
    EXPECT_EQ(counter, 0);
}

TEST(CallbackList, movedLifetimeStillValid) {
    CallbackList<void()> callbackList;
    int counter = 0;
    CallbackLifetime l1 = callbackList.add([&counter]() { ++counter; });
    CallbackLifetime l2 = std::move(l1);
    callbackList();
    EXPECT_EQ(counter, 1);
}

TOYBOX_NAMESPACE_END