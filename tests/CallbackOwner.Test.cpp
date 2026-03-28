#include "Toybox/CallbackList.h"
#include "Toybox/CallbackOwner.h"
#include <gtest/gtest.h>

TOYBOX_NAMESPACE_BEGIN

TEST(CallbackOwner, holdsCallbackAlive) {
    CallbackList<void()> list;
    int counter = 0;
    {
        CallbackOwner owner;
        owner.registerCallback(list.add([&counter]() { ++counter; }));
        list();
        EXPECT_EQ(counter, 1);
    }
    list();
    EXPECT_EQ(counter, 1); // Callback removed when owner was destroyed
}

TEST(CallbackOwner, multipleCallbacks) {
    CallbackList<void()> list;
    int counter = 0;
    {
        CallbackOwner owner;
        owner.registerCallback(list.add([&counter]() { ++counter; }));
        owner.registerCallback(list.add([&counter]() { ++counter; }));
        owner.registerCallback(list.add([&counter]() { ++counter; }));
        list();
        EXPECT_EQ(counter, 3);
    }
    list();
    EXPECT_EQ(counter, 3); // All three removed on owner destruction
}

TEST(CallbackOwner, callbacksFromMultipleLists) {
    CallbackList<void()> list1;
    CallbackList<void()> list2;
    int counter = 0;
    {
        CallbackOwner owner;
        owner.registerCallback(list1.add([&counter]() { ++counter; }));
        owner.registerCallback(list2.add([&counter]() { ++counter; }));
        list1();
        list2();
        EXPECT_EQ(counter, 2);
    }
    list1();
    list2();
    EXPECT_EQ(counter, 2);
}

TEST(CallbackOwner, safeWhenCallbackListDestroyedFirst) {
    CallbackOwner owner;
    {
        CallbackList<void()> list;
        owner.registerCallback(list.add([]() {}));
    } // list destroyed here — owner still holds the CallbackLifetime
    // owner destroyed here — must NOT crash / use-after-free
    EXPECT_TRUE(true);
}

TOYBOX_NAMESPACE_END
