#pragma once
#include "Toybox/Common.h"
#include "Toybox/Serialization.h"
#include <chrono>
#include <iostream>
#include <mutex>
#include <sstream>

TOYBOX_NAMESPACE_BEGIN

/// Process-wide thread-safe logger with timestamped stream output.
class Logger {
public:
    enum class LogLevel { DEBUG, INFO, WARNING, ERROR };

private:
    Logger()  = default;
    ~Logger() = default;

    struct {
        std::ostream* stream = &std::cout;
        std::mutex    lock;
    } mOutput;
    LogLevel mLogLevel = LogLevel::DEBUG;

public:
    /// Returns the singleton logger instance.
    static Logger& instance();

    /// Redirects subsequent log output to out. The caller owns the stream.
    void           setOutputStream(std::ostream& out);

    /// Suppresses messages below logLevel.
    void           setLevel(LogLevel logLevel) { mLogLevel = logLevel; }

    /// Formats all arguments into one log line guarded by the output mutex.
    template <typename... Args>
    void log(LogLevel logLevel, Args&&... args) {
        if (logLevel < mLogLevel) {
            return;
        }
        std::ostringstream stream;
        (stream << ... << args); // Fold expression (C++17)

        auto                        now = std::chrono::system_clock::now();
        std::lock_guard<std::mutex> lock(mOutput.lock);
        *mOutput.stream << now << " [" << levelToString(logLevel) << "] " << stream.str() << std::endl;
    }

private:
    std::string levelToString(LogLevel level);
};

TOYBOX_NAMESPACE_END
