#pragma once

#include <string>
#include <map>
#include <utility>

namespace oj {

// POST /api/submit：落库 + 入队，立即返回（异步判题）
std::pair<int, std::string> handleSubmit(const std::string& body, int user_id);

// GET /api/submissions/:id：提交详情（仅本人或管理员可见代码与结果）
std::pair<int, std::string> handleSubmissionDetail(int id, int user_id, bool is_admin);

// GET /api/submissions?problem_id=&page=&page_size=：当前用户提交历史
std::pair<int, std::string> handleSubmissionList(const std::map<std::string, std::string>& query,
                                                 int user_id, bool is_admin);

// GET /api/users/me/stats：个人做题统计
std::pair<int, std::string> handleUserStats(int user_id);

// GET /api/leaderboard：做题数排行
std::pair<int, std::string> handleLeaderboard();

} // namespace oj
