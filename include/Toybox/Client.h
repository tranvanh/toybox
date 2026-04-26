#pragma once
#include "Toybox/Common.h"
#include <boost/asio.hpp>
#include <deque>
#include <functional>

TOYBOX_NAMESPACE_BEGIN

class Client {
    mutable boost::asio::io_context      mContext;
    mutable boost::asio::ip::tcp::socket  mSocket;
    mutable std::deque<std::vector<char>> mSendQueue;
    mutable bool                          mWriteInProgress = false;
    struct {
        std::vector<char> body   = std::vector<char>(BUFSIZ);
        std::size_t       length = 0;
    } mReceiveMessage;

public:
    Client();
    ~Client();
    void run();
    void stop();
    bool connect(const std::string& address, short port);
    // Client send follows a rule of sending a size first and content after
    void sendMessage(const std::string& msg) const;
    void subscribe();

    std::function<void(std::string)> onReceive;

private:
    void readHeader();
    void readBody();
    void doWrite() const;
};

TOYBOX_NAMESPACE_END
