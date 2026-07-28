#include "Toybox/Server.h"
#include "Toybox/Logger.h"
#include "Toybox/NetworkComponent.h"
#include <algorithm>
#include <atomic>
#include <cstring>
#include <deque>
#include <vector>

TOYBOX_NAMESPACE_BEGIN

class Session final : public NetworkComponent {
    friend class Server;

    // Serializes this session's handlers even though the server io_context may
    // run on multiple threads.
    boost::asio::strand<boost::asio::any_io_executor> mStrand;
    Server&            mServer;
    unsigned short     mRemotePort = 0;
    std::atomic<bool>  mIsSubscriber{false};

    struct {
        std::vector<char> body   = std::vector<char>(BUFSIZ);
        std::size_t       length = 0;
    } mMessage;

    std::deque<std::vector<char>> mSendQueue;
    bool                          mWriteInProgress = false;

public:
    explicit Session(boost::asio::ip::tcp::socket socket, Server& server)
        : NetworkComponent(std::move(socket))
        , mStrand(mSocket.get_executor())
        , mServer(server) {}

    virtual ~Session() = default;
    virtual void start() override;
    virtual void read() override;

    virtual void write() override {}

    void post(const std::string& msg);

private:
    void readHeader();
    void readBody();
    void doWrite();
};

void Session::start() {
    mRemotePort = mSocket.remote_endpoint().port();
    read();
}

void Session::read() {
    readHeader();
}

void Session::readHeader() {
    // Hold shared ownership until the async handler runs. Without self, the
    // server could erase the session while an operation is still outstanding.
    auto self = shared_from_this();
    boost::asio::async_read(
        mSocket,
        boost::asio::buffer(&mMessage.length, sizeof(mMessage.length)),
        boost::asio::bind_executor(mStrand, [this, self](boost::system::error_code ec, std::size_t) {
            if (!ec) {
                readBody();
            } else {
                mServer.removeSession(this);
                if (mServer.onDisconnect)
                    mServer.onDisconnect(mRemotePort);
            }
        }));
}

void Session::readBody() {
    auto self = shared_from_this();
    mMessage.body.resize(mMessage.length);

    boost::asio::async_read(
        mSocket,
        boost::asio::buffer(mMessage.body, mMessage.length * sizeof(char)),
        boost::asio::bind_executor(mStrand, [this, self](boost::system::error_code ec, [[maybe_unused]] std::size_t length) {
            if (!ec) {
                std::string_view body(mMessage.body.data(), mMessage.length);
                // Subscribe is a control frame; all other frames are delivered
                // to user code as application messages.
                if (body == kSubscribeMessage) {
                    mIsSubscriber.store(true, std::memory_order_relaxed);
                } else if (mServer.onRecieve) {
                    mServer.onRecieve(std::string(body));
                }
                readHeader();
            } else {
                mServer.removeSession(this);
                if (mServer.onDisconnect) {
                    mServer.onDisconnect(mRemotePort);
                }
            }
        }));
}

void Session::post(const std::string& msg) {
    const std::size_t len = msg.size();
    // Server and client share the same length-prefixed frame layout.
    std::vector<char> frame(sizeof(len) + len);
    std::memcpy(frame.data(), &len, sizeof(len));
    std::memcpy(frame.data() + sizeof(len), msg.data(), len);

    auto self = shared_from_this();
    boost::asio::post(mStrand, [this, self, frame = std::move(frame)]() mutable {
        mSendQueue.push_back(std::move(frame));
        if (!mWriteInProgress) {
            doWrite();
        }
    });
}

void Session::doWrite() {
    mWriteInProgress = true;
    auto self = shared_from_this();
    boost::asio::async_write(
        mSocket,
        boost::asio::buffer(mSendQueue.front()),
        boost::asio::bind_executor(mStrand, [this, self](boost::system::error_code ec, std::size_t) {
            mSendQueue.pop_front();
            if (!ec) {
                if (!mSendQueue.empty()) {
                    doWrite();
                } else {
                    mWriteInProgress = false;
                }
            } else {
                mWriteInProgress = false;
                Logger::instance().log(Logger::LogLevel::ERROR, "Session write failed");
            }
        }));
}

// ========================================================
// Server
// ========================================================

Server::Server(short port, unsigned threadCount)
    : mAcceptor(mContext, boost::asio::ip::tcp::endpoint(boost::asio::ip::tcp::v4(), port))
    , mThreadCount(std::max(1u, threadCount)) {}

Server::~Server() {
    // Stopping the context releases outstanding async operations so worker
    // threads can exit their run() calls.
    mContext.stop();
    for (auto& t : mThreads)
        t.join();
}

void Server::run() {
    // Start accepting before the worker threads enter the io_context.
    accept();
    mThreads.reserve(mThreadCount);
    for (unsigned i = 0; i < mThreadCount; ++i) {
        mThreads.emplace_back([this] { mContext.run(); });
    }
}

void Server::broadcast(const std::string& msg) {
    std::shared_lock lock(mActiveSessions.mtx);
    for (const auto& session : mActiveSessions.data) {
        if (session->mIsSubscriber.load(std::memory_order_relaxed))
            session->post(msg);
    }
}

void Server::removeSession(Session* session) {
    std::unique_lock lock(mActiveSessions.mtx);
    // Sessions are stored by shared_ptr, while async handlers report the raw
    // this pointer they already own.
    mActiveSessions.data.erase(
        std::find_if(mActiveSessions.data.begin(), mActiveSessions.data.end(),
                     [session](const std::shared_ptr<Session>& s) { return s.get() == session; }));
}

std::size_t Server::sessionCount() const {
    std::shared_lock lock(mActiveSessions.mtx);
    return mActiveSessions.data.size();
}

std::size_t Server::subscriberCount() const {
    std::shared_lock lock(mActiveSessions.mtx);
    return std::count_if(mActiveSessions.data.begin(), mActiveSessions.data.end(),
                         [](const std::shared_ptr<Session>& s) {
                             return s->mIsSubscriber.load(std::memory_order_relaxed);
                         });
}

void Server::accept() {
    mAcceptor.async_accept([this](boost::system::error_code ec, boost::asio::ip::tcp::socket socket) {
        if (!ec) {
            const auto           remote_endpoint = socket.remote_endpoint();
            const unsigned short port            = remote_endpoint.port();
            auto session = std::make_shared<Session>(std::move(socket), *this);
            {
                std::unique_lock lock(mActiveSessions.mtx);
                mActiveSessions.data.insert(session);
            }
            if (onConnect) {
                onConnect(port);
            }
            session->start();
        }
        // Keep the accept loop alive after each completion.
        accept();
    });
}

TOYBOX_NAMESPACE_END
