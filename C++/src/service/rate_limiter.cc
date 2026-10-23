#include "service/rate_limiter.h"
#include "utils/config.h"
#include <chrono>

namespace oj {

RateLimiter& RateLimiter::getInstance() {
    static RateLimiter instance;
    return instance;
}

void RateLimiter::setLimit(int limit, int window_seconds) {
    std::lock_guard<std::mutex> lock(mutex_);
    limit_ = limit;
    window_seconds_ = window_seconds > 0 ? window_seconds : 60;
}

bool RateLimiter::allow(int user_id) {
    std::lock_guard<std::mutex> lock(mutex_);

    if (limit_ < 0) {
        // 首次调用时从配置读取（0 或负值表示不限制）
        int cfg = ConfigManager::getInstance().getInt("execution.rate_limit_per_minute", 0);
        limit_ = cfg;
        window_seconds_ = 60;
    }

    if (limit_ <= 0) return true;

    long long now_ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                           std::chrono::system_clock::now().time_since_epoch()).count();
    long long window_start = now_ms - static_cast<long long>(window_seconds_) * 1000;

    auto& dq = hits_[user_id];
    while (!dq.empty() && dq.front() < window_start) {
        dq.pop_front();
    }

    if (static_cast<int>(dq.size()) >= limit_) {
        return false;
    }

    dq.push_back(now_ms);
    return true;
}

} // namespace oj
