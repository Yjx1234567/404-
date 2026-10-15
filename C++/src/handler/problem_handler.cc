#include "handler/problem_handler.h"
#include "service/problem_service.h"
#include "model/problem.h"
#include "model/test_case.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <json/json.h>
#include <mysql/mysql.h>
#include <sstream>
#include <map>

namespace oj {

static Json::Value problemToJson(const Problem& p, bool with_content) {
    Json::Value prob;
    prob["id"] = p.getId();
    prob["title"] = p.getTitle();
    prob["difficulty"] = p.getDifficulty();
    prob["created_at"] = p.getCreatedAt();

    Json::Value tags(Json::arrayValue);
    for (const auto& t : p.getTags()) {
        tags.append(t);
    }
    prob["tags"] = tags;

    if (with_content) {
        prob["content"] = p.getContent();
        prob["template"] = p.getTemplate();
        prob["has_spj"] = p.hasSpj();

        Json::Value test_cases(Json::arrayValue);
        auto tcs = p.getTestCases();
        for (const auto& tc : tcs) {
            Json::Value t;
            t["id"] = tc.getId();
            t["input"] = tc.getInput();
            t["expected"] = tc.getExpected();
            t["position"] = tc.getPosition();
            test_cases.append(t);
        }
        prob["test_cases"] = test_cases;
    }
    return prob;
}

std::pair<int, std::string> handleProblemList(const std::map<std::string, std::string>& query) {
    Json::Value root;

    auto get_str = [&query](const std::string& key) -> std::string {
        auto it = query.find(key);
        return it == query.end() ? std::string() : it->second;
    };
    auto get_int = [&query](const std::string& key, int def) -> int {
        auto it = query.find(key);
        if (it == query.end() || it->second.empty()) return def;
        try {
            return std::stoi(it->second);
        } catch (...) {
            return def;
        }
    };

    ProblemQuery q;
    q.page = get_int("page", 1);
    q.page_size = get_int("page_size", 20);
    q.difficulty = get_str("difficulty");
    q.keyword = get_str("keyword");
    q.tag_id = get_int("tag_id", 0);

    // 非法难度值直接忽略而不是报错，保持列表页容错
    if (!ProblemService::validateDifficulty(q.difficulty)) {
        q.difficulty = "";
    }

    int total = 0;
    auto problem_list = ProblemService::searchProblems(q, total);

    Json::Value problems(Json::arrayValue);
    for (const auto& p : problem_list) {
        problems.append(problemToJson(p, false));
    }

    root["problems"] = problems;
    root["total"] = total;
    root["page"] = q.page < 1 ? 1 : q.page;
    root["page_size"] = q.page_size;

    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleProblemDetail(int id) {
    Json::Value root;

    if (id <= 0) {
        root["error"] = "invalid problem id";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    Problem p = ProblemService::getProblem(id);
    if (p.getId() == 0) {
        root["error"] = "problem not found";
        Json::StreamWriterBuilder builder;
        return {404, Json::writeString(builder, root)};
    }

    root = problemToJson(p, true);

    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleTagsList() {
    Json::Value root;

    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) {
        root["error"] = "database unavailable";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }

    std::string sql = "SELECT t.id, t.name, COUNT(pt.problem_id) AS problem_count "
                      "FROM tags t LEFT JOIN problem_tags pt ON pt.tag_id = t.id "
                      "GROUP BY t.id, t.name ORDER BY t.name ASC";
    Json::Value tags(Json::arrayValue);
    if (mysql_query(conn, sql.c_str()) == 0) {
        MYSQL_RES* res = mysql_store_result(conn);
        if (res) {
            MYSQL_ROW row;
            while ((row = mysql_fetch_row(res))) {
                Json::Value t;
                t["id"] = std::stoi(row[0]);
                t["name"] = row[1] ? row[1] : "";
                t["problem_count"] = std::stoi(row[2] ? row[2] : "0");
                tags.append(t);
            }
            mysql_free_result(res);
        }
    }
    pool.releaseConnection(conn);

    root["tags"] = tags;
    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

} // namespace oj
