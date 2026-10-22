#include "model/test_case.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <mysql/mysql.h>
#include <sstream>

namespace oj {

TestCase::TestCase() : id_(0), problem_id_(0), position_(0) {}

TestCase::TestCase(int id, int problem_id, const std::string& input,
                   const std::string& expected, int position)
    : id_(id), problem_id_(problem_id), input_(input),
      expected_(expected), position_(position) {}

bool TestCase::saveToDB() {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    // 使用转义后的字符串拼接，防止测试用例中的特殊字符破坏 SQL
    std::string query = "INSERT INTO test_cases (problem_id, input, expected, position) VALUES (" +
                        std::to_string(problem_id_) + ", '" + escapeSql(input_) + "', '" +
                        escapeSql(expected_) + "', " +
                        std::to_string(position_) + ")";

    if (mysql_query(conn, query.c_str()) != 0) {
        Logger::getInstance().error("TestCase save failed: " + std::string(mysql_error(conn)));
        pool.releaseConnection(conn);
        return false;
    }
    
    id_ = static_cast<int>(mysql_insert_id(conn));
    pool.releaseConnection(conn);
    return true;
}

bool TestCase::loadFromDB(int id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "SELECT * FROM test_cases WHERE id = " + std::to_string(id);
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
    problem_id_ = std::stoi(row[1]);
    input_ = row[2] ? row[2] : "";
    expected_ = row[3] ? row[3] : "";
    position_ = std::stoi(row[4]);
    
    mysql_free_result(res);
    pool.releaseConnection(conn);
    return true;
}

bool TestCase::deleteFromDB() {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    
    std::string query = "DELETE FROM test_cases WHERE id = " + std::to_string(id_);
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }
    
    pool.releaseConnection(conn);
    return true;
}

bool TestCase::deleteByProblemId(int problem_id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "DELETE FROM test_cases WHERE problem_id = " + std::to_string(problem_id);
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }

    pool.releaseConnection(conn);
    return true;
}

std::vector<TestCase> TestCase::getByProblemId(int problem_id) {
    std::vector<TestCase> cases;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return cases;
    
    std::string query = "SELECT * FROM test_cases WHERE problem_id = " + std::to_string(problem_id) +
                        " ORDER BY position ASC";
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return cases;
    }
    
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return cases;
    }
    
    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        cases.emplace_back(
            std::stoi(row[0]),
            std::stoi(row[1]),
            row[2] ? row[2] : "",
            row[3] ? row[3] : "",
            std::stoi(row[4])
        );
    }
    
    mysql_free_result(res);
    pool.releaseConnection(conn);
    return cases;
}

} // namespace oj
