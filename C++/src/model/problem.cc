#include "model/problem.h"
#include "model/test_case.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <mysql/mysql.h>

namespace oj {

Problem::Problem() : id_(0) {}

Problem::Problem(int id, const std::string& title, const std::string& difficulty,
                 const std::string& content, const std::string& template_code,
                 const std::string& created_at)
    : id_(id), title_(title), difficulty_(difficulty), content_(content),
      template_(template_code), created_at_(created_at) {}

// 注意：新列 spj_code 加入后 SELECT * 的列序会变，
// 所有读取一律使用显式列名，避免位置索引错位
bool Problem::saveToDB() {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "INSERT INTO problems (title, difficulty, content, template, spj_code) VALUES ('" +
                        escapeSql(title_) + "', '" + escapeSql(difficulty_) + "', '" +
                        escapeSql(content_) + "', '" + escapeSql(template_) + "', '" +
                        escapeSql(spj_code_) + "')";

    if (mysql_query(conn, query.c_str()) != 0) {
        Logger::getInstance().error("Problem save failed: " + std::string(mysql_error(conn)));
        pool.releaseConnection(conn);
        return false;
    }

    id_ = static_cast<int>(mysql_insert_id(conn));
    pool.releaseConnection(conn);

    if (!tags_.empty()) saveTagsToDB();
    return true;
}

bool Problem::updateToDB() {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "UPDATE problems SET title='" + escapeSql(title_) +
                        "', difficulty='" + escapeSql(difficulty_) +
                        "', content='" + escapeSql(content_) +
                        "', template='" + escapeSql(template_) +
                        "', spj_code='" + escapeSql(spj_code_) +
                        "' WHERE id=" + std::to_string(id_);

    if (mysql_query(conn, query.c_str()) != 0) {
        Logger::getInstance().error("Problem update failed: " + std::string(mysql_error(conn)));
        pool.releaseConnection(conn);
        return false;
    }

    pool.releaseConnection(conn);

    if (!tags_.empty()) saveTagsToDB();
    return true;
}

bool Problem::loadFromDB(int id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "SELECT id, title, difficulty, content, template, IFNULL(spj_code,''), IFNULL(solution,''), created_at "
                        "FROM problems WHERE id = " + std::to_string(id);
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
    title_ = row[1] ? row[1] : "";
    difficulty_ = row[2] ? row[2] : "";
    content_ = row[3] ? row[3] : "";
    template_ = row[4] ? row[4] : "";
    spj_code_ = row[5] ? row[5] : "";
    solution_ = row[6] ? row[6] : "";
    created_at_ = row[7] ? row[7] : "";

    mysql_free_result(res);
    pool.releaseConnection(conn);

    loadTagsFromDB();
    return true;
}

bool Problem::deleteFromDB() {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    std::string query = "DELETE FROM problems WHERE id = " + std::to_string(id_);
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }

    pool.releaseConnection(conn);
    return true;
}

void Problem::loadTagsFromDB() {
    tags_.clear();
    if (id_ == 0) return;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return;

    std::string query = "SELECT t.name FROM tags t "
                        "JOIN problem_tags pt ON t.id = pt.tag_id "
                        "WHERE pt.problem_id = " + std::to_string(id_) + " ORDER BY t.id";
    if (mysql_query(conn, query.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                if (row[0]) tags_.push_back(row[0]);
            }
            mysql_free_result(res);
        }
    }
    pool.releaseConnection(conn);
}

bool Problem::saveTagsToDB() {
    if (id_ == 0) return false;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;

    // 重建关联：先清空旧关联
    std::string del = "DELETE FROM problem_tags WHERE problem_id = " + std::to_string(id_);
    if (mysql_query(conn, del.c_str()) != 0) {
        pool.releaseConnection(conn);
        return false;
    }

    for (const auto& name : tags_) {
        if (name.empty()) continue;
        // 标签不存在则创建
        std::string find = "SELECT id FROM tags WHERE name = '" + escapeSql(name) + "'";
        int tag_id = 0;
        if (mysql_query(conn, find.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                if (row && row[0]) tag_id = std::stoi(row[0]);
                mysql_free_result(res);
            }
        }
        if (tag_id == 0) {
            std::string ins = "INSERT INTO tags (name) VALUES ('" + escapeSql(name) + "')";
            if (mysql_query(conn, ins.c_str()) != 0) continue;
            tag_id = static_cast<int>(mysql_insert_id(conn));
        }
        std::string link = "INSERT IGNORE INTO problem_tags (problem_id, tag_id) VALUES (" +
                           std::to_string(id_) + ", " + std::to_string(tag_id) + ")";
        if (mysql_query(conn, link.c_str()) != 0) {
            Logger::getInstance().warning("Failed to link tag '" + name + "' to problem " + std::to_string(id_));
        }
    }

    pool.releaseConnection(conn);
    return true;
}

std::vector<TestCase> Problem::getTestCases() const {
    return TestCase::getByProblemId(id_);
}

static Problem rowToProblem(MYSQL_ROW row) {
    // 显式列序：id, title, difficulty, content, template, IFNULL(spj_code,''), IFNULL(solution,''), created_at
    Problem p;
    p.setId(std::stoi(row[0]));
    p.setTitle(row[1] ? row[1] : "");
    p.setDifficulty(row[2] ? row[2] : "");
    p.setContent(row[3] ? row[3] : "");
    p.setTemplate(row[4] ? row[4] : "");
    p.setSpjCode(row[5] ? row[5] : "");
    p.setSolution(row[6] ? row[6] : "");
    p.setCreatedAt(row[7] ? row[7] : "");
    return p;
}

static const char* kProblemCols =
    "id, title, difficulty, content, template, IFNULL(spj_code,''), IFNULL(solution,''), created_at";

std::vector<Problem> Problem::getAll() {
    std::vector<Problem> problems;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return problems;

    std::string query = std::string("SELECT ") + kProblemCols + " FROM problems ORDER BY id ASC";
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return problems;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return problems;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        problems.push_back(rowToProblem(row));
    }

    mysql_free_result(res);
    pool.releaseConnection(conn);
    return problems;
}

std::vector<Problem> Problem::getByDifficulty(const std::string& difficulty) {
    std::vector<Problem> problems;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return problems;

    std::string query = std::string("SELECT ") + kProblemCols +
                        " FROM problems WHERE difficulty = '" + escapeSql(difficulty) + "' ORDER BY id ASC";
    if (mysql_query(conn, query.c_str()) != 0) {
        pool.releaseConnection(conn);
        return problems;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return problems;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        problems.push_back(rowToProblem(row));
    }

    mysql_free_result(res);
    pool.releaseConnection(conn);
    return problems;
}

std::vector<Problem> Problem::search(const ProblemQuery& query, int& total) {
    std::vector<Problem> problems;
    total = 0;

    // 拼接 WHERE（全部走 escapeSql，防注入）
    std::string where = "1=1";
    if (!query.difficulty.empty()) {
        where += " AND p.difficulty = '" + escapeSql(query.difficulty) + "'";
    }
    if (!query.keyword.empty()) {
        where += " AND p.title LIKE '%" + escapeSql(query.keyword) + "%'";
    }
    if (query.tag_id > 0) {
        where += " AND EXISTS (SELECT 1 FROM problem_tags pt WHERE pt.problem_id = p.id AND pt.tag_id = " +
                 std::to_string(query.tag_id) + ")";
    }

    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return problems;

    // 总数
    std::string count_sql = "SELECT COUNT(*) FROM problems p WHERE " + where;
    if (mysql_query(conn, count_sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            total = row && row[0] ? std::stoi(row[0]) : 0;
            mysql_free_result(res);
        }
    }

    int page = query.page < 1 ? 1 : query.page;
    int page_size = query.page_size < 1 ? 20 : (query.page_size > 100 ? 100 : query.page_size);
    int offset = (page - 1) * page_size;

    std::string sql = std::string("SELECT ") + kProblemCols + " FROM problems p WHERE " + where +
                      " ORDER BY p.id ASC LIMIT " + std::to_string(page_size) +
                      " OFFSET " + std::to_string(offset);
    if (mysql_query(conn, sql.c_str()) != 0) {
        pool.releaseConnection(conn);
        return problems;
    }

    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return problems;
    }

    MYSQL_ROW row;
    while ((row = mysql_fetch_row(res))) {
        problems.push_back(rowToProblem(row));
    }

    mysql_free_result(res);
    pool.releaseConnection(conn);
    return problems;
}

} // namespace oj
