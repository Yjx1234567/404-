#pragma once

#include <string>
#include <vector>

namespace oj {

// 提交记录：每次代码提交在 DB 中落一行。
// status 生命周期：PENDING（已入队未判）→ JUDGING（判题中）→ 终态（AC/WA/TLE/MLE/RE/CE/SE）
class Submission {
public:
    Submission();

    int getId() const { return id_; }
    void setId(int id) { id_ = id; }

    int getUserId() const { return user_id_; }
    void setUserId(int uid) { user_id_ = uid; }

    int getProblemId() const { return problem_id_; }
    void setProblemId(int pid) { problem_id_ = pid; }

    const std::string& getCode() const { return code_; }
    void setCode(const std::string& code) { code_ = code; }

    const std::string& getStatus() const { return status_; }
    void setStatus(const std::string& status) { status_ = status; }

    int getPassed() const { return passed_; }
    void setPassed(int p) { passed_ = p; }

    int getTotal() const { return total_; }
    void setTotal(int t) { total_ = t; }

    int getTimeMs() const { return time_ms_; }
    void setTimeMs(int t) { time_ms_ = t; }

    const std::string& getErrorInfo() const { return error_info_; }
    void setErrorInfo(const std::string& e) { error_info_ = e; }

    const std::string& getResult() const { return result_; }
    void setResult(const std::string& r) { result_ = r; }

    const std::string& getCreatedAt() const { return created_at_; }
    void setCreatedAt(const std::string& t) { created_at_ = t; }

    bool saveToDB();
    bool loadFromDB(int id);
    // 判题完成后回写结果
    bool updateResult(const std::string& status, int passed, int total,
                      int time_ms, const std::string& error_info,
                      const std::string& result_json);
    // 仅更新状态（判题开始时置 JUDGING）
    bool updateStatus(const std::string& status);

    // 分页查询某用户的提交记录；problem_id > 0 时按题目过滤
    static std::vector<Submission> getByUser(int user_id, int page, int page_size,
                                             int problem_id = 0);
    static int countByUser(int user_id, int problem_id = 0);

    // ===== 统计查询 =====
    // AC 的不同题目数
    static int countSolvedProblems(int user_id);
    // 尝试过（有提交）的不同题目数
    static int countAttemptedProblems(int user_id);
    // 总提交次数
    static int countSubmissions(int user_id);
    // 按难度统计 AC 的不同题目数，difficulty 为空则汇总
    static int countSolvedByDifficulty(int user_id, const std::string& difficulty);
    // 某用户是否 AC 过某题
    static bool hasAC(int user_id, int problem_id);

    // 重启恢复：取所有未判完（PENDING/JUDGING）的提交重新入队
    static std::vector<Submission> getUnfinished();

private:
    int id_;
    int user_id_;
    int problem_id_;
    std::string code_;
    std::string status_;
    int passed_;
    int total_;
    int time_ms_;
    std::string error_info_;
    std::string result_;
    std::string created_at_;
};

} // namespace oj
