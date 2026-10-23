#include "service/judge_queue.h"
#include "service/executor_service.h"
#include "model/submission.h"
#include "utils/config.h"
#include "utils/logger.h"
#include <json/json.h>

namespace oj {

JudgeQueue& JudgeQueue::getInstance() {
    static JudgeQueue instance;
    return instance;
}

void JudgeQueue::start(int worker_count) {
    if (worker_count < 1) worker_count = 1;
    if (worker_count > 8) worker_count = 8;

    bool expected = false;
    if (!running_.compare_exchange_strong(expected, true)) {
        return; // 已启动
    }

    // 恢复上次服务退出时未判完的提交
    auto unfinished = Submission::getUnfinished();
    for (const auto& s : unfinished) {
        Submission full;
        if (full.loadFromDB(s.getId())) {
            enqueue(full.getId(), full.getCode(), full.getProblemId());
            Logger::getInstance().info("Requeued unfinished submission #" + std::to_string(full.getId()));
        }
    }

    for (int i = 0; i < worker_count; ++i) {
        workers_.emplace_back([this]() { workerLoop(); });
    }
    Logger::getInstance().info("Judge queue started with " + std::to_string(worker_count) + " workers");
}

void JudgeQueue::stop() {
    bool expected = true;
    if (!running_.compare_exchange_strong(expected, false)) {
        return;
    }
    cv_.notify_all();
    for (auto& w : workers_) {
        if (w.joinable()) w.join();
    }
    workers_.clear();
}

void JudgeQueue::enqueue(int submission_id, const std::string& code, int problem_id) {
    {
        std::lock_guard<std::mutex> lock(mutex_);
        queue_.push({submission_id, code, problem_id});
    }
    cv_.notify_one();
}

size_t JudgeQueue::pendingCount() {
    std::lock_guard<std::mutex> lock(mutex_);
    return queue_.size();
}

void JudgeQueue::workerLoop() {
    while (true) {
        Task task;
        {
            std::unique_lock<std::mutex> lock(mutex_);
            cv_.wait(lock, [this]() { return !queue_.empty() || !running_.load(); });

            if (queue_.empty()) {
                if (!running_.load()) return;
                continue;
            }

            task = queue_.front();
            queue_.pop();
        }
        processTask(task);
    }
}

void JudgeQueue::processTask(const Task& task) {
    Submission submission;
    submission.setId(task.submission_id);
    // 标记判题开始
    submission.updateStatus("JUDGING");

    JudgeResult result = ExecutorService::execute(task.code, task.problem_id);

    // 取各用例最大耗时作为总耗时
    int max_ms = 0;
    for (const auto& d : result.details) {
        if (d.elapsed_ms > max_ms) max_ms = d.elapsed_ms;
    }

    // details 序列化为 JSON 存库
    Json::Value details(Json::arrayValue);
    for (const auto& d : result.details) {
        Json::Value item;
        item["case"] = d.case_num;
        item["exit_code"] = d.exit_code;
        item["elapsed_ms"] = d.elapsed_ms;
        item["stdout"] = d.stdout;
        item["status"] = d.status;
        if (d.status == "WA") {
            item["expected"] = d.expected;
            item["actual"] = d.actual;
        }
        if (!d.error_info.empty()) {
            item["error"] = d.error_info;
        }
        details.append(item);
    }
    Json::StreamWriterBuilder builder;
    std::string details_json = Json::writeString(builder, details);

    submission.updateResult(result.status, result.passed, result.total,
                            max_ms, result.error_info.empty() ? result.message : result.error_info,
                            details_json);

    Logger::getInstance().info("Submission #" + std::to_string(task.submission_id) +
                               " judged: " + result.status +
                               " (" + std::to_string(result.passed) + "/" + std::to_string(result.total) + ")");
}

} // namespace oj
