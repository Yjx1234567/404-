#pragma once

#include <condition_variable>
#include <mutex>
#include <queue>
#include <string>
#include <thread>
#include <vector>
#include <atomic>

namespace oj {

// 异步判题队列：提交接口只负责落库 + 入队（立即返回），
// 后台工作线程取出任务执行编译/运行，并把结果回写到 submissions 表。
// 服务重启时 start() 会自动把遗留的 PENDING/JUDGING 提交重新入队。
class JudgeQueue {
public:
    static JudgeQueue& getInstance();

    // 启动 worker 线程池并恢复未完成的提交
    void start(int worker_count);
    // 停止队列（等待已在判的任务完成，丢弃未开始的任务）
    void stop();

    void enqueue(int submission_id, const std::string& code, int problem_id);

    size_t pendingCount();

private:
    JudgeQueue() = default;
    ~JudgeQueue() = default;
    JudgeQueue(const JudgeQueue&) = delete;
    JudgeQueue& operator=(const JudgeQueue&) = delete;

    struct Task {
        int submission_id;
        std::string code;
        int problem_id;
    };

    void workerLoop();
    void processTask(const Task& task);

    std::queue<Task> queue_;
    std::mutex mutex_;
    std::condition_variable cv_;
    std::vector<std::thread> workers_;
    std::atomic<bool> running_{false};
};

} // namespace oj
