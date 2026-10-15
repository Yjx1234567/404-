#include "db/connection_pool.h"
#include "utils/logger.h"
#include <stdexcept>
#include <chrono>

namespace oj {

// 独立于连接池的 SQL 转义：按 MySQL 手册规则转义特殊字符
// （\0 \n \r \ ' " 以及 ZMWUES 序列），保证拼接安全
std::string escapeSql(const std::string& value) {
    std::string out;
    out.reserve(value.size() + 8);
    for (char c : value) {
        switch (c) {
            case '\0': out += "\\0"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\\': out += "\\\\"; break;
            case '\'': out += "\\'"; break;
            case '"':  out += "\\\""; break;
            case '\x1a': out += "\\Z"; break;
            default:   out += c; break;
        }
    }
    return out;
}

ConnectionPool::~ConnectionPool() {
    close();
}

ConnectionPool& ConnectionPool::getInstance() {
    static ConnectionPool instance;
    return instance;
}

bool ConnectionPool::init(const std::string& host, int port,
                          const std::string& username, const std::string& password,
                          const std::string& database, int max_connections) {
    ConnectionConfig config;
    config.host = host;
    config.port = port;
    config.username = username;
    config.password = password;
    config.database = database;
    config.max_connections = max_connections;
    return init(config);
}

bool ConnectionPool::init(const ConnectionConfig& config) {
    std::lock_guard<std::mutex> lock(mutex_);
    if (initialized_) {
        closeLocked();
    }
    
    config_ = config;
    totalCount_ = 0;
    
    for (int i = 0; i < config_.max_connections; ++i) {
        if (!createConnection()) {
            closeLocked();
            return false;
        }
    }
    
    initialized_ = true;
    Logger::getInstance().info("Connection pool initialized with " + std::to_string(totalCount_) + " connections");
    return true;
}

bool ConnectionPool::createConnection() {
    MYSQL* conn = mysql_init(nullptr);
    if (!conn) {
        Logger::getInstance().error("mysql_init failed");
        return false;
    }
    
    unsigned long client_flag = CLIENT_MULTI_STATEMENTS;
    if (!mysql_real_connect(conn, config_.host.c_str(), config_.username.c_str(),
                            config_.password.c_str(), config_.database.c_str(),
                            config_.port, nullptr, client_flag)) {
        Logger::getInstance().error("mysql_real_connect failed: " + std::string(mysql_error(conn)));
        mysql_close(conn);
        return false;
    }
    
    connections_.push_back(conn);
    inUse_.push_back(false);
    totalCount_++;
    return true;
}

MYSQL* ConnectionPool::getConnection() {
    std::unique_lock<std::mutex> lock(mutex_);
    if (!initialized_) {
        // 未初始化（如单元测试场景）返回空指针而非抛异常，
        // 调用方（model/service 层）均有 if (!conn) 兜底
        Logger::getInstance().warning("getConnection called before pool init");
        return nullptr;
    }

    // 等待任一连接空闲（带超时，避免死等：30 秒后抛异常而不是永久挂起）
    bool available = cv_.wait_for(lock, std::chrono::seconds(30), [this]() {
        for (size_t i = 0; i < connections_.size(); ++i) {
            if (!inUse_[i]) return true;
        }
        return false;
    });
    if (!available) {
        throw std::runtime_error("Timeout waiting for a database connection");
    }

    for (size_t i = 0; i < connections_.size(); ++i) {
        if (!inUse_[i]) {
            inUse_[i] = true;
            return connections_[i];
        }
    }

    throw std::runtime_error("No available connection");
}

void ConnectionPool::releaseConnection(MYSQL* conn) {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < connections_.size(); ++i) {
        if (connections_[i] == conn) {
            inUse_[i] = false;
            cv_.notify_one();
            return;
        }
    }
    Logger::getInstance().warning("Released unknown connection");
}

void ConnectionPool::close() {
    std::lock_guard<std::mutex> lock(mutex_);
    closeLocked();
}

void ConnectionPool::closeLocked() {
    for (size_t i = 0; i < connections_.size(); ++i) {
        if (connections_[i]) {
            mysql_close(connections_[i]);
        }
    }
    connections_.clear();
    inUse_.clear();
    totalCount_ = 0;
    initialized_ = false;
}

int ConnectionPool::getAvailableCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    int count = 0;
    for (bool used : inUse_) {
        if (!used) count++;
    }
    return count;
}

int ConnectionPool::getTotalCount() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return totalCount_;
}

bool ConnectionPool::isInitialized() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return initialized_;
}

void ConnectionPool::reset() {
    close();
}

} // namespace oj
