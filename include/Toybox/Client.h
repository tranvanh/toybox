#pragma once
#include "Toybox/Common.h"
#include <boost/asio.hpp>
#include <deque>
#include <functional>

TOYBOX_NAMESPACE_BEGIN

// Async TCP client. Call connect(), set onReceive, then call run() on a
// dedicated thread to drive the Asio event loop.
//
// Wire protocol (length-prefixed frames):
//   [ sizeof(std::size_t) bytes: body length, native endian ][ N bytes: body ]
class Client {
    mutable boost::asio::io_context      mContext;
    mutable boost::asio::ip::tcp::socket  mSocket;
    mutable std::deque<std::vector<char>> mSendQueue;  // pending outbound frames
    mutable bool                          mWriteInProgress = false;
    struct {
        std::vector<char> body   = std::vector<char>(BUFSIZ);
        std::size_t       length = 0;
    } mReceiveMessage;

public:
    Client();
    ~Client();

    // Blocks the calling thread until stop() is called or the context runs out of work.
    void run();
    void stop();

    // Synchronously opens a TCP connection. Returns true on success.
    // Must be called before run().
    bool connect(const std::string& address, short port);

    // Enqueues msg as a length-prefixed frame. Safe to call from any thread.
    // Writes are serialised through the Asio strand — only one async_write is
    // in flight at a time.
    void sendMessage(const std::string& msg) const;

    // Sends the subscribe control message to the server.
    void subscribe();

    // Called on the io_context thread each time a complete message is received.
    std::function<void(std::string)> onReceive;

private:
    void readHeader();    // async-reads the native-size length prefix
    void readBody();      // async-reads exactly mReceiveMessage.length bytes
    void doWrite() const; // pops and writes the front of mSendQueue
};

TOYBOX_NAMESPACE_END
