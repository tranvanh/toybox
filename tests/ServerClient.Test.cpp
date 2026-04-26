#include "Toybox/Client.h"
#include "Toybox/Server.h"
#include <array>
#include <atomic>
#include <chrono>
#include <gtest/gtest.h>
#include <thread>

TOYBOX_NAMESPACE_BEGIN

template<typename Pred>
static bool waitFor(Pred pred, std::chrono::milliseconds timeout = std::chrono::seconds(3)) {
    const auto deadline = std::chrono::steady_clock::now() + timeout;
    while (!pred() && std::chrono::steady_clock::now() < deadline)
        std::this_thread::sleep_for(std::chrono::milliseconds(5));
    return pred();
}

// ── BroadcastSingleClient ──────────────────────────────────────────────────

TEST(ServerClient, BroadcastSingleClient) {
    Server server(9100, 2);
    std::atomic<bool> connected{false};
    server.onConnect = [&](unsigned short) { connected.store(true); };

    std::thread serverThread([&] { server.run(); });

    Client client;
    std::atomic<bool> received{false};
    std::string       receivedMsg;
    client.onReceive = [&](std::string msg) {
        receivedMsg = std::move(msg);
        received.store(true);
    };

    ASSERT_TRUE(client.connect("127.0.0.1", 9100));
    std::thread clientThread([&] { client.run(); });

    ASSERT_TRUE(waitFor([&] { return connected.load(); }));
    client.subscribe();
    ASSERT_TRUE(waitFor([&] { return server.subscriberCount() >= 1u; }));

    server.broadcast("hello");

    ASSERT_TRUE(waitFor([&] { return received.load(); }));
    EXPECT_EQ(receivedMsg, "hello");

    client.stop();
    clientThread.join();
    serverThread.join();
}

// ── BroadcastMultipleClients ───────────────────────────────────────────────

TEST(ServerClient, BroadcastMultipleClients) {
    constexpr int N = 3;
    Server        server(9101, 2);
    std::atomic<int> connected{0};
    server.onConnect = [&](unsigned short) { connected.fetch_add(1); };

    std::thread serverThread([&] { server.run(); });

    std::array<Client, N>            clients;
    std::array<std::atomic<bool>, N> received{};
    std::array<std::string, N>       messages{};

    std::vector<std::thread> clientThreads;
    for (int i = 0; i < N; ++i) {
        clients[i].onReceive = [&, i](std::string msg) {
            messages[i] = std::move(msg);
            received[i].store(true);
        };
        ASSERT_TRUE(clients[i].connect("127.0.0.1", 9101));
        clientThreads.emplace_back([&clients, i] { clients[i].run(); });
    }

    ASSERT_TRUE(waitFor([&] { return connected.load() == N; }));
    for (int i = 0; i < N; ++i)
        clients[i].subscribe();
    ASSERT_TRUE(waitFor([&] { return server.subscriberCount() >= static_cast<std::size_t>(N); }));

    server.broadcast("multicast");

    for (int i = 0; i < N; ++i) {
        ASSERT_TRUE(waitFor([&, i] { return received[i].load(); }));
        EXPECT_EQ(messages[i], "multicast");
    }

    for (int i = 0; i < N; ++i) {
        clients[i].stop();
        clientThreads[i].join();
    }
    serverThread.join();
}

// ── BroadcastHighLoad ──────────────────────────────────────────────────────

TEST(ServerClient, BroadcastHighLoad) {
    constexpr int MESSAGES = 500;
    Server        server(9102, 2);
    std::atomic<bool> connected{false};
    server.onConnect = [&](unsigned short) { connected.store(true); };

    std::thread serverThread([&] { server.run(); });

    Client           client;
    std::atomic<int> count{0};
    client.onReceive = [&](std::string) { count.fetch_add(1); };

    ASSERT_TRUE(client.connect("127.0.0.1", 9102));
    std::thread clientThread([&] { client.run(); });

    ASSERT_TRUE(waitFor([&] { return connected.load(); }));
    client.subscribe();
    ASSERT_TRUE(waitFor([&] { return server.subscriberCount() >= 1u; }));

    for (int i = 0; i < MESSAGES; ++i)
        server.broadcast("msg");

    ASSERT_TRUE(waitFor([&] { return count.load() >= MESSAGES; }, std::chrono::seconds(10)));
    EXPECT_EQ(count.load(), MESSAGES);

    client.stop();
    clientThread.join();
    serverThread.join();
}

// ── ClientOnReceiveNotCalledBeforeBroadcast ────────────────────────────────

TEST(ServerClient, ClientOnReceiveNotCalledBeforeBroadcast) {
    Server server(9103, 1);
    std::atomic<bool> connected{false};
    server.onConnect = [&](unsigned short) { connected.store(true); };

    std::thread serverThread([&] { server.run(); });

    Client           client;
    std::atomic<bool> received{false};
    client.onReceive = [&](std::string) { received.store(true); };

    ASSERT_TRUE(client.connect("127.0.0.1", 9103));
    std::thread clientThread([&] { client.run(); });

    ASSERT_TRUE(waitFor([&] { return connected.load(); }));

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(received.load());

    client.stop();
    clientThread.join();
    serverThread.join();
}

// ── NonSubscriberDoesNotReceiveBroadcast ───────────────────────────────────

TEST(ServerClient, NonSubscriberDoesNotReceiveBroadcast) {
    Server server(9105, 1);
    std::atomic<bool> connected{false};
    server.onConnect = [&](unsigned short) { connected.store(true); };

    std::thread serverThread([&] { server.run(); });

    Client            client;
    std::atomic<bool> received{false};
    client.onReceive = [&](std::string) { received.store(true); };

    ASSERT_TRUE(client.connect("127.0.0.1", 9105));
    std::thread clientThread([&] { client.run(); });

    ASSERT_TRUE(waitFor([&] { return connected.load(); }));

    server.broadcast("ignored");

    std::this_thread::sleep_for(std::chrono::milliseconds(200));
    EXPECT_FALSE(received.load());

    client.stop();
    clientThread.join();
    serverThread.join();
}

// ── DisconnectedSessionRemoved ─────────────────────────────────────────────

TEST(ServerClient, DisconnectedSessionRemoved) {
    Server server(9104, 1);
    std::atomic<bool> connected{false};
    server.onConnect = [&](unsigned short) { connected.store(true); };

    std::thread serverThread([&] { server.run(); });

    {
        Client client;
        ASSERT_TRUE(client.connect("127.0.0.1", 9104));
        std::thread clientThread([&] { client.run(); });

        ASSERT_TRUE(waitFor([&] { return connected.load(); }));
        EXPECT_EQ(server.sessionCount(), 1u);

        client.stop();
        clientThread.join();
    }

    ASSERT_TRUE(waitFor([&] { return server.sessionCount() == 0u; }));
    EXPECT_EQ(server.sessionCount(), 0u);

    serverThread.join();
}

TOYBOX_NAMESPACE_END
