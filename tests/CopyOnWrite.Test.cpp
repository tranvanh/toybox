#include "Toybox/CopyOnWrite.h"
#include <gtest/gtest.h>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

TOYBOX_NAMESPACE_BEGIN

TEST(CopyOnWrite, GetReturnsConstructedValue) {
    CopyOnWrite<int> cow(42);
    EXPECT_EQ(*cow.get(), 42);
}

TEST(CopyOnWrite, WriteUpdatesValue) {
    CopyOnWrite<int> cow(10);
    cow.write([](int& v) { v = 99; });
    EXPECT_EQ(*cow.get(), 99);
}

TEST(CopyOnWrite, SnapshotUnaffectedByWrite) {
    CopyOnWrite<int> cow(1);
    auto snapshot = cow.get();
    cow.write([](int& v) { v = 2; });
    EXPECT_EQ(*snapshot, 1);
    EXPECT_EQ(*cow.get(), 2);
}

TEST(CopyOnWrite, WorksWithString) {
    CopyOnWrite<std::string> cow("hello");
    cow.write([](std::string& s) { s += " world"; });
    EXPECT_EQ(*cow.get(), "hello world");
}

TEST(CopyOnWrite, MoveConstructor) {
    CopyOnWrite<int> cow(7);
    CopyOnWrite<int> moved(std::move(cow));
    EXPECT_EQ(*moved.get(), 7);
    EXPECT_EQ(cow.get(), nullptr);
}

TEST(CopyOnWrite, ConstructWithMultipleArguments) {
    struct Point { int x, y, z; };
    CopyOnWrite<Point> cow(1, 2, 3);
    auto p = cow.get();
    EXPECT_EQ(p->x, 1);
    EXPECT_EQ(p->y, 2);
    EXPECT_EQ(p->z, 3);
}

// Readers always see a consistent (non-null) value while concurrent writes happen
TEST(CopyOnWrite, ConcurrentReadersAlwaysSeeValidValue) {
    CopyOnWrite<int> cow(0);
    std::atomic<bool> stop{false};

    std::vector<std::thread> readers;
    for (int i = 0; i < 4; ++i) {
        readers.emplace_back([&] {
            while (!stop.load()) {
                auto snapshot = cow.get();
                ASSERT_NE(snapshot, nullptr);
                ASSERT_GE(*snapshot, 0);
            }
        });
    }

    for (int i = 1; i <= 1000; ++i)
        cow.write([i](int& v) { v = i; });

    stop.store(true);
    for (auto& t : readers) t.join();
}

// Snapshots captured before a write are immutable — concurrent writes don't corrupt them
TEST(CopyOnWrite, SnapshotImmutableUnderConcurrentWrites) {
    CopyOnWrite<int> cow(42);
    auto snapshot = cow.get();

    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i) {
        writers.emplace_back([&] {
            for (int j = 0; j < 250; ++j)
                cow.write([](int& v) { v++; });
        });
    }

    for (auto& t : writers) t.join();

    // snapshot must still hold the value it was taken at
    EXPECT_EQ(*snapshot, 42);
    // cow's value advanced beyond the starting point (at least one write committed)
    EXPECT_GT(*cow.get(), 42);
}

// Concurrent writers do not corrupt the stored value
TEST(CopyOnWrite, ConcurrentWritersProduceValidValue) {
    CopyOnWrite<int> cow(0);

    std::vector<std::thread> writers;
    for (int i = 0; i < 4; ++i) {
        writers.emplace_back([&] {
            for (int j = 0; j < 250; ++j)
                cow.write([](int& v) { v++; });
        });
    }

    for (auto& t : writers) t.join();

    // Each increment is a read-copy-update, so under races some increments may
    // be lost (last-writer-wins). The value must be in [1, 1000] and not garbage.
    int result = *cow.get();
    EXPECT_GE(result, 1);
    EXPECT_LE(result, 1000);
}

static_assert(!std::copy_constructible<CopyOnWrite<int>>,
    "CopyOnWrite must not be copy constructible");
static_assert(!std::is_copy_assignable_v<CopyOnWrite<int>>,
    "CopyOnWrite must not be copy assignable");
static_assert(std::move_constructible<CopyOnWrite<int>>,
    "CopyOnWrite must be move constructible");

TOYBOX_NAMESPACE_END
