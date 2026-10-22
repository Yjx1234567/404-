#include "model/submission.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <mysql/mysql.h>

namespace oj {

Submission::Submission()
    : id_(0), user_id_(0), problem_id_(0), passed_(0), total_(0), time_ms_(0) {}

bool Submission::saveToDB() {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "INSERT INTO submissions (user_id, problem_id, code, status) VALUES (" +
                        std::to_string(user_id_) + ", " + std::to_string(problem_id_) + ", '" +
                        escapeSql(code_) + "', '" + escapeSql(status_.empty() ? "PENDING" : status_) + "')";

    if (mysql_query(conn, query.c_str()) != 0) {
        Logger::getInstance().error("Submission save failed: " + std::string(mysql_error(conn)));
        pool.releaseConnection(conn);
        return false;
    }

    id_ = static_cast<int>(mysql_insert_id(conn));
    pool.releaseConnection(conn);
    return true;
}

bool Submission::loadFromDB(int id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "SELECT id, user_id, problem_id, code, status, passed, total, "
                        "time_ms, IFNULL(error_info,''), IFNULL(result,''), created_at "
                        "FROM submissions WHERE id = " + std::to_string(id);
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
    problem_id_ = std::stoi(row[2]);
    code_ = row[3] ? row[3] : "";
    status_ = row[4] ? row[4] : "";
    passed_ = std::stoi(row[5] ? row[5] : "0");
    total_ = std::stoi(row[6] ? row[6] : "0");
    time_ms_ = std::stoi(row[7] ? row[7] : "0");
    error_info_ = row[8] ? row[8] : "";
    result_ = row[9] ? row[9] : "";
    created_at_ = row[10] ? row[10] : "";

    mysql_free_result(res);
    pool.releaseConnection(conn);
    return true;
}

bool Submission::updateResult(const std::string& status, int passed, int total,
                              int time_ms, const std::string& error_info,
                              const std::string& result_json) {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "UPDATE submissions SET status='" + escapeSql(status) +
                        "', passed=" + std::to_string(passed) +
                        ", total=" + std::to_string(total) +
                        ", time_ms=" + std::to_string(time_ms) +
                        ", error_info='" + escapeSql(error_info) +
                        "', result='" + escapeSql(result_json) +
                        "' WHERE id=" + std::to_string(id_);

    if (mysql_query(conn, query.c_str()) != 0) {
        Logger::getInstance().error("Submission updateResult failed: " + std::string(mysql_error(conn)));
        pool.releaseConnection(conn);
        return false;
    }

    pool.releaseConnection(conn);
    return true;
}

bool Submission::updateStatus(const std::string& status) {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "UPDATE submissions SET status='" + escapeSql(status) +
                        "' WHERE id=" + std::to_string(id_);
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }

    pool.releaseConnection(conn);
    return true;
}

static Submission rowToSubmission(MYSQL_ROW row) {
    Submission s;
    s.setId(std::stoi(row[0]));
    s.setUserId(std::stoi(row[1]));
    s.setProblemId(std::stoi(row[2]));
    s.setStatus(row[3] ? row[3] : "");
    s.setPassed(std::stoi(row[4] ? row[4] : "0"));
    s.setTotal(std::stoi(row[5] ? row[5] : "0"));
    s.setTimeMs(std::stoi(row[6] ? row[6] : "0"));
    s.setCreatedAt(row[7] ? row[7] : "");
    return s;
}

std::vector<Submission> Submission::getByUser(int user_id, int page, int page_size,
                                              int problem_id) {
    std::vector<Submission> list;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return list;

    std::string filter = "user_id = " + std::to_string(user_id);
    if (problem_id > 0) {
        filter += " AND problem_id = " + std::to_string(problem_id);
    }

    int offset = (page - 1) * page_size;
    std::string query = "SELECT id, user_id, problem_id, status, passed, total, time_ms, created_at "
                        "FROM submissions WHERE " + filter +
                        " ORDER BY id DESC LIMIT " + std::to_string(page_size) +
                        " OFFSET " + std::to_string(offset);

    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return list;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return list;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        list.push_back(rowToSubmission(row));
    }

    mysql_free_result(res);
    pool.releaseConnection(conn);
    return list;
}

static int countQuery(const std::string& sql) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return 0;

    if (mysql_query(conn, sql.c_str()) != 0) {
        pool.releaseConnection(conn);
        return 0;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return 0;
    }

    MYSQL_ROW row = mysql_fetch_row(res);
    int count = row && row[0] ? std::stoi(row[0]) : 0;

    mysql_free_result(res);
    pool.releaseConnection(conn);
    return count;
}

int Submission::countByUser(int user_id, int problem_id) {
    std::string filter = "user_id = " + std::to_string(user_id);
    if (problem_id > 0) {
        filter += " AND problem_id = " + std::to_string(problem_id);
    }
    return countQuery("SELECT COUNT(*) FROM submissions WHERE " + filter);
}

int Submission::countSolvedProblems(int user_id) {
    return countQuery("SELECT COUNT(DISTINCT problem_id) FROM submissions "
                      "WHERE user_id = " + std::to_string(user_id) + " AND status = 'AC'");
}

int Submission::countAttemptedProblems(int user_id) {
    return countQuery("SELECT COUNT(DISTINCT problem_id) FROM submissions "
                      "WHERE user_id = " + std::to_string(user_id));
}

int Submission::countSubmissions(int user_id) {
    return countQuery("SELECT COUNT(*) FROM submissions WHERE user_id = " +
                      std::to_string(user_id));
}

int Submission::countSolvedByDifficulty(int user_id, const std::string& difficulty) {
    std::string sql = "SELECT COUNT(DISTINCT s.problem_id) FROM submissions s "
                      "JOIN problems p ON s.problem_id = p.id "
                      "WHERE s.user_id = " + std::to_string(user_id) + " AND s.status = 'AC'";
    if (!difficulty.empty()) {
        sql += " AND p.difficulty = '" + escapeSql(difficulty) + "'";
    }
    return countQuery(sql);
}

bool Submission::hasAC(int user_id, int problem_id) {
    return countQuery("SELECT COUNT(*) FROM submissions WHERE user_id = " +
                      std::to_string(user_id) + " AND problem_id = " +
                      std::to_string(problem_id) + " AND status = 'AC'") > 0;
}

std::vector<Submission> Submission::getUnfinished() {
    std::vector<Submission> list;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return list;

    std::string query = "SELECT id, user_id, problem_id, status, passed, total, time_ms, created_at "
                        "FROM submissions WHERE status IN ('PENDING','JUDGING') ORDER BY id ASC";
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return list;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return list;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        list.push_back(rowToSubmission(row));
    }

    mysql_free_result(res);
    pool.releaseConnection(conn);
    return list;
}

} // namespace oj
