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

    void set_level(level lvl) {
        level_ = lvl;
        level_set_ = true;         // 标记已手动设过，不再继承父的 level
    }
    const std::string& name() const { return name_; }

    /** 动态修改格式模式 */
    void set_pattern(std::string p) { formatter_->set_pattern(std::move(p)); }

    /** 格式化一条消息到 dest（供 async_logger 使用） */
    void format(log_msg& msg, std::string& dest) {
        formatter_->format(msg, dest);
    }

    /**
     * 级别过滤：先看自己的 level，没设就看父的。
     * 递归往上找直到有 level 的祖先或根。
     */
    bool should_log(level msg_level) const {
        if (level_set_)   // 自己设过 → 用自己的
            return static_cast<int>(msg_level) >= static_cast<int>(level_);
        if (parent_)      // 没设 → 问父（父可能又问祖父）
            return parent_->should_log(msg_level);
        return true;      // 没父也没设 → 全过
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

    /**
     * 将格式化后的字符串发给所有 sink。
     * 先写自己的 sink，再递归写父的 sink。
     * 效果：子输出到自己 + 父的 sink（比如全局文件 sink）
     */
    void log_formatted(level lvl, const std::string& formatted) {
        for (auto& s : sinks_) {
            if (s->should_log(lvl)) s->log(lvl, formatted);
        }
        if (parent_) parent_->log_formatted(lvl, formatted);
    }

    /** 添加额外 sink（如同时输出到 stdout + 文件） */
    void add_sink(std::shared_ptr<sink> s) {
        sinks_.push_back(std::move(s));
    }

    /** 设置父 logger（registry::create() 自动调用，无需手动调） */
    void set_parent(std::shared_ptr<logger> p) { parent_ = p; }


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
    bool level_set_{false};
    std::vector<std::shared_ptr<sink>> sinks_;
    std::shared_ptr<logger> parent_;
};
