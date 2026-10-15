#include "utils/logger.h"
#include <iostream>
#include <filesystem>

namespace oj {

Logger& Logger::getInstance() {
    static Logger instance;
    return instance;
}

Logger::Logger() : level_(LogLevel::INFO), console_(true) {}

Logger::~Logger() {
    if (filestream_.is_open()) {
        filestream_.close();
    }
}

void Logger::setLevel(LogLevel level) {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = level;
}

void Logger::setFile(const std::string& filepath) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (filestream_.is_open()) {
        filestream_.close();
    }
    filepath_ = filepath;
    filestream_.open(filepath_, std::ios::app);
}

void Logger::setConsole(bool enabled) {
    std::lock_guard<std::mutex> lock(mutex_);
    console_ = enabled;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::DEBUG, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::INFO, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::WARNING, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::ERROR, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (level < level_) {
        return;
    }
    
    std::string logLine = "[" + getTimestamp() + "] [" + levelToString(level) + "] " + message;
    
    if (console_) {
        std::cout << logLine << std::endl;
    }
    
    if (filestream_.is_open()) {
        filestream_ << logLine << std::endl;
        filestream_.flush();
    }
}

void Logger::reset() {
    std::lock_guard<std::mutex> lock(mutex_);
    level_ = LogLevel::INFO;
    console_ = true;
    filepath_.clear();
    if (filestream_.is_open()) {
        filestream_.close();
    }
}

std::string Logger::levelToString(LogLevel level) const {
    switch (level) {
        case LogLevel::DEBUG:   return "DEBUG";
        case LogLevel::INFO:    return "INFO";
        case LogLevel::WARNING: return "WARNING";
        case LogLevel::ERROR:   return "ERROR";
        default:                return "UNKNOWN";
    }
}

std::string Logger::getTimestamp() const {
    auto now = std::time(nullptr);
    auto tm = *std::localtime(&now);
    std::ostringstream oss;
    oss << std::put_time(&tm, "%Y-%m-%d %H:%M:%S");
    return oss.str();
}

} // namespace oj
