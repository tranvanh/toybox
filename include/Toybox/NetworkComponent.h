#pragma once
#include "Toybox/Common.h"
#include <boost/asio.hpp>
#include <string_view>

TOYBOX_NAMESPACE_BEGIN

/// Control frame body used by clients to opt into server broadcasts.
inline constexpr std::string_view kSubscribeMessage = "\x01SUBSCRIBE";

/// Common socket base for asynchronous network endpoints.
///
/// The base provides shared ownership support for async handlers and stores the
/// socket/buffer primitives used by concrete client/server session types.
class NetworkComponent : public std::enable_shared_from_this<NetworkComponent> {
public:
    explicit NetworkComponent(boost::asio::ip::tcp::socket socket)
        : mSocket(std::move(socket)) {}

    virtual ~NetworkComponent() = default;

    /// Starts endpoint-specific async work.
    virtual void start() = 0;

    /// Schedules or continues an asynchronous read.
    virtual void read() = 0;

    /// Schedules endpoint-specific write work.
    virtual void write() = 0;

protected:
    boost::asio::streambuf mBuffer;
    boost::asio::ip::tcp::socket     mSocket;
};

TOYBOX_NAMESPACE_END
