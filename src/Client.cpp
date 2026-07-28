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
        // Prime the read loop before run() starts dispatching handlers.
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
    // Frame layout: [native-size length header][body].
    std::vector<char> frame(sizeof(len) + len);
    std::memcpy(frame.data(), &len, sizeof(len));
    std::memcpy(frame.data() + sizeof(len), msg.data(), len);
    boost::asio::post(mContext.get_executor(), [this, frame = std::move(frame)]() mutable {
        mSendQueue.push_back(std::move(frame));
        // Keep exactly one write in flight; completion handlers continue the
        // queue until it is empty.
        if (!mWriteInProgress)
            doWrite();
    });
}

void Client::doWrite() const {
    mWriteInProgress = true;
    boost::asio::async_write(
        mSocket,
        boost::asio::buffer(mSendQueue.front()),
        [this](boost::system::error_code ec, std::size_t) {
            mSendQueue.pop_front();
            if (!ec) {
                if (!mSendQueue.empty()) doWrite();
                else mWriteInProgress = false;
            } else {
                mWriteInProgress = false;
                Logger::instance().log(Logger::LogLevel::ERROR, "Send failed");
            }
        });
}

void Client::readHeader() {
    // The header is read directly into the reusable receive metadata.
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
    // Resize the reusable body buffer to match the just-read frame length.
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
