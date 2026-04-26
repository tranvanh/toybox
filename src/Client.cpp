#include "Toybox/Client.h"
#include "Toybox/Logger.h"
#include "Toybox/NetworkComponent.h"
#include <cstring>

TOYBOX_NAMESPACE_BEGIN

Client::Client() : mSocket(mContext) {
}

Client::~Client() {
    mContext.stop();
}

bool Client::connect(const std::string& address, short port) {
    boost::system::error_code ec;
    mSocket.connect(boost::asio::ip::tcp::endpoint(boost::asio::ip::make_address(address), port), ec);
    if (ec == boost::system::errc::success) {
        readHeader();
    }
    return ec == boost::system::errc::success;
}

void Client::run() {
    mContext.run();
}

void Client::stop() {
    mContext.stop();
}

void Client::subscribe() {
    sendMessage(std::string(kSubscribeMessage));
}

void Client::sendMessage(const std::string& msg) const {
    const std::size_t len = msg.size();
    mSendFrame.resize(sizeof(len) + len);
    std::memcpy(mSendFrame.data(), &len, sizeof(len));
    std::memcpy(mSendFrame.data() + sizeof(len), msg.data(), len);

    boost::asio::async_write(
        mSocket,
        boost::asio::buffer(mSendFrame),
        [](std::error_code ec, std::size_t) {
            if (ec) {
                Logger::instance().log(Logger::LogLevel::ERROR, "Send failed");
            }
        });
}

void Client::readHeader() {
    boost::asio::async_read(
        mSocket,
        boost::asio::buffer(&mReceiveMessage.length, sizeof(mReceiveMessage.length)),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                readBody();
            }
        });
}

void Client::readBody() {
    mReceiveMessage.body.resize(mReceiveMessage.length);
    boost::asio::async_read(
        mSocket,
        boost::asio::buffer(mReceiveMessage.body, mReceiveMessage.length),
        [this](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                if (onReceive) {
                    onReceive(std::string(mReceiveMessage.body.data(), mReceiveMessage.length));
                }
                readHeader();
            }
        });
}

TOYBOX_NAMESPACE_END
