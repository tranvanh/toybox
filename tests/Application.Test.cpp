#include "Toybox/Application.h"
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

TOYBOX_NAMESPACE_BEGIN

TEST(Application, runAndStop) {
    Application app(4);
    app.run();
    app.stop();
    EXPECT_TRUE(true);
}

TEST(Application, stopWithoutRun) {
    Application app(4);
    app.stop();
    EXPECT_TRUE(true);
}

TEST(Application, runBackgroundTasksExecuted) {
    Application app(4);
    app.run();
    std::atomic<int> counter(0);
    for (int i = 0; i < 10; ++i) {
        app.runBackgroundTask([&counter]() { ++counter; });
    }
    app.stop();
    EXPECT_EQ(counter, 10);
}

TEST(Application, tasksQueuedBeforeRun) {
    Application app(4);
    std::atomic<int> counter(0);
    for (int i = 0; i < 5; ++i) {
        app.runBackgroundTask([&counter]() { ++counter; });
    }
    app.run();
    app.stop();
    EXPECT_EQ(counter, 5);
}

TEST(Application, destructorStopsPool) {
    std::atomic<int> counter(0);
    {
        Application app(2);
        app.run();
        app.runBackgroundTask([&counter]() { ++counter; });
    } // destructor should drain tasks
    EXPECT_EQ(counter, 1);
}

TOYBOX_NAMESPACE_END
