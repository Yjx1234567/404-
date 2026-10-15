#pragma once

#include <mysql/mysql.h>
#include <string>
#include <vector>
#include <mutex>
#include <condition_variable>
#include <memory>

namespace oj {

// SQL 转义工具：将用户输入安全转义后嵌入 SQL 字符串字面量
// （处理 \ ' " \n \r 等特殊字符，防止 SQL 注入与拼接错误）
std::string escapeSql(const std::string& value);

struct ConnectionConfig {
    std::string host;
    int port;
    std::string username;
    std::string password;
    std::string database;
    int max_connections;
};

class ConnectionPool {
public:
    static ConnectionPool& getInstance();
    
    bool init(const std::string& host, int port,
              const std::string& username, const std::string& password,
              const std::string& database, int max_connections);
    
    bool init(const ConnectionConfig& config);
    
    MYSQL* getConnection();
    void releaseConnection(MYSQL* conn);
    
    void close();
    
    int getAvailableCount() const;
    int getTotalCount() const;
    bool isInitialized() const;
    
    void reset();
    
private:
    ConnectionPool() : initialized_(false), totalCount_(0) {}
    ~ConnectionPool();
    ConnectionPool(const ConnectionPool&) = delete;
    ConnectionPool& operator=(const ConnectionPool&) = delete;
    
    void closeLocked();
    
    bool createConnection();
    
    mutable std::mutex mutex_;
    std::condition_variable cv_;
    
    std::vector<MYSQL*> connections_;
    std::vector<bool> inUse_;
    
    ConnectionConfig config_;
    bool initialized_;
    int totalCount_;
};

} // namespace oj
