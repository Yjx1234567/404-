#pragma once

#include <string>
#include <utility>

namespace oj {

// 角色约定：班级创建者是老师（teacher），加入者是学生（student）。
// 系统管理员（role=admin）拥有全部班级的管理权限。

// POST /api/classes {name} → 创建班级（任何登录用户），返回 {id, join_code}
std::pair<int, std::string> handleCreateClass(const std::string& body, int user_id);

// POST /api/classes/join {join_code} → 通过邀请码加入班级
std::pair<int, std::string> handleJoinClass(const std::string& body, int user_id);

// GET /api/classes/mine → 我创建/加入的班级列表
std::pair<int, std::string> handleMyClasses(int user_id);

// GET /api/classes/:id → 班级详情（老师/管理员含成员列表）
std::pair<int, std::string> handleClassDetail(int class_id, int user_id, bool is_admin);

// DELETE /api/classes/:id → 删除班级（老师本人或管理员）
std::pair<int, std::string> handleDeleteClass(int class_id, int user_id, bool is_admin);

// POST /api/classes/:id/assignments {title, description, problem_ids[], due_at} → 布置作业
std::pair<int, std::string> handleCreateAssignment(int class_id, const std::string& body,
                                                   int user_id, bool is_admin);

// GET /api/classes/:id/assignments → 作业列表（含我每题完成状态）
std::pair<int, std::string> handleListAssignments(int class_id, int user_id);

// GET /api/assignments/:id → 作业详情；老师/管理员额外返回全班完成矩阵
std::pair<int, std::string> handleAssignmentDetail(int assignment_id, int user_id, bool is_admin);

} // namespace oj
