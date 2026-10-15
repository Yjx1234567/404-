#include "handler/class_handler.h"
#include "model/submission.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <json/json.h>
#include <mysql/mysql.h>
#include <random>
#include <chrono>
#include <iomanip>
#include <sstream>

namespace oj {

namespace {

std::pair<int, std::string> jsonError(const std::string& msg, int status) {
    Json::Value root;
    root["error"] = msg;
    Json::StreamWriterBuilder builder;
    return {status, Json::writeString(builder, root)};
}

std::pair<int, std::string> jsonOk(const Json::Value& root, int status = 200) {
    Json::StreamWriterBuilder builder;
    return {status, Json::writeString(builder, root)};
}

// 是否为该班老师
bool isClassTeacher(int class_id, int user_id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    std::string sql = "SELECT COUNT(*) FROM classes WHERE id = " + std::to_string(class_id) +
                      " AND teacher_id = " + std::to_string(user_id);
    bool teacher = false;
    if (mysql_query(conn, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            teacher = row && row[0] && std::stoi(row[0]) > 0;
            mysql_free_result(res);
        }
    }
    pool.releaseConnection(conn);
    return teacher;
}

// 是否为该班成员
bool isClassMember(int class_id, int user_id) {
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return false;
    std::string sql = "SELECT COUNT(*) FROM class_members WHERE class_id = " + std::to_string(class_id) +
                      " AND user_id = " + std::to_string(user_id);
    bool member = false;
    if (mysql_query(conn, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            member = row && row[0] && std::stoi(row[0]) > 0;
            mysql_free_result(res);
        }
    }
    pool.releaseConnection(conn);
    return member;
}

// 生成 6 位不重复邀请码
std::string generateJoinCode() {
    static const char charset[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789"; // 去掉易混字符
    static std::mt19937 rng(static_cast<unsigned>(
        std::chrono::system_clock::now().time_since_epoch().count()));
    auto& pool = ConnectionPool::getInstance();
    for (int attempt = 0; attempt < 10; ++attempt) {
        std::string code;
        for (int i = 0; i < 6; ++i) {
            code += charset[rng() % (sizeof(charset) - 1)];
        }
        MYSQL* conn = pool.getConnection();
        if (!conn) return code;
        std::string sql = "SELECT COUNT(*) FROM classes WHERE join_code = '" + code + "'";
        bool exists = false;
        if (mysql_query(conn, sql.c_str()) == 0) {
            MYSQL_RES* res = mysql_store_result(conn);
            if (res) {
                MYSQL_ROW row = mysql_fetch_row(res);
                exists = row && row[0] && std::stoi(row[0]) > 0;
                mysql_free_result(res);
            }
        }
        pool.releaseConnection(conn);
        if (!exists) return code;
    }
    return "OJ" + std::to_string(std::chrono::system_clock::now().time_since_epoch().count() % 1000000);
}

} // namespace

std::pair<int, std::string> handleCreateClass(const std::string& body, int user_id) {
    Json::Reader reader;
    Json::Value request;
    if (!reader.parse(body, request) || !request.isMember("name") ||
        request["name"].asString().empty()) {
        return jsonError("class name is required", 400);
    }

    std::string name = request["name"].asString();
    if (name.size() > 128) {
        return jsonError("class name too long", 400);
    }
    std::string join_code = generateJoinCode();

    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql = "INSERT INTO classes (name, join_code, teacher_id) VALUES ('" +
                      escapeSql(name) + "', '" + join_code + "', " + std::to_string(user_id) + ")";
    if (mysql_query(conn, sql.c_str()) != 0) {
        std::string err = mysql_error(conn);
        pool.releaseConnection(conn);
        Logger::getInstance().error("Create class failed: " + err);
        return jsonError("failed to create class", 500);
    }
    int id = static_cast<int>(mysql_insert_id(conn));
    pool.releaseConnection(conn);

    Json::Value root;
    root["id"] = id;
    root["name"] = name;
    root["join_code"] = join_code;
    root["message"] = "class created successfully";
    return jsonOk(root, 201);
}

std::pair<int, std::string> handleJoinClass(const std::string& body, int user_id) {
    Json::Reader reader;
    Json::Value request;
    if (!reader.parse(body, request) || !request.isMember("join_code") ||
        request["join_code"].asString().empty()) {
        return jsonError("join_code is required", 400);
    }

    std::string join_code = request["join_code"].asString();

    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql = "SELECT id, teacher_id FROM classes WHERE join_code = '" + escapeSql(join_code) + "'";
    int class_id = 0;
    int teacher_id = 0;
    if (mysql_query(conn, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row = mysql_fetch_row(res);
            if (row) {
                class_id = std::stoi(row[0]);
                teacher_id = std::stoi(row[1]);
            }
            mysql_free_result(res);
        }
    }

    if (class_id == 0) {
        pool.releaseConnection(conn);
        return jsonError("class not found", 404);
    }
    if (teacher_id == user_id) {
        pool.releaseConnection(conn);
        return jsonError("you are the teacher of this class", 400);
    }

    std::string ins = "INSERT IGNORE INTO class_members (class_id, user_id) VALUES (" +
                      std::to_string(class_id) + ", " + std::to_string(user_id) + ")";
    if (mysql_query(conn, ins.c_str()) != 0) {
        pool.releaseConnection(conn);
        return jsonError("failed to join class", 500);
    }
    bool already = mysql_affected_rows(conn) == 0;
    pool.releaseConnection(conn);

    Json::Value root;
    root["class_id"] = class_id;
    if (already) {
        root["message"] = "already a member";
    } else {
        root["message"] = "joined successfully";
    }
    return jsonOk(root);
}

std::pair<int, std::string> handleMyClasses(int user_id) {
    Json::Value root;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql =
        "SELECT c.id, c.name, c.join_code, c.teacher_id, u.username, "
        "(c.teacher_id = " + std::to_string(user_id) + ") AS is_teacher "
        "FROM classes c JOIN users u ON u.id = c.teacher_id "
        "WHERE c.teacher_id = " + std::to_string(user_id) +
        " OR c.id IN (SELECT class_id FROM class_members WHERE user_id = " +
        std::to_string(user_id) + ") ORDER BY c.id DESC";

    Json::Value items(Json::arrayValue);
    if (mysql_query(conn, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                Json::Value item;
                item["id"] = std::stoi(row[0]);
                item["name"] = row[1] ? row[1] : "";
                item["join_code"] = row[2] ? row[2] : "";
                item["teacher_id"] = std::stoi(row[3]);
                item["teacher_name"] = row[4] ? row[4] : "";
                item["is_teacher"] = std::stoi(row[5] ? row[5] : "0") > 0;
                items.append(item);
            }
            mysql_free_result(res);
        }
    }
    pool.releaseConnection(conn);

    root["classes"] = items;
    return jsonOk(root);
}

std::pair<int, std::string> handleClassDetail(int class_id, int user_id, bool is_admin) {
    Json::Value root;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql = "SELECT c.id, c.name, c.join_code, c.teacher_id, u.username "
                      "FROM classes c JOIN users u ON u.id = c.teacher_id "
                      "WHERE c.id = " + std::to_string(class_id);
    if (mysql_query(conn, sql.c_str()) != 0) {
        pool.releaseConnection(conn);
        return jsonError("query failed", 500);
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return jsonError("query failed", 500);
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        pool.releaseConnection(conn);
        return jsonError("class not found", 404);
    }

    int teacher_id = std::stoi(row[3]);
    root["id"] = class_id;
    root["name"] = row[1] ? row[1] : "";
    root["join_code"] = row[2] ? row[2] : "";
    root["teacher_id"] = teacher_id;
    root["teacher_name"] = row[4] ? row[4] : "";
    mysql_free_result(res);

    bool teacher = (teacher_id == user_id);
    bool member = isClassMember(class_id, user_id);
    if (!teacher && !member && !is_admin) {
        pool.releaseConnection(conn);
        return jsonError("forbidden", 403);
    }
    root["is_teacher"] = teacher;
    root["is_member"] = member;

    // 成员列表（老师/管理员可见）
    if (teacher || is_admin) {
        std::string msql = "SELECT u.id, u.username, m.joined_at FROM class_members m "
                           "JOIN users u ON u.id = m.user_id WHERE m.class_id = " +
                           std::to_string(class_id) + " ORDER BY m.joined_at ASC";
        Json::Value members(Json::arrayValue);
        if (mysql_query(conn, msql.c_str()) == 0) {
            MYSQL_RES* mres = mysql_store_result(conn);
            if (mres) {
                MYSQL_ROW mrow;
                while ((mrow = mysql_fetch_row(mres))) {
                    Json::Value m;
                    m["user_id"] = std::stoi(mrow[0]);
                    m["username"] = mrow[1] ? mrow[1] : "";
                    m["joined_at"] = mrow[2] ? mrow[2] : "";
                    members.append(m);
                }
                mysql_free_result(mres);
            }
        }
        root["members"] = members;
    }

    pool.releaseConnection(conn);
    return jsonOk(root);
}

std::pair<int, std::string> handleDeleteClass(int class_id, int user_id, bool is_admin) {
    if (!is_admin && !isClassTeacher(class_id, user_id)) {
        return jsonError("forbidden", 403);
    }

    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql = "DELETE FROM classes WHERE id = " + std::to_string(class_id);
    if (mysql_query(conn, sql.c_str()) != 0) {
        pool.releaseConnection(conn);
        return jsonError("failed to delete class", 500);
    }
    bool affected = mysql_affected_rows(conn) > 0;
    pool.releaseConnection(conn);

    if (!affected) return jsonError("class not found", 404);

    Json::Value root;
    root["message"] = "class deleted successfully";
    return jsonOk(root);
}

std::pair<int, std::string> handleCreateAssignment(int class_id, const std::string& body,
                                                   int user_id, bool is_admin) {
    if (!is_admin && !isClassTeacher(class_id, user_id)) {
        return jsonError("forbidden", 403);
    }

    Json::Reader reader;
    Json::Value request;
    if (!reader.parse(body, request) || !request.isMember("title") ||
        request["title"].asString().empty()) {
        return jsonError("assignment title is required", 400);
    }
    if (!request.isMember("problem_ids") || !request["problem_ids"].isArray() ||
        request["problem_ids"].empty()) {
        return jsonError("problem_ids is required (at least one problem)", 400);
    }

    std::string title = request["title"].asString();
    std::string description = request.isMember("description") ? request["description"].asString() : "";
    std::string due_at = request.isMember("due_at") ? request["due_at"].asString() : "";

    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql = "INSERT INTO assignments (class_id, title, description" +
                      std::string(due_at.empty() ? "" : ", due_at") + ") VALUES ('" +
                      std::to_string(class_id) + "', '" + escapeSql(title) + "', '" +
                      escapeSql(description) + "'" +
                      (due_at.empty() ? "" : ", '" + escapeSql(due_at) + "'") + ")";
    if (mysql_query(conn, sql.c_str()) != 0) {
        std::string err = mysql_error(conn);
        pool.releaseConnection(conn);
        Logger::getInstance().error("Create assignment failed: " + err);
        return jsonError("failed to create assignment", 500);
    }
    int assignment_id = static_cast<int>(mysql_insert_id(conn));

    int position = 0;
    for (const auto& pid : request["problem_ids"]) {
        if (!pid.isInt() && !pid.isUInt()) continue;
        std::string ins = "INSERT IGNORE INTO assignment_problems (assignment_id, problem_id, position) VALUES (" +
                          std::to_string(assignment_id) + ", " + std::to_string(pid.asInt()) + ", " +
                          std::to_string(position++) + ")";
        mysql_query(conn, ins.c_str());
    }
    pool.releaseConnection(conn);

    Json::Value root;
    root["id"] = assignment_id;
    root["message"] = "assignment created successfully";
    return jsonOk(root, 201);
}

std::pair<int, std::string> handleListAssignments(int class_id, int user_id) {
    bool teacher = isClassTeacher(class_id, user_id);
    bool member = isClassMember(class_id, user_id);
    // 非成员且非老师直接 403（管理员由路由层放行）
    if (!teacher && !member) {
        return jsonError("forbidden", 403);
    }

    Json::Value root;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql = "SELECT a.id, a.title, a.description, IFNULL(a.due_at,''), a.created_at "
                      "FROM assignments a WHERE a.class_id = " + std::to_string(class_id) +
                      " ORDER BY a.id DESC";
    if (mysql_query(conn, sql.c_str()) != 0) {
        pool.releaseConnection(conn);
        return jsonError("query failed", 500);
    }

    MYSQL_RES* res = mysql_store_result(conn);
    Json::Value items(Json::arrayValue);
    if (res) {
        MYSQL_ROW row;
        while ((row = mysql_fetch_row(res))) {
            Json::Value item;
            int assignment_id = std::stoi(row[0]);
            item["id"] = assignment_id;
            item["title"] = row[1] ? row[1] : "";
            item["description"] = row[2] ? row[2] : "";
            item["due_at"] = row[3] ? row[3] : "";
            item["created_at"] = row[4] ? row[4] : "";

            // 作业内每道题 + 我的完成状态
            std::string psql = "SELECT ap.problem_id, p.title, p.difficulty FROM assignment_problems ap "
                               "JOIN problems p ON p.id = ap.problem_id WHERE ap.assignment_id = " +
                               std::to_string(assignment_id) + " ORDER BY ap.position ASC";
            if (mysql_query(conn, psql.c_str()) == 0) {
                MYSQL_RES* pres = mysql_store_result(conn);
                Json::Value problems(Json::arrayValue);
                int solved_count = 0;
                if (pres) {
                    MYSQL_ROW prow;
                    while ((prow = mysql_fetch_row(pres))) {
                        Json::Value p;
                        int problem_id = std::stoi(prow[0]);
                        p["problem_id"] = problem_id;
                        p["title"] = prow[1] ? prow[1] : "";
                        p["difficulty"] = prow[2] ? prow[2] : "";
                        bool ac = Submission::hasAC(user_id, problem_id);
                        p["solved"] = ac;
                        if (ac) solved_count++;
                        problems.append(p);
                    }
                    mysql_free_result(pres);
                }
                item["problems"] = problems;
                item["solved_count"] = solved_count;
                item["total_count"] = static_cast<int>(problems.size());
            }
            items.append(item);
        }
        mysql_free_result(res);
    }
    pool.releaseConnection(conn);

    root["assignments"] = items;
    return jsonOk(root);
}

std::pair<int, std::string> handleAssignmentDetail(int assignment_id, int user_id, bool is_admin) {
    Json::Value root;
    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) return jsonError("database unavailable", 500);

    std::string sql = "SELECT a.class_id, a.title, a.description, IFNULL(a.due_at,''), a.created_at "
                      "FROM assignments a WHERE a.id = " + std::to_string(assignment_id);
    if (mysql_query(conn, sql.c_str()) != 0) {
        pool.releaseConnection(conn);
        return jsonError("query failed", 500);
    }
    MYSQL_RES* res = mysql_store_result(conn);
    if (!res) {
        pool.releaseConnection(conn);
        return jsonError("query failed", 500);
    }
    MYSQL_ROW row = mysql_fetch_row(res);
    if (!row) {
        mysql_free_result(res);
        pool.releaseConnection(conn);
        return jsonError("assignment not found", 404);
    }

    int class_id = std::stoi(row[0]);
    root["id"] = assignment_id;
    root["class_id"] = class_id;
    root["title"] = row[1] ? row[1] : "";
    root["description"] = row[2] ? row[2] : "";
    root["due_at"] = row[3] ? row[3] : "";
    root["created_at"] = row[4] ? row[4] : "";
    mysql_free_result(res);

    bool teacher = isClassTeacher(class_id, user_id);
    bool member = isClassMember(class_id, user_id);
    if (!teacher && !member && !is_admin) {
        pool.releaseConnection(conn);
        return jsonError("forbidden", 403);
    }
    root["is_teacher"] = teacher;

    // 题目列表
    std::string psql = "SELECT ap.problem_id, p.title, p.difficulty FROM assignment_problems ap "
                       "JOIN problems p ON p.id = ap.problem_id WHERE ap.assignment_id = " +
                       std::to_string(assignment_id) + " ORDER BY ap.position ASC";
    if (mysql_query(conn, psql.c_str()) == 0) {
        MYSQL_RES* pres = mysql_store_result(conn);
        Json::Value problems(Json::arrayValue);
        if (pres) {
            MYSQL_ROW prow;
            while ((prow = mysql_fetch_row(pres))) {
                Json::Value p;
                int problem_id = std::stoi(prow[0]);
                p["problem_id"] = problem_id;
                p["title"] = prow[1] ? prow[1] : "";
                p["difficulty"] = prow[2] ? prow[2] : "";
                p["solved"] = Submission::hasAC(user_id, problem_id);
                problems.append(p);
            }
            mysql_free_result(pres);
        }
        root["problems"] = problems;
    }

    // 老师/管理员：全班完成情况矩阵
    if (teacher || is_admin) {
        // 全体成员
        std::vector<std::pair<int, std::string>> members;
        std::string msql = "SELECT u.id, u.username FROM class_members m JOIN users u ON u.id = m.user_id "
                           "WHERE m.class_id = " + std::to_string(class_id) + " ORDER BY u.id";
        if (mysql_query(conn, msql.c_str()) == 0) {
            MYSQL_RES* mres = mysql_store_result(conn);
            if (mres) {
                MYSQL_ROW mrow;
                while ((mrow = mysql_fetch_row(mres))) {
                    members.emplace_back(std::stoi(mrow[0]), mrow[1] ? mrow[1] : "");
                }
                mysql_free_result(mres);
            }
        }

        // 每个成员对每题的 AC 状态
        Json::Value matrix(Json::arrayValue);
        for (const auto& [uid, uname] : members) {
            Json::Value m;
            m["user_id"] = uid;
            m["username"] = uname;
            Json::Value solved_map(Json::objectValue);
            int solved_count = 0;
            if (mysql_query(conn, psql.c_str()) == 0) {
                MYSQL_RES* pres = mysql_store_result(conn);
                if (pres) {
                    MYSQL_ROW prow;
                    while ((prow = mysql_fetch_row(pres))) {
                        int problem_id = std::stoi(prow[0]);
                        bool ac = Submission::hasAC(uid, problem_id);
                        solved_map[std::to_string(problem_id)] = ac;
                        if (ac) solved_count++;
                    }
                    mysql_free_result(pres);
                }
            }
            m["solved_map"] = solved_map;
            m["solved_count"] = solved_count;
            matrix.append(m);
        }
        root["member_status"] = matrix;
    }

    pool.releaseConnection(conn);
    return jsonOk(root);
}

} // namespace oj
