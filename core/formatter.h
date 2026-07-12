// formatter.h — 格式化的策略模式实现
//
// flag_formatter 是抽象基类，每个 %X 标志对应一个子类。
// pattern_formatter 编译模式串 → 生成 formatters_ 链表 → format() 逐个执行。

#pragma once
#include "common.h"
#include <string>
#include <vector>
#include <memory>
#include <ctime>
#include <chrono>
#include <mutex>

// ====== 抽象标志格式化器 ======

class flag_formatter {
public:
    virtual ~flag_formatter() = default;
    /// @param tm_time 缓存的时间结构体（避免每条日志调用 localtime）
    virtual void format(const log_msg& msg, const std::tm& tm_time,
                        std::string& dest) = 0;
};

// ====== 具体 flag 实现 ======

class literal_flag : public flag_formatter {
    std::string text_;
public:
    explicit literal_flag(std::string t) : text_(std::move(t)) {}
    void format(const log_msg&, const std::tm&, std::string& dest) override {
        dest += text_;
    }
};

class name_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += msg.logger_name;
    }
};

class level_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += level_name(msg.lvl);
    }
};

class short_level_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += level_name(msg.lvl)[0];
    }
};

class message_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += msg.payload;
    }
};

class thread_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += std::to_string(msg.thread_id);
    }
};

class year_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        dest += std::to_string(tm.tm_year + 1900);
    }
};

class month_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_mon + 1, dest);
    }
};

class day_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_mday, dest);
    }
};

class hour_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_hour, dest);
    }
};

class min_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_min, dest);
    }
};

class sec_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_sec, dest);
    }
};

class msec_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.time.time_since_epoch()) % 1000;
        pad3(static_cast<int>(ms.count()), dest);
    }
};

class percent_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm&, std::string& dest) override {
        dest += '%';
    }
};

// ====== 模式编译器 ======

/**
 * 把模式串（如 "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v"）
 * 编译成 formatters_ 链表，format() 逐个执行。
 *
 * 时间缓存优化：同一条日志可能多次 format（多 sink），
 * 如果时间没变就不重新调 localtime。
 */
class pattern_formatter {
public:
    explicit pattern_formatter(std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v")
        : pattern_(std::move(pattern)) {
        compile_pattern_(pattern_);
    }

    /** 格式化一条日志消息到 dest */
    void format(const log_msg& msg, std::string& dest) {
        std::lock_guard<std::mutex> lock(fmt_mutex_);
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
            msg.time.time_since_epoch());
        if (secs != last_log_secs_) {
            std::time_t t = std::chrono::system_clock::to_time_t(msg.time);
            localtime_platform(&t, &cached_tm_);
            last_log_secs_ = secs;
        }

        for (auto& f : formatters_) {
            f->format(msg, cached_tm_, dest);
        }
        dest += '\n';
    }

    /** 动态更换模式 */
    void set_pattern(std::string pattern) {
        pattern_ = std::move(pattern);
        compile_pattern_(pattern_);
    }

private:
    /** 将模式串编译为 flag_formatter 链表 */
    void compile_pattern_(const std::string& pattern) {
        formatters_.clear();
        std::string literal;

        for (auto it = pattern.begin(); it != pattern.end(); ++it) {
            if (*it == '%' && std::next(it) != pattern.end()) {
                if (!literal.empty()) {
                    formatters_.push_back(
                        std::make_unique<literal_flag>(std::move(literal)));
                    literal.clear();
                }
                char flag = *++it;
                formatters_.push_back(create_flag_(flag));
            } else {
                literal += *it;
            }
        }

        if (!literal.empty()) {
            formatters_.push_back(
                std::make_unique<literal_flag>(std::move(literal)));
        }
    }

    /** 根据 flag 字符创建对应的 flag_formatter */
    std::unique_ptr<flag_formatter> create_flag_(char flag) {
        switch (flag) {
            case 'n': return std::make_unique<name_flag>();
            case 'l': return std::make_unique<level_flag>();
            case 'L': return std::make_unique<short_level_flag>();
            case 'v': return std::make_unique<message_flag>();
            case 't': return std::make_unique<thread_flag>();
            case 'Y': return std::make_unique<year_flag>();
            case 'm': return std::make_unique<month_flag>();
            case 'd': return std::make_unique<day_flag>();
            case 'H': return std::make_unique<hour_flag>();
            case 'M': return std::make_unique<min_flag>();
            case 'S': return std::make_unique<sec_flag>();
            case 'e': return std::make_unique<msec_flag>();
            case '%': return std::make_unique<percent_flag>();
            default:
                return std::make_unique<literal_flag>(std::string("%") + flag);
        }
    }

    std::string pattern_;
    std::vector<std::unique_ptr<flag_formatter>> formatters_;
    std::tm cached_tm_{};
    std::chrono::seconds last_log_secs_{0};
    std::mutex fmt_mutex_;
};
