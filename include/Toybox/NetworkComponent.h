#pragma once
#include "Toybox/Common.h"
#include <boost/asio.hpp>
#include <string_view>

TOYBOX_NAMESPACE_BEGIN

inline constexpr std::string_view kSubscribeMessage = "\x01SUBSCRIBE";

class NetworkComponent : public std::enable_shared_from_this<NetworkComponent> {
public:
    explicit NetworkComponent(boost::asio::ip::tcp::socket socket)
        : mSocket(std::move(socket)) {}

    virtual ~NetworkComponent() = default;
    virtual void start() = 0;
    virtual void read() = 0;
    virtual void write() = 0;

protected:
    boost::asio::streambuf mBuffer;
    boost::asio::ip::tcp::socket     mSocket;
};

TOYBOX_NAMESPACE_END
