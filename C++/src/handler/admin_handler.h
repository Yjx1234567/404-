#pragma once

#include <string>
#include <utility>

namespace oj {

// Admin handlers: return {status_code, json_body}
// 创建题目（POST /api/admin/problems）
std::pair<int, std::string> handleCreateProblem(const std::string& body);

// 更新题目（PUT /api/admin/problems/:id）：
// 提供的字段会被覆盖；test_cases 提供时整体替换；tags 提供时整体替换
std::pair<int, std::string> handleUpdateProblem(int id, const std::string& body);

// 管理员题目详情（GET /api/admin/problems/:id）：含 spj_code、tags、测试用例
std::pair<int, std::string> handleGetProblemAdmin(int id);

// 删除题目（DELETE /api/admin/problems/:id）
std::pair<int, std::string> handleDeleteProblem(int id);

} // namespace oj
