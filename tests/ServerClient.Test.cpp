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

// ── HighLoadClientToServer ────────────────────────────────────────────────

TEST(ServerClient, HighLoadClientToServer) {
    constexpr int MESSAGES = 10'000;
    Server        server(9106, 2);
    std::atomic<int> received{0};
    server.onRecieve = [&](std::string) { received.fetch_add(1); };

    std::thread serverThread([&] { server.run(); });

    Client client;
    std::atomic<bool> connected{false};
    server.onConnect = [&](unsigned short) { connected.store(true); };

    ASSERT_TRUE(client.connect("127.0.0.1", 9106));
    std::thread clientThread([&] { client.run(); });

    ASSERT_TRUE(waitFor([&] { return connected.load(); }));

    for (int i = 0; i < MESSAGES; ++i)
        client.sendMessage("msg");

    ASSERT_TRUE(waitFor([&] { return received.load() >= MESSAGES; }, std::chrono::seconds(15)));
    EXPECT_EQ(received.load(), MESSAGES);

    client.stop();
    clientThread.join();
    serverThread.join();
}

// ── HighLoadBroadcastMultipleClients ─────────────────────────────────────

TEST(ServerClient, HighLoadBroadcastMultipleClients) {
    constexpr int MESSAGES = 5'000;
    constexpr int N        = 5;
    Server        server(9107, 4);
    std::atomic<int> connected{0};
    server.onConnect = [&](unsigned short) { connected.fetch_add(1); };

    std::thread serverThread([&] { server.run(); });

    std::array<Client, N>            clients;
    std::array<std::atomic<int>, N>  counts{};
    std::vector<std::thread>         clientThreads;

    for (int i = 0; i < N; ++i) {
        clients[i].onReceive = [&, i](std::string) { counts[i].fetch_add(1); };
        ASSERT_TRUE(clients[i].connect("127.0.0.1", 9107));
        clientThreads.emplace_back([&clients, i] { clients[i].run(); });
    }

    ASSERT_TRUE(waitFor([&] { return connected.load() == N; }));
    for (int i = 0; i < N; ++i)
        clients[i].subscribe();
    ASSERT_TRUE(waitFor([&] { return server.subscriberCount() >= static_cast<std::size_t>(N); }));

    for (int i = 0; i < MESSAGES; ++i)
        server.broadcast("msg");

    for (int i = 0; i < N; ++i) {
        ASSERT_TRUE(waitFor([&, i] { return counts[i].load() >= MESSAGES; }, std::chrono::seconds(20)));
        EXPECT_EQ(counts[i].load(), MESSAGES);
    }

    for (int i = 0; i < N; ++i) {
        clients[i].stop();
        clientThreads[i].join();
    }
    serverThread.join();
}

// ── HighLoadBidirectional ─────────────────────────────────────────────────

TEST(ServerClient, HighLoadBidirectional) {
    constexpr int MESSAGES = 5'000;
    Server        server(9108, 2);
    std::atomic<bool> connected{false};
    server.onConnect = [&](unsigned short) { connected.store(true); };
    std::atomic<int> serverReceived{0};
    server.onRecieve = [&](std::string) { serverReceived.fetch_add(1); };

    std::thread serverThread([&] { server.run(); });

    Client           client;
    std::atomic<int> clientReceived{0};
    client.onReceive = [&](std::string) { clientReceived.fetch_add(1); };

    ASSERT_TRUE(client.connect("127.0.0.1", 9108));
    std::thread clientThread([&] { client.run(); });

    ASSERT_TRUE(waitFor([&] { return connected.load(); }));
    client.subscribe();
    ASSERT_TRUE(waitFor([&] { return server.subscriberCount() >= 1u; }));

    for (int i = 0; i < MESSAGES; ++i) {
        client.sendMessage("c2s");
        server.broadcast("s2c");
    }

    ASSERT_TRUE(waitFor([&] { return serverReceived.load() >= MESSAGES; }, std::chrono::seconds(15)));
    ASSERT_TRUE(waitFor([&] { return clientReceived.load() >= MESSAGES; }, std::chrono::seconds(15)));
    EXPECT_EQ(serverReceived.load(), MESSAGES);
    EXPECT_EQ(clientReceived.load(), MESSAGES);

    client.stop();
    clientThread.join();
    serverThread.join();
}

// ── HighLoadMultipleClientsSendToServer ───────────────────────────────────

TEST(ServerClient, HighLoadMultipleClientsSendToServer) {
    constexpr int N              = 4;
    constexpr int MSGS_PER_CLIENT = 2'500;
    constexpr int TOTAL           = N * MSGS_PER_CLIENT;
    Server        server(9109, 4);
    std::atomic<int> connected{0};
    server.onConnect = [&](unsigned short) { connected.fetch_add(1); };
    std::atomic<int> received{0};
    server.onRecieve = [&](std::string) { received.fetch_add(1); };

    std::thread serverThread([&] { server.run(); });

    std::array<Client, N>    clients;
    std::vector<std::thread> clientThreads;

    for (int i = 0; i < N; ++i) {
        ASSERT_TRUE(clients[i].connect("127.0.0.1", 9109));
        clientThreads.emplace_back([&clients, i] { clients[i].run(); });
    }

    ASSERT_TRUE(waitFor([&] { return connected.load() == N; }));

    for (int i = 0; i < N; ++i)
        for (int j = 0; j < MSGS_PER_CLIENT; ++j)
            clients[i].sendMessage("msg");

    ASSERT_TRUE(waitFor([&] { return received.load() >= TOTAL; }, std::chrono::seconds(20)));
    EXPECT_EQ(received.load(), TOTAL);

    for (int i = 0; i < N; ++i) {
        clients[i].stop();
        clientThreads[i].join();
    }
    serverThread.join();
}

TOYBOX_NAMESPACE_END
