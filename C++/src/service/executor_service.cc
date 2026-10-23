#include "service/executor_service.h"
#include "model/problem.h"
#include "model/test_case.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <cstdlib>
#include <cstdio>
#include <unistd.h>
#include <fcntl.h>
#include <sys/wait.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/syscall.h>
#include <sched.h>
#include <pwd.h>
#include <grp.h>
#include <signal.h>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <chrono>
#include <cstring>
#include <algorithm>
#include <atomic>

namespace oj {

// FNV-1a：为 SPJ 源码生成缓存 key
static std::string fnv1aHex(const std::string& s) {
    unsigned long long h = 1469598103934665603ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    char buf[32];
    snprintf(buf, sizeof(buf), "%016llx", h);
    return buf;
}

JudgeResult ExecutorService::execute(const std::string& code, int problem_id,
                                     const std::string& spj_code) {
    JudgeResult result;
    result.passed = 0;
    result.total = 0;

    std::string temp_dir = ConfigManager::getInstance().getString("execution.temp_dir", "/tmp/oj_temp");
    std::filesystem::create_directories(temp_dir);

    // 唯一临时文件名：PID + 原子自增序号 + 时间戳，并发提交不会撞名
    static std::atomic<unsigned long> submit_seq{0};
    std::string source_path = temp_dir + "/submit_" + std::to_string(problem_id) + "_" +
                              std::to_string(static_cast<long>(getpid())) + "_" +
                              std::to_string(++submit_seq) + "_" +
                              std::to_string(std::chrono::system_clock::now().time_since_epoch().count()) + ".cpp";
    std::string exec_path = source_path.substr(0, source_path.size() - 4);

    // Write source code to file
    std::ofstream source_file(source_path);
    if (!source_file) {
        result.status = statusToString(JudgeStatus::SE);
        result.message = "System Error: Failed to write source file";
        return result;
    }
    source_file << code;
    source_file.close();

    // Compile（带超时，防编译炸弹）
    int compile_timeout = ConfigManager::getInstance().getInt("execution.compile_timeout_seconds", 15);
    std::string compile_error;
    if (!compile(source_path, exec_path, compile_timeout, compile_error)) {
        result.status = statusToString(JudgeStatus::CE);
        result.message = "Compilation Error";
        result.error_info = compile_error;

        std::filesystem::remove(source_path);
        return result;
    }

    // Get problem test cases
    Problem problem;
    if (!problem.loadFromDB(problem_id)) {
        result.status = statusToString(JudgeStatus::SE);
        result.message = "System Error: Problem not found";
        std::filesystem::remove(source_path);
        std::filesystem::remove(exec_path);
        return result;
    }

    auto test_cases = problem.getTestCases();
    result.total = static_cast<int>(test_cases.size());

    if (test_cases.empty()) {
        result.status = statusToString(JudgeStatus::AC);
        result.message = "All test cases passed (no test cases)";
        result.passed = 0;
        std::filesystem::remove(source_path);
        std::filesystem::remove(exec_path);
        return result;
    }

    // Special Judge 准备（编译失败直接 SE，不逐用例报错）
    std::string spj_path;
    bool use_spj = false;
    if (!spj_code.empty()) {
        std::string spj_err;
        if (!prepareSpj(spj_code, problem_id, spj_path, spj_err)) {
            result.status = statusToString(JudgeStatus::SE);
            result.message = "System Error: Special judge compile failed";
            result.error_info = spj_err;
            std::filesystem::remove(source_path);
            std::filesystem::remove(exec_path);
            return result;
        }
        use_spj = true;
    }

    int timeout_sec = ConfigManager::getInstance().getInt("execution.timeout_seconds", 5);
    int max_memory_mb = ConfigManager::getInstance().getInt("execution.max_memory_mb", 256);

    bool all_passed = true;
    for (size_t i = 0; i < test_cases.size(); ++i) {
        const auto& tc = test_cases[i];

        // 首个用例失败后不再继续（经典 OJ 行为）：
        // 避免每题 5s 超时 × N 用例拖垮整体判题时间
        if (!all_passed) break;

        int exit_code = 0;
        std::string output;
        int elapsed_ms = 0;
        long max_rss_kb = 0;
        std::string error_info;

        bool run_ok = runTestCase(exec_path, tc.getInput(), tc.getExpected(),
                                  timeout_sec, max_memory_mb,
                                  exit_code, output, elapsed_ms, max_rss_kb, error_info);

        TestCaseResult tc_result;
        tc_result.case_num = static_cast<int>(i + 1);
        tc_result.exit_code = exit_code;
        tc_result.elapsed_ms = elapsed_ms;
        tc_result.stdout = output;
        tc_result.expected = tc.getExpected();
        tc_result.actual = output;
        tc_result.error_info = error_info;

        if (!run_ok) {
            long mem_limit_kb = static_cast<long>(max_memory_mb) * 1024;
            // OOM 判定：坏分配失败（SIGABRT/SIGSEGV）时虚拟地址空间已撞上限，
            // 但 vector 扩容等场景下 RSS 通常只有上限的 50%~70%
            // （旧缓冲区 + 新缓冲区同时映射），故阈值取 50% 而非 90%
            bool mem_exceeded = max_rss_kb > 0 && max_rss_kb >= mem_limit_kb / 2;
            if (error_info.find("timeout") != std::string::npos) {
                tc_result.status = statusToString(JudgeStatus::TLE);
                result.status = statusToString(JudgeStatus::TLE);
                result.message = "Time Limit Exceeded";
            } else if (mem_exceeded &&
                       (error_info.find("Segmentation fault") != std::string::npos ||
                        error_info.find("out of memory") != std::string::npos)) {
                tc_result.status = statusToString(JudgeStatus::MLE);
                if (result.status.empty() || result.status == statusToString(JudgeStatus::AC)) {
                    result.status = statusToString(JudgeStatus::MLE);
                    result.message = "Memory Limit Exceeded";
                }
                tc_result.error_info = "Memory limit exceeded (peak RSS " +
                                       std::to_string(max_rss_kb / 1024) + "MB / limit " +
                                       std::to_string(max_memory_mb) + "MB)";
            } else if (exit_code != 0) {
                tc_result.status = statusToString(JudgeStatus::RE);
                if (result.status.empty() || result.status == statusToString(JudgeStatus::AC)) {
                    result.status = statusToString(JudgeStatus::RE);
                    result.message = "Runtime Error";
                }
            } else {
                tc_result.status = statusToString(JudgeStatus::SE);
                result.status = statusToString(JudgeStatus::SE);
                result.message = "System Error";
            }
            all_passed = false;
        } else {
            bool case_passed = false;
            if (use_spj) {
                // SPJ：把输入/期望/实际写到文件，交给特判程序判定
                std::string in_file = exec_path + ".spj_in";
                std::string exp_file = exec_path + ".spj_exp";
                std::string act_file = exec_path + ".spj_act";
                { std::ofstream f(in_file); f << tc.getInput(); }
                { std::ofstream f(exp_file); f << tc.getExpected(); }
                { std::ofstream f(act_file); f << output; }

                std::string spj_msg;
                bool spj_ok = runSpj(spj_path, in_file, exp_file, act_file, spj_msg);
                if (!spj_msg.empty()) tc_result.error_info = spj_msg;
                std::filesystem::remove(in_file);
                std::filesystem::remove(exp_file);
                std::filesystem::remove(act_file);

                case_passed = spj_ok;
            } else {
                // 普通比对（去尾部空白）
                std::string trimmed_output = output;
                std::string trimmed_expected = tc.getExpected();

                auto trim_end = [](std::string& s) {
                    while (!s.empty() && (s.back() == ' ' || s.back() == '\t' || s.back() == '\n' || s.back() == '\r')) {
                        s.pop_back();
                    }
                };
                trim_end(trimmed_output);
                trim_end(trimmed_expected);

                case_passed = (trimmed_output == trimmed_expected);
            }

            if (case_passed) {
                tc_result.status = statusToString(JudgeStatus::AC);
                result.passed++;
            } else {
                tc_result.status = statusToString(JudgeStatus::WA);
                all_passed = false;
                if (result.status.empty() || result.status == statusToString(JudgeStatus::AC)) {
                    result.status = statusToString(JudgeStatus::WA);
                    result.message = "Wrong Answer";
                }
            }
        }

        result.details.push_back(tc_result);
    }

    if (all_passed) {
        result.status = statusToString(JudgeStatus::AC);
        result.message = "All test cases passed";
    }

    // Cleanup
    std::filesystem::remove(source_path);
    std::filesystem::remove(exec_path);

    return result;
}

bool ExecutorService::compile(const std::string& source_path, const std::string& exec_path,
                              int timeout_sec, std::string& compile_error) {
    std::string compile_cmd = ConfigManager::getInstance().getString("execution.compile_command", "g++ -std=c++17 -O2");
    // 编译错误输出重定向到临时文件，避免管道缓冲区限制
    std::string err_file = exec_path + ".compile_error";
    compile_cmd += " -o " + exec_path + " " + source_path + " 2>" + err_file;

    // fork + sh -c 执行编译，父进程带超时等待：模板元编程炸弹等编译期
    // 死循环会被 SIGKILL，不会挂死判题线程（原 std::system 无超时）
    pid_t pid = fork();
    if (pid == -1) {
        compile_error = "System Error: failed to fork for compile";
        return false;
    }

    if (pid == 0) {
        execl("/bin/sh", "sh", "-c", compile_cmd.c_str(), (char*)nullptr);
        _exit(127);
    }

    int status = 0;
    bool timed_out = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec);
    while (true) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r == -1) {
            compile_error = "System Error: waitpid failed during compile";
            std::filesystem::remove(err_file);
            return false;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            timed_out = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(50 * 1000);
    }

    bool ok = false;
    if (timed_out) {
        compile_error = "Compilation timed out (" + std::to_string(timeout_sec) + "s limit)";
    } else if (WIFEXITED(status)) {
        ok = (WEXITSTATUS(status) == 0);
        if (!ok && WEXITSTATUS(status) == 127) {
            compile_error = "System Error: failed to launch compiler";
        }
    }

    // 兜底：即使返回 0，可执行文件不存在也视为失败
    if (ok && !std::filesystem::exists(exec_path)) {
        ok = false;
    }

    if (!ok && compile_error.empty()) {
        std::ifstream err_fs(err_file);
        if (err_fs) {
            std::ostringstream oss;
            oss << err_fs.rdbuf();
            compile_error = oss.str();
        }
        if (compile_error.empty()) {
            compile_error = "Compilation failed (no error output)";
        }
        // 编译错误信息可能很长，截断到 8KB，避免响应过大
        const size_t max_len = 8192;
        if (compile_error.size() > max_len) {
            compile_error = compile_error.substr(0, max_len) + "\n... (truncated)";
        }
    }

    std::filesystem::remove(err_file);
    return ok;
}

// 沙箱加固（子进程内调用，best-effort：非 root 环境自动跳过降权/禁网）
void ExecutorService::applySandbox(const std::string& work_dir) {
    // 1) 禁网：独立网络命名空间，用户代码无法访问内网/数据库
    //    需要 CAP_SYS_ADMIN（root），失败则忽略
    unshare(CLONE_NEWNET);

    // 2) 降权：root 运行时切换到低权限用户，防止用户代码读写服务文件
    std::string judge_user = ConfigManager::getInstance().getString("execution.judge_user", "nobody");
    if (geteuid() == 0 && !judge_user.empty()) {
        struct passwd* pw = getpwnam(judge_user.c_str());
        if (pw) {
            setgroups(0, nullptr);
            setgid(pw->pw_gid);
            setuid(pw->pw_uid);
        }
    }

    // 3) 工作目录切到判题临时目录
    if (!work_dir.empty()) {
        chdir(work_dir.c_str());
    }

    // 4) 打开的文件描述符限制
    struct rlimit rl;
    rl.rlim_cur = 64;
    rl.rlim_max = 64;
    setrlimit(RLIMIT_NOFILE, &rl);
}

bool ExecutorService::runTestCase(const std::string& exec_path, const std::string& input,
                                   const std::string& expected, int timeout_sec, int max_memory_mb,
                                   int& exit_code, std::string& output, int& elapsed_ms,
                                   long& max_rss_kb, std::string& error_info) {
    // 使用临时文件重定向 stdin/stdout/stderr（参考经典 OJ 实现方案）。
    // 不用管道的原因：管道缓冲区只有 64KB，大输入 write 会阻塞父进程，
    // 而子进程可能因 stdout 写满而阻塞，形成死锁。临时文件彻底规避该问题。
    static std::atomic<unsigned long> seq{0};
    unsigned long uid = ++seq;
    std::string base = exec_path + ".tc" + std::to_string(uid);
    std::string stdin_file = base + ".in";
    std::string stdout_file = base + ".out";
    std::string stderr_file = base + ".err";

    // 写入输入文件
    {
        std::ofstream ofs(stdin_file);
        if (!ofs) {
            error_info = "Failed to create stdin temp file";
            return false;
        }
        ofs << input;
    }

    auto cleanup_files = [&]() {
        std::error_code ec;
        std::filesystem::remove(stdin_file, ec);
        std::filesystem::remove(stdout_file, ec);
        std::filesystem::remove(stderr_file, ec);
    };

    pid_t pid = fork();
    if (pid == -1) {
        error_info = "Failed to fork";
        cleanup_files();
        return false;
    }

    auto start = std::chrono::steady_clock::now();

    if (pid == 0) {
        // ===== 子进程 =====
        // 打开三个临时文件并重定向标准流
        int fd_in = open(stdin_file.c_str(), O_RDONLY);
        int fd_out = open(stdout_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int fd_err = open(stderr_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        if (fd_in < 0 || fd_out < 0 || fd_err < 0) {
            _exit(125);
        }
        dup2(fd_in, STDIN_FILENO);
        dup2(fd_out, STDOUT_FILENO);
        dup2(fd_err, STDERR_FILENO);
        close(fd_in);
        close(fd_out);
        close(fd_err);

        // 沙箱加固（禁网/降权，best-effort）
        std::string temp_dir = ConfigManager::getInstance().getString("execution.temp_dir", "/tmp/oj_temp");
        applySandbox(temp_dir);

        // 资源限制：内核级强制执行，比父进程 kill 更可靠
        struct rlimit rl;

        // CPU 时间：超出后内核发 SIGXCPU
        rl.rlim_cur = timeout_sec;
        rl.rlim_max = timeout_sec + 1;
        setrlimit(RLIMIT_CPU, &rl);

        // 地址空间（内存）：超出后 malloc 返回空 / SIGSEGV
        size_t mem_bytes = static_cast<size_t>(max_memory_mb) * 1024 * 1024;
        rl.rlim_cur = mem_bytes;
        rl.rlim_max = mem_bytes;
        setrlimit(RLIMIT_AS, &rl);

        // 栈：8MB
        rl.rlim_cur = 8 * 1024 * 1024;
        rl.rlim_max = 8 * 1024 * 1024;
        setrlimit(RLIMIT_STACK, &rl);

        // 输出文件大小：1MB，防止刷屏型恶意代码
        rl.rlim_cur = 1024 * 1024;
        rl.rlim_max = 1024 * 1024;
        setrlimit(RLIMIT_FSIZE, &rl);

        // 进程数：放宽到 64，允许用户代码使用 std::thread 等正常多线程
        rl.rlim_cur = 64;
        rl.rlim_max = 64;
        setrlimit(RLIMIT_NPROC, &rl);

        // 核心转储关闭
        rl.rlim_cur = 0;
        rl.rlim_max = 0;
        setrlimit(RLIMIT_CORE, &rl);

        // 最小化环境变量，避免环境信息泄露；PATH 供偶发的 system() 调用使用
        char* const envp[] = { (char*)"PATH=/usr/bin:/bin", (char*)"HOME=/tmp", nullptr };
        execle(exec_path.c_str(), exec_path.c_str(), nullptr, envp);
        _exit(127); // exec 失败
    }

    // ===== 父进程：带超时的等待（轮询 + SIGKILL 兜底），wait4 取峰值内存 =====
    int status = 0;
    bool timed_out = false;
    struct rusage ru;
    memset(&ru, 0, sizeof(ru));
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(timeout_sec + 1);

    while (true) {
        pid_t r = wait4(pid, &status, WNOHANG, &ru);
        if (r == pid) {
            break; // 子进程已结束
        }
        if (r == -1) {
            error_info = "waitpid failed";
            cleanup_files();
            return false;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            timed_out = true;
            kill(pid, SIGKILL);
            wait4(pid, &status, 0, &ru);
            break;
        }
        usleep(50 * 1000); // 50ms 轮询间隔
    }

    auto end = std::chrono::steady_clock::now();
    elapsed_ms = static_cast<int>(std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count());
    // ru_maxrss 单位为 KB（Linux）
    max_rss_kb = ru.ru_maxrss;

    // 读取输出（限制读取量，防止超大输出占用内存）
    std::string stderr_out;
    {
        std::ifstream ofs(stdout_file);
        if (ofs) {
            std::ostringstream oss;
            oss << ofs.rdbuf();
            output = oss.str();
            const size_t max_out = 1024 * 1024;
            if (output.size() > max_out) {
                output = output.substr(0, max_out);
            }
        }
        // stderr 在文件删除前读出，帮助用户定位运行时错误
        std::ifstream efs(stderr_file);
        if (efs) {
            std::ostringstream ess;
            ess << efs.rdbuf();
            stderr_out = ess.str();
            const size_t max_err = 4096;
            if (stderr_out.size() > max_err) {
                stderr_out = stderr_out.substr(0, max_err) + "\n... (truncated)";
            }
        }
    }

    cleanup_files();

    if (timed_out) {
        error_info = "timeout";
        exit_code = -1;
        return false;
    }

    if (WIFEXITED(status)) {
        exit_code = WEXITSTATUS(status);
        if (exit_code != 0) {
            error_info = "Process exited with code " + std::to_string(exit_code);
            if (!stderr_out.empty()) {
                error_info += "\nstderr: " + stderr_out;
            }
            return false;
        }
    } else if (WIFSIGNALED(status)) {
        int sig = WTERMSIG(status);
        exit_code = -sig;
        if (sig == SIGKILL || sig == SIGXCPU) {
            error_info = "timeout";
        } else if (sig == SIGSEGV) {
            error_info = "Segmentation fault";
        } else if (sig == SIGABRT) {
            error_info = "Aborted (possibly out of memory)";
        } else if (sig == SIGFPE) {
            error_info = "Floating point exception";
        } else {
            error_info = "Killed by signal " + std::to_string(sig);
        }
        if (!stderr_out.empty()) {
            error_info += "\nstderr: " + stderr_out;
        }
        return false;
    } else {
        error_info = "Unknown process termination";
        exit_code = -1;
        return false;
    }

    return true;
}

bool ExecutorService::prepareSpj(const std::string& spj_code, int problem_id,
                                 std::string& spj_path, std::string& err) {
    std::string temp_dir = ConfigManager::getInstance().getString("execution.temp_dir", "/tmp/oj_temp");
    std::string cache_dir = temp_dir + "/spj_cache";
    std::filesystem::create_directories(cache_dir);

    // 按源码哈希缓存：源码没变就复用上次编译结果
    spj_path = cache_dir + "/spj_" + std::to_string(problem_id) + "_" + fnv1aHex(spj_code);

    if (std::filesystem::exists(spj_path)) {
        return true;
    }

    std::string src_path = spj_path + ".cpp";
    {
        std::ofstream ofs(src_path);
        if (!ofs) {
            err = "Failed to write spj source file";
            return false;
        }
        ofs << spj_code;
    }

    int compile_timeout = ConfigManager::getInstance().getInt("execution.compile_timeout_seconds", 15);
    if (!compile(src_path, spj_path, compile_timeout, err)) {
        std::filesystem::remove(src_path);
        return false;
    }

    std::filesystem::remove(src_path);
    return true;
}

bool ExecutorService::runSpj(const std::string& spj_path, const std::string& input_file,
                             const std::string& expected_file, const std::string& actual_file,
                             std::string& spj_message) {
    // SPJ 调用约定：argv[1]=输入文件 argv[2]=期望输出 argv[3]=用户输出
    // 退出码 0 → AC，非 0 → WA；stdout 首行作为说明信息
    std::string out_file = actual_file + ".spj_out";
    std::string err_file = actual_file + ".spj_err";

    pid_t pid = fork();
    if (pid == -1) {
        spj_message = "SPJ fork failed";
        return false;
    }

    if (pid == 0) {
        int fd_out = open(out_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int fd_err = open(err_file.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0600);
        int fd_in = open("/dev/null", O_RDONLY);
        if (fd_in < 0 || fd_out < 0 || fd_err < 0) _exit(125);
        dup2(fd_in, STDIN_FILENO);
        dup2(fd_out, STDOUT_FILENO);
        dup2(fd_err, STDERR_FILENO);
        close(fd_in); close(fd_out); close(fd_err);

        // 10 秒超时上限：特判程序挂死不能拖垮判题
        struct rlimit rl;
        rl.rlim_cur = 10;
        rl.rlim_max = 11;
        setrlimit(RLIMIT_CPU, &rl);

        execl(spj_path.c_str(), spj_path.c_str(),
              input_file.c_str(), expected_file.c_str(), actual_file.c_str(), (char*)nullptr);
        _exit(127);
    }

    int status = 0;
    bool timed_out = false;
    const auto deadline = std::chrono::steady_clock::now() + std::chrono::seconds(11);
    while (true) {
        pid_t r = waitpid(pid, &status, WNOHANG);
        if (r == pid) break;
        if (r == -1) {
            spj_message = "SPJ waitpid failed";
            std::filesystem::remove(out_file);
            std::filesystem::remove(err_file);
            return false;
        }
        if (std::chrono::steady_clock::now() > deadline) {
            timed_out = true;
            kill(pid, SIGKILL);
            waitpid(pid, &status, 0);
            break;
        }
        usleep(20 * 1000);
    }

    // 读 stdout 首行作为信息
    {
        std::ifstream ofs(out_file);
        if (ofs) {
            std::string line;
            if (std::getline(ofs, line)) {
                spj_message = line;
            }
        }
    }
    std::error_code ec;
    std::filesystem::remove(out_file, ec);
    std::filesystem::remove(err_file, ec);

    if (timed_out) {
        spj_message = "SPJ timeout";
        return false;
    }

    bool ok = WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return ok;
}

std::string ExecutorService::statusToString(JudgeStatus status) {
    switch (status) {
        case JudgeStatus::AC:  return "AC";
        case JudgeStatus::WA:  return "WA";
        case JudgeStatus::TLE: return "TLE";
        case JudgeStatus::MLE: return "MLE";
        case JudgeStatus::RE:  return "RE";
        case JudgeStatus::CE:  return "CE";
        case JudgeStatus::SE:  return "SE";
        default:               return "UNKNOWN";
    }
}

} // namespace oj
