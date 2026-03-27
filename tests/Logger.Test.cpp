#include "Toybox/Logger.h"
#include <gtest/gtest.h>
#include <sstream>

TOYBOX_NAMESPACE_BEGIN

class LoggerTest : public ::testing::Test {
protected:
    std::ostringstream oss;

    void SetUp() override {
        Logger::instance().setOutputStream(oss);
        Logger::instance().setLevel(Logger::LogLevel::DEBUG);
    }

    void TearDown() override {
        Logger::instance().setOutputStream(std::cout);
        Logger::instance().setLevel(Logger::LogLevel::DEBUG);
    }
};

TEST_F(LoggerTest, singleton) {
    EXPECT_EQ(&Logger::instance(), &Logger::instance());
}

TEST_F(LoggerTest, outputRedirected) {
    Logger::instance().log(Logger::LogLevel::INFO, "hello");
    EXPECT_NE(oss.str().find("hello"), std::string::npos);
}

TEST_F(LoggerTest, levelDebugInOutput) {
    Logger::instance().log(Logger::LogLevel::DEBUG, "msg");
    EXPECT_NE(oss.str().find("DEBUG"), std::string::npos);
}

TEST_F(LoggerTest, levelInfoInOutput) {
    Logger::instance().log(Logger::LogLevel::INFO, "msg");
    EXPECT_NE(oss.str().find("INFO"), std::string::npos);
}

TEST_F(LoggerTest, levelWarningInOutput) {
    Logger::instance().log(Logger::LogLevel::WARNING, "msg");
    EXPECT_NE(oss.str().find("WARNING"), std::string::npos);
}

TEST_F(LoggerTest, levelErrorInOutput) {
    Logger::instance().log(Logger::LogLevel::ERROR, "msg");
    EXPECT_NE(oss.str().find("ERROR"), std::string::npos);
}

TEST_F(LoggerTest, filtersBelowLevel) {
    Logger::instance().setLevel(Logger::LogLevel::WARNING);
    Logger::instance().log(Logger::LogLevel::DEBUG, "debug msg");
    Logger::instance().log(Logger::LogLevel::INFO, "info msg");
    EXPECT_TRUE(oss.str().empty());
}

TEST_F(LoggerTest, allowsAtOrAboveLevel) {
    Logger::instance().setLevel(Logger::LogLevel::WARNING);
    Logger::instance().log(Logger::LogLevel::WARNING, "warning msg");
    Logger::instance().log(Logger::LogLevel::ERROR, "error msg");
    EXPECT_NE(oss.str().find("warning msg"), std::string::npos);
    EXPECT_NE(oss.str().find("error msg"), std::string::npos);
}

TEST_F(LoggerTest, multipleArgsConcatenated) {
    Logger::instance().log(Logger::LogLevel::INFO, "foo", 42, "bar");
    EXPECT_NE(oss.str().find("foo42bar"), std::string::npos);
}

TEST_F(LoggerTest, emptyMessage) {
    Logger::instance().log(Logger::LogLevel::INFO, "");
    EXPECT_FALSE(oss.str().empty()); // Timestamp and level are still written
}

TOYBOX_NAMESPACE_END
