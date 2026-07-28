#pragma once
#include "Toybox/Common.h"
#include <boost/asio.hpp>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

TOYBOX_NAMESPACE_BEGIN

class Session;

/// Asynchronous TCP server with session tracking and broadcast support.
///
/// Clients communicate using the same native-endian length-prefixed frame
/// format as Client. Sessions that send kSubscribeMessage receive broadcasts.
class Server {
    boost::asio::io_context         mContext;
    boost::asio::ip::tcp::acceptor  mAcceptor;
    unsigned                        mThreadCount;
    std::vector<std::thread> mThreads;
    struct {
        std::unordered_set<std::shared_ptr<Session>> data;
        mutable std::shared_mutex                    mtx;
    } mActiveSessions;

public:
    explicit Server(short port, unsigned threadCount = std::thread::hardware_concurrency());
    ~Server();

    /// Starts accepting connections and runs the io_context on worker threads.
    void run();

    /// Sends msg to every session that has subscribed.
    void broadcast(const std::string& msg);

    /// Number of currently tracked sessions.
    std::size_t sessionCount() const;

    /// Number of tracked sessions that have sent the subscribe control frame.
    std::size_t subscriberCount() const;

    /// Removes a disconnected session from the active-session set.
    void removeSession(Session* session);

    /// Called on the server io_context thread when a non-control message arrives.
    std::function<void(std::string)> onRecieve;

    /// Called when a client connection is accepted; argument is remote port.
    std::function<void(const unsigned short)> onConnect;

    /// Called when a session read fails; argument is remote port.
    std::function<void(const unsigned short)> onDisconnect;

private:
    void accept();
};

TOYBOX_NAMESPACE_END
