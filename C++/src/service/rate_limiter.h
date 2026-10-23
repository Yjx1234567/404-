#pragma once

#include <deque>
#include <mutex>
#include <unordered_map>

namespace oj {

// 提交频率限制：每用户滑动窗口限流（默认 60 秒内最多 N 次）。
// limit <= 0 表示不限制。内存态即可，重启清零无妨。
class RateLimiter {
public:
    static RateLimiter& getInstance();

    // 允许提交返回 true；超限返回 false
    bool allow(int user_id);

    // 供测试注入
    void setLimit(int limit, int window_seconds);

private:
    RateLimiter() = default;
    RateLimiter(const RateLimiter&) = delete;
    RateLimiter& operator=(const RateLimiter&) = delete;

    int limit_ = -1;          // -1 = 未初始化，首次 allow 时读配置
    int window_seconds_ = 60;
    std::mutex mutex_;
    std::unordered_map<int, std::deque<long long>> hits_;  // user_id -> 时间戳(ms)
};

} // namespace oj
