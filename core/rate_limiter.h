// rate_limiter.h — 日志去重，防刷屏
//
// 原理：用 unordered_map 记录每条消息上次输出的时间戳。
// 相同消息在 N 秒内再次出现 → should_log() 返回 false → 跳过。
// steady_clock 保证不受系统时间调整影响。
//
// 用法：
//   rate_limiter limiter(5);   // 相同消息 5 秒内只输出一次
//   if (limiter.should_log(msg)) LOG_INFO(log, msg);

#pragma once
#include <unordered_map>
#include <string>
#include <chrono>

/**
 * 日志频率限制器
 *
 * 基于消息内容去重，不是基于级别或 logger 名称。
 * 用哈希表记录每条消息的上次输出时间，O(1) 查找。
 * 注意：不同消息越多 last_ 越大，生产环境可加 LRU 淘汰。
 */
class rate_limiter {
public:
    /** @param seconds 去重窗口（秒），相同消息此窗口内只输出第一次 */
    explicit rate_limiter(int seconds)
        : interval_(seconds) {}

    /**
     * @param msg 日志正文
     * @return true = 可以输出（没发过或窗口已过）；false = 窗口内重复，跳过
     */
    bool should_log(const std::string& msg) {
        auto now = std::chrono::steady_clock::now();
        auto it = last_.find(msg);
        if (it != last_.end() && now - it->second < interval_) {
            return false;               // 还未过窗口期
        }
        last_[msg] = now;               // 记录本次输出时间
        return true;
    }

private:
    std::chrono::seconds interval_;
    /// 消息 → 上次输出时间点。key 暴涨说明消息种类多，可能需 LRU
    std::unordered_map<std::string, std::chrono::steady_clock::time_point> last_;
};