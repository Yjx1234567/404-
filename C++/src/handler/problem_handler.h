#pragma once

#include <string>
#include <map>
#include <utility>

namespace oj {

// Problem handlers: return {status_code, json_body}
// 题目列表（支持 page/page_size/difficulty/keyword/tag_id 查询参数）
std::pair<int, std::string> handleProblemList(const std::map<std::string, std::string>& query);
std::pair<int, std::string> handleProblemDetail(int id);
// GET /api/tags：所有标签及题目数
std::pair<int, std::string> handleTagsList();

} // namespace oj
