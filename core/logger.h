// logger.h — 日志记录器
//
// 拥有一个 formatter 和多个 sink。
// log() 格式化消息 → 分发给所有 sink。

#pragma once
#include "common.h"
#include "formatter.h"
#include "sink.h"
#include <string>
#include <vector>
#include <memory>
#include <thread>

class logger {
public:
    /**
     * @param name    日志器名称
     * @param s       输出目标（sink）
     * @param pattern 格式模式串，如 "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"
     */
    logger(std::string name, std::shared_ptr<sink> s,
           std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v")
        : name_(std::move(name)),
          formatter_(std::make_unique<pattern_formatter>(std::move(pattern))) {
        sinks_.push_back(std::move(s));
    }

    void set_level(level lvl) { level_ = lvl; }
    const std::string& name() const { return name_; }

    /** 动态修改格式模式 */
    void set_pattern(std::string p) { formatter_->set_pattern(std::move(p)); }

    /** 格式化一条消息到 dest（供 async_logger 使用） */
    void format(log_msg& msg, std::string& dest) {
        formatter_->format(msg, dest);
    }

    bool should_log(level msg_level) const {
        return static_cast<int>(msg_level) >= static_cast<int>(level_);
    }

    /** 记录日志：创建 log_msg → 格式化 → 分发给所有 sink */
    void log(level lvl, const std::string& payload) {
        if (!should_log(lvl)) return;

        log_msg msg;
        msg.logger_name = name_;
        msg.lvl = lvl;
        msg.payload = payload;
        msg.time = std::chrono::system_clock::now();
        msg.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());

        std::string formatted;
        formatter_->format(msg, formatted);
        log_formatted(lvl, formatted);
    }

    /** 将格式化后的字符串发给所有 sink */
    void log_formatted(level lvl, const std::string& formatted) {
        for (auto& s : sinks_) {
            if (s->should_log(lvl)) {
                s->log(lvl, formatted);
            }
        }
    }

    /** 添加额外 sink（如同时输出到 stdout + 文件） */
    void add_sink(std::shared_ptr<sink> s) {
        sinks_.push_back(std::move(s));
    }

    // 便捷级别方法
    void trace(const std::string& msg) { log(level::trace, msg); }
    void debug(const std::string& msg) { log(level::debug, msg); }
    void info(const std::string& msg)  { log(level::info,  msg); }
    void warn(const std::string& msg)  { log(level::warn,  msg); }
    void error(const std::string& msg) { log(level::err,   msg); }
    void crit(const std::string& msg)  { log(level::crit,  msg); }

private:
    std::unique_ptr<pattern_formatter> formatter_;
    std::string name_;
    level level_{ level::info };
    std::vector<std::shared_ptr<sink>> sinks_;
};
