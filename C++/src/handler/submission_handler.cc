#include "handler/submission_handler.h"
#include "service/executor_service.h"
#include "service/judge_queue.h"
#include "service/rate_limiter.h"
#include "service/problem_service.h"
#include "model/submission.h"
#include "model/problem.h"
#include "db/connection_pool.h"
#include "utils/logger.h"
#include <json/json.h>
#include <mysql/mysql.h>
#include <sstream>

namespace oj {

// 判题状态对应的提示文案（与旧版同步判题接口保持一致）
static const char* statusMessage(const std::string& status) {
    if (status == "AC")  return "All test cases passed";
    if (status == "WA")  return "Wrong Answer";
    if (status == "TLE") return "Time Limit Exceeded";
    if (status == "MLE") return "Memory Limit Exceeded";
    if (status == "RE")  return "Runtime Error";
    if (status == "CE")  return "Compilation Error";
    if (status == "SE")  return "System Error";
    if (status == "PENDING" || status == "JUDGING") return "Judging";
    return "Unknown";
}

std::pair<int, std::string> handleSubmit(const std::string& body, int user_id) {
    Json::Value root;
    Json::Reader reader;
    Json::Value request;

    if (!reader.parse(body, request)) {
        root["error"] = "invalid json";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    if (!request.isMember("code") || request["code"].asString().empty()) {
        root["error"] = "code is required";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    if (!request.isMember("problem_id")) {
        root["error"] = "problem_id is required";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    std::string code = request["code"].asString();
    int problem_id = request["problem_id"].asInt();

    if (problem_id <= 0) {
        root["error"] = "invalid problem id";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    // 代码长度上限 64KB
    if (code.size() > 64 * 1024) {
        root["error"] = "code too large (max 64KB)";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    // Verify problem exists
    Problem p = ProblemService::getProblem(problem_id);
    if (p.getId() == 0) {
        root["error"] = "problem not found";
        Json::StreamWriterBuilder builder;
        return {404, Json::writeString(builder, root)};
    }

    // 频率限制
    if (!RateLimiter::getInstance().allow(user_id)) {
        root["error"] = "too many submissions, please wait a moment";
        Json::StreamWriterBuilder builder;
        return {429, Json::writeString(builder, root)};
    }

    // 落库 + 入队，立即返回
    Submission submission;
    submission.setUserId(user_id);
    submission.setProblemId(problem_id);
    submission.setCode(code);
    submission.setStatus("PENDING");

    if (!submission.saveToDB()) {
        root["error"] = "failed to save submission";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }

    JudgeQueue::getInstance().enqueue(submission.getId(), code, problem_id);

    root["submission_id"] = submission.getId();
    root["status"] = "PENDING";
    root["message"] = "Judging";
    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleSubmissionDetail(int id, int user_id, bool is_admin) {
    Json::Value root;

    if (id <= 0) {
        root["error"] = "invalid submission id";
        Json::StreamWriterBuilder builder;
        return {400, Json::writeString(builder, root)};
    }

    Submission s;
    if (!s.loadFromDB(id)) {
        root["error"] = "submission not found";
        Json::StreamWriterBuilder builder;
        return {404, Json::writeString(builder, root)};
    }

    if (!is_admin && s.getUserId() != user_id) {
        root["error"] = "forbidden";
        Json::StreamWriterBuilder builder;
        return {403, Json::writeString(builder, root)};
    }

    root["id"] = s.getId();
    root["user_id"] = s.getUserId();
    root["problem_id"] = s.getProblemId();
    root["status"] = s.getStatus();
    root["message"] = statusMessage(s.getStatus());
    root["passed"] = s.getPassed();
    root["total"] = s.getTotal();
    root["time_ms"] = s.getTimeMs();
    root["created_at"] = s.getCreatedAt();
    if (!s.getErrorInfo().empty()) {
        root["error_info"] = s.getErrorInfo();
    }
    if (!s.getCode().empty()) {
        root["code"] = s.getCode();
    }

    // result 列存的是 details JSON 数组，解析后并入响应
    if (!s.getResult().empty()) {
        Json::Value details;
        Json::Reader r;
        if (r.parse(s.getResult(), details) && details.isArray()) {
            root["details"] = details;
        }
    }

    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleSubmissionList(const std::map<std::string, std::string>& query,
                                                 int user_id, bool is_admin) {
    Json::Value root;

    auto get_int = [&query](const std::string& key, int def) -> int {
        auto it = query.find(key);
        if (it == query.end() || it->second.empty()) return def;
        try {
            return std::stoi(it->second);
        } catch (...) {
            return def;
        }
    };

    // 管理员可通过 user_id 查看他人记录，普通用户只能看自己
    int target_user = user_id;
    if (is_admin) {
        target_user = get_int("user_id", user_id);
    }

    int problem_id = get_int("problem_id", 0);
    int page = get_int("page", 1);
    int page_size = get_int("page_size", 20);
    if (page < 1) page = 1;
    if (page_size < 1 || page_size > 100) page_size = 20;

    auto list = Submission::getByUser(target_user, page, page_size, problem_id);
    int total = Submission::countByUser(target_user, problem_id);

    Json::Value items(Json::arrayValue);
    for (const auto& s : list) {
        Json::Value item;
        item["id"] = s.getId();
        item["user_id"] = s.getUserId();
        item["problem_id"] = s.getProblemId();
        item["status"] = s.getStatus();
        item["passed"] = s.getPassed();
        item["total"] = s.getTotal();
        item["time_ms"] = s.getTimeMs();
        item["created_at"] = s.getCreatedAt();

        // 附带题目标题，前端列表直接展示
        Problem p;
        if (p.loadFromDB(s.getProblemId())) {
            item["problem_title"] = p.getTitle();
        } else {
            item["problem_title"] = "";
        }
        items.append(item);
    }

    root["submissions"] = items;
    root["total"] = total;
    root["page"] = page;
    root["page_size"] = page_size;

    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleUserStats(int user_id) {
    Json::Value root;

    root["solved"] = Submission::countSolvedProblems(user_id);
    root["attempted"] = Submission::countAttemptedProblems(user_id);
    root["submissions"] = Submission::countSubmissions(user_id);

    Json::Value by_diff(Json::objectValue);
    by_diff["Easy"] = Submission::countSolvedByDifficulty(user_id, "Easy");
    by_diff["Medium"] = Submission::countSolvedByDifficulty(user_id, "Medium");
    by_diff["Hard"] = Submission::countSolvedByDifficulty(user_id, "Hard");
    root["solved_by_difficulty"] = by_diff;

    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

std::pair<int, std::string> handleLeaderboard() {
    Json::Value root;

    auto& pool = ConnectionPool::getInstance();
    MYSQL* conn = pool.getConnection();
    if (!conn) {
        root["error"] = "database unavailable";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }

    // 全体用户按 AC 题数排行；并列时提交次数少者优先
    std::string sql =
        "SELECT u.id, u.username, "
        "COUNT(DISTINCT CASE WHEN s.status = 'AC' THEN s.problem_id END) AS solved, "
        "COUNT(s.id) AS total_subs "
        "FROM users u LEFT JOIN submissions s ON s.user_id = u.id "
        "GROUP BY u.id, u.username "
        "ORDER BY solved DESC, total_subs ASC, u.id ASC LIMIT 50";

    if (mysql_query(conn, sql.c_str()) != 0) {
        pool.releaseConnection(conn);
        root["error"] = "query failed";
        Json::StreamWriterBuilder builder;
        return {500, Json::writeString(builder, root)};
    }

    MYSQL_RES* res = mysql_store_result(conn);
    Json::Value items(Json::arrayValue);
    if (res) {
        MYSQL_ROW row;
        int rank = 0;
        while ((row = mysql_fetch_row(res))) {
            Json::Value item;
            item["rank"] = ++rank;
            item["user_id"] = std::stoi(row[0]);
            item["username"] = row[1] ? row[1] : "";
            item["solved"] = std::stoi(row[2] ? row[2] : "0");
            item["submissions"] = std::stoi(row[3] ? row[3] : "0");
            items.append(item);
        }
        mysql_free_result(res);
    }
    pool.releaseConnection(conn);

    root["leaderboard"] = items;
    Json::StreamWriterBuilder builder;
    return {200, Json::writeString(builder, root)};
}

} // namespace oj
