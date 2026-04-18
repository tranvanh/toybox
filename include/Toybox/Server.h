#pragma once
#include "Toybox/Common.h"
#include <boost/asio.hpp>
#include <shared_mutex>
#include <thread>
#include <unordered_set>

TOYBOX_NAMESPACE_BEGIN

class Session;

class Server {
    boost::asio::io_context         mContext;
    boost::asio::ip::tcp::acceptor  mAcceptor;
    unsigned                        mThreadCount;
    struct {
        std::unordered_set<std::shared_ptr<Session>> data;
        mutable std::shared_mutex                    mtx;
    } mActiveSessions;

public:
    explicit Server(short port, unsigned threadCount = std::thread::hardware_concurrency());
    ~Server();
    void run();

    void broadcast(const std::string& msg);
    void removeSession(Session* session);
    std::size_t sessionCount() const;

    std::function<void(std::string)> onRecieve;
    std::function<void(const unsigned short)> onConnect;
    std::function<void(const unsigned short)> onDisconnect;

private:
    void accept();
};

TOYBOX_NAMESPACE_END
