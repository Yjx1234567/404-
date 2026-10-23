#include <gtest/gtest.h>
#include "utils/logger.h"
#include <sstream>
#include <filesystem>

using namespace oj;

TEST(LoggerTest, Singleton) {
    Logger& l1 = Logger::getInstance();
    Logger& l2 = Logger::getInstance();
    EXPECT_EQ(&l1, &l2);
}

TEST(LoggerTest, SetLevel) {
    Logger::getInstance().reset();
    Logger::getInstance().setLevel(LogLevel::DEBUG);
    Logger::getInstance().setConsole(false);
    
    // Test that debug level is accepted
    EXPECT_NO_THROW(Logger::getInstance().debug("test message"));
    EXPECT_NO_THROW(Logger::getInstance().info("test message"));
    EXPECT_NO_THROW(Logger::getInstance().warning("test message"));
    EXPECT_NO_THROW(Logger::getInstance().error("test message"));
    
    Logger::getInstance().reset();
}

TEST(LoggerTest, LogLevelFiltering) {
    Logger::getInstance().reset();
    Logger::getInstance().setLevel(LogLevel::ERROR);
    Logger::getInstance().setConsole(false);
    
    // Debug, info, warning should be filtered out at ERROR level
    // We just verify no exceptions are thrown
    EXPECT_NO_THROW(Logger::getInstance().debug("debug"));
    EXPECT_NO_THROW(Logger::getInstance().info("info"));
    EXPECT_NO_THROW(Logger::getInstance().warning("warning"));
    EXPECT_NO_THROW(Logger::getInstance().error("error"));
    
    Logger::getInstance().reset();
}
