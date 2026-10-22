#include "model/user.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <mysql/mysql.h>

namespace oj {

User::User() : id_(0) {}

User::User(int id, const std::string& username, const std::string& password,
           const std::string& role, const std::string& created_at)
    : id_(id), username_(username), password_(password), role_(role), created_at_(created_at) {}

bool User::saveToDB() {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    // 密码为 bcrypt 哈希（不含特殊字符），用户名/角色仍统一转义
    std::string query = "INSERT INTO users (username, password, role) VALUES ('" +
                        escapeSql(username_) + "', '" + escapeSql(password_) + "', '" +
                        escapeSql(role_) + "')";
    
    if (mysql_query(conn, query.c_str()) != 0) {
        Logger::getInstance().error("User save failed: " + std::string(mysql_error(conn)));
        pool.releaseConnection(conn);
        return false;
    }
    
    id_ = static_cast<int>(mysql_insert_id(conn));
    pool.releaseConnection(conn);
    return true;
}

bool User::loadFromDB(int id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "SELECT * FROM users WHERE id = " + std::to_string(id);
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }
    
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return false;
    }
    
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        pool.releaseConnection(conn);
        return false;
    }
    
    id_ = std::stoi(row[0]);
    username_ = row[1] ? row[1] : "";
    password_ = row[2] ? row[2] : "";
    role_ = row[3] ? row[3] : "user";
    created_at_ = row[4] ? row[4] : "";
    
    mysql_free_result(res);
    pool.releaseConnection(conn);
    return true;
}

bool User::loadFromDBByUsername(const std::string& username) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "SELECT * FROM users WHERE username = '" + escapeSql(username) + "'";
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }
    
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return false;
    }
    
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        pool.releaseConnection(conn);
        return false;
    }
    
    id_ = std::stoi(row[0]);
    username_ = row[1] ? row[1] : "";
    password_ = row[2] ? row[2] : "";
    role_ = row[3] ? row[3] : "user";
    created_at_ = row[4] ? row[4] : "";
    
    mysql_free_result(res);
    pool.releaseConnection(conn);
    return true;
}

bool User::deleteFromDB() {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "DELETE FROM users WHERE id = " + std::to_string(id_);
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }
    
    pool.releaseConnection(conn);
    return true;
}

User User::getByUsername(const std::string& username) {
    User user;
    user.loadFromDBByUsername(username);
    return user;
}

bool User::usernameExists(const std::string& username) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "SELECT COUNT(*) FROM users WHERE username = '" + escapeSql(username) + "'";
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }
    
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return false;
    }
    
    MYSQL_ROW row = mysql_fetch_row(res);
    bool exists = false;
    if (row && row[0]) {
        exists = std::stoi(row[0]) > 0;
    }
    
    mysql_free_result(res);
    pool.releaseConnection(conn);
    return exists;
}

} // namespace oj
