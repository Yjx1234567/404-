#include "model/session.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <mysql/mysql.h>
#include <random>
#include <sstream>
#include <iomanip>
#include <chrono>

namespace oj {

Session::Session() : id_(0), user_id_(0) {}

Session::Session(int id, int user_id, const std::string& token,
                 const std::string& expires_at, const std::string& created_at)
    : id_(id), user_id_(user_id), token_(token), expires_at_(expires_at), created_at_(created_at) {}

bool Session::isExpired() const {
    if (expires_at_.empty()) return true;
    
    std::tm tm = {};
    std::istringstream ss(expires_at_);
    ss >> std::get_time(&tm, "%Y-%m-%d %H:%M:%S");
    if (ss.fail()) {
        return true;
    }
    auto expiry = std::chrono::system_clock::from_time_t(std::mktime(&tm));
    return std::chrono::system_clock::now() > expiry;
}

bool Session::saveToDB() {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "INSERT INTO sessions (user_id, token, expires_at) VALUES (" +
                        std::to_string(user_id_) + ", '" + escapeSql(token_) + "', '" +
                        escapeSql(expires_at_) + "')";
    
    if (mysql_query(conn, query.c_str()) != 0) {
        Logger::getInstance().error("Session save failed: " + std::string(mysql_error(conn)));
        pool.releaseConnection(conn);
        return false;
    }
    
    id_ = static_cast<int>(mysql_insert_id(conn));
    pool.releaseConnection(conn);
    return true;
}

bool Session::loadFromDB(int id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "SELECT * FROM sessions WHERE id = " + std::to_string(id);
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
    user_id_ = std::stoi(row[1]);
    token_ = row[2] ? row[2] : "";
    expires_at_ = row[3] ? row[3] : "";
    created_at_ = row[4] ? row[4] : "";
    
    mysql_free_result(res);
    pool.releaseConnection(conn);
    return true;
}

bool Session::loadFromDBByToken(const std::string& token) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "SELECT * FROM sessions WHERE token = '" + escapeSql(token) + "'";
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
    user_id_ = std::stoi(row[1]);
    token_ = row[2] ? row[2] : "";
    expires_at_ = row[3] ? row[3] : "";
    created_at_ = row[4] ? row[4] : "";
    
    mysql_free_result(res);
    pool.releaseConnection(conn);
    return true;
}

bool Session::deleteFromDB() {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "DELETE FROM sessions WHERE id = " + std::to_string(id_);
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }
    
    pool.releaseConnection(conn);
    return true;
}

bool Session::deleteByToken() {
    if (token_.empty()) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "DELETE FROM sessions WHERE token = '" + escapeSql(token_) + "'";
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }
    
    pool.releaseConnection(conn);
    return true;
}

Session Session::getByToken(const std::string& token) {
    Session session;
    session.loadFromDBByToken(token);
    return session;
}

std::string Session::generateToken() {
    const char chars[] = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<> dis(0, 61);
    
    std::string token;
    for (int i = 0; i < 32; ++i) {
        token += chars[dis(gen)];
    }
    return token;
}

} // namespace oj
