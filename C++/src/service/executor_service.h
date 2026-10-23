#pragma once

#include <string>
#include <vector>
#include <tuple>

namespace oj {

enum class JudgeStatus {
    AC,     // Accepted
    WA,     // Wrong Answer
    TLE,    // Time Limit Exceeded
    MLE,    // Memory Limit Exceeded
    RE,     // Runtime Error
    CE,     // Compilation Error
    SE      // System Error
};

struct TestCaseResult {
    int case_num;
    int exit_code;
    int elapsed_ms;
    std::string stdout;
    std::string status;
    std::string expected;
    std::string actual;
    std::string error_info;
};

struct JudgeResult {
    std::string status;
    std::string message;
    int passed;
    int total;
    std::vector<TestCaseResult> details;
    std::string error_info;
};

class ExecutorService {
public:
    // 判题入口：编译用户代码，逐用例运行并比对。
    // spj_code 非空时启用 Special Judge（见 prepareSpj 注释的调用约定）
    static JudgeResult execute(const std::string& code, int problem_id,
                               const std::string& spj_code = "");
    static std::string statusToString(JudgeStatus status);

private:
    // 带超时的编译（防止模板元编程炸弹挂死判题线程）
    static bool compile(const std::string& source_path, const std::string& exec_path,
                        int timeout_sec, std::string& compile_error);

    // 运行单个测试用例；max_rss_kb 返回进程实际峰值内存（KB，来自 wait4 rusage）
    static bool runTestCase(const std::string& exec_path, const std::string& input,
                            const std::string& expected, int timeout_sec, int max_memory_mb,
                            int& exit_code, std::string& output, int& elapsed_ms,
                            long& max_rss_kb, std::string& error_info);

    // 运行 Special Judge 程序（管理员代码，只做超时保护）
    static bool runSpj(const std::string& spj_path, const std::string& input_file,
                       const std::string& expected_file, const std::string& actual_file,
                       std::string& spj_message);

    // 编译（并按源码哈希缓存）SPJ 可执行文件，返回 false 时 err 给出原因
    static bool prepareSpj(const std::string& spj_code, int problem_id,
                           std::string& spj_path, std::string& err);

    // 沙箱加固：禁网 + 降权（best-effort，需 root 才完全生效）+ 基础 rlimit
    static void applySandbox(const std::string& work_dir);
};

} // namespace oj
