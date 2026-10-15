#pragma once

#include <string>
#include <mutex>
#include <fstream>
#include <iostream>
#include <ctime>
#include <sstream>
#include <iomanip>

namespace oj {

enum class LogLevel {
    DEBUG = 0,
    INFO = 1,
    WARNING = 2,
    ERROR = 3
};

class Logger {
public:
    static Logger& getInstance();
    
    void setLevel(LogLevel level);
    void setFile(const std::string& filepath);
    void setConsole(bool enabled);
    
    void debug(const std::string& message);
    void info(const std::string& message);
    void warning(const std::string& message);
    void error(const std::string& message);
    
    void log(LogLevel level, const std::string& message);
    
    void reset();
    
private:
    Logger();
    ~Logger();
    Logger(const Logger&) = delete;
    Logger& operator=(const Logger&) = delete;
    
    std::string levelToString(LogLevel level) const;
    std::string getTimestamp() const;
    
    mutable std::mutex mutex_;
    LogLevel level_;
    std::string filepath_;
    bool console_;
    std::ofstream filestream_;
};

} // namespace oj
