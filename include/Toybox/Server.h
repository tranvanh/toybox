#pragma once
#include "Toybox/Common.h"
#include <boost/asio.hpp>
#include <unordered_set>

TOYBOX_NAMESPACE_BEGIN

class Session;

class Server {
    boost::asio::io_context                    mContext;
    boost::asio::ip::tcp::acceptor              mAcceptor;
    struct {
        std::unordered_set<std::shared_ptr<Session>> data;
        mutable std::mutex                           mtx;
    } mActiveSessions;

public:
    explicit Server(short port);
    ~Server();
    void run();

    std::function<void(std::string)> onRecieve;
    std::function<void(const unsigned short)> onConnect;
    std::function<void(const unsigned short)> onDisconnect;

private:
    void accept();

};

TOYBOX_NAMESPACE_END
