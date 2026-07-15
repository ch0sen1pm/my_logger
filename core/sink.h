// sink.h — 日志输出目标（策略模式）
//
// sink 是抽象基类。提供 stdout_sink、file_sink、rotating_file_sink。
// base_sink 是模板类，通过 Mutex 模板参数控制线程安全。

#pragma once
#include "common.h"
#include <iostream>
#include <fstream>
#include <string>
#include <mutex>
#include <chrono>
#include <ctime>
#include <memory>
#include <cstdio>

// ====== 互斥锁包装 ======

/// 空互斥锁（单线程模式，零开销）
struct null_mutex {
    void lock() {}
    void unlock() {}
};

// ====== 抽象 Sink ======

class sink {
public:
    virtual ~sink() = default;
    virtual void log(level lvl, const std::string& msg) = 0;
    virtual void flush() = 0;

    void set_level(level lvl) { level_ = lvl; }
    level get_level() const { return level_; }
    bool should_log(level msg_level) {
        return static_cast<int>(msg_level) >= static_cast<int>(level_);
    }

protected:
    level level_{ level::trace };
};

// ====== 模板基类：注入锁策略 ======

/**
 * @tparam Mutex 锁类型。null_mutex = 单线程，std::mutex = 线程安全
 */
template <typename Mutex = null_mutex>
class base_sink : public sink {
public:
    void log(level lvl, const std::string& msg) final {
        std::lock_guard<Mutex> lock(mtx_);
        sink_it_(lvl, msg);
    }

    void flush() final {
        std::lock_guard<Mutex> lock(mtx_);
        flush_();
    }

protected:
    virtual void sink_it_(level lvl, const std::string& msg) = 0;
    virtual void flush_() = 0;
    Mutex mtx_;
};

// ====== 标准输出 Sink ======

class stdout_sink : public base_sink<> {
public:
    void sink_it_(level lvl, const std::string& msg) override {
        std::cout << msg;
    }
    void flush_() override {
        std::cout << std::flush;
    }
};

// ====== 文件 Sink ======

class file_sink : public base_sink<std::mutex> {
public:
    file_sink(const std::string& filename) {
        file_.open(filename);
    }
    void sink_it_(level lvl, const std::string& msg) override {
        file_ << msg;
        file_ << std::flush;  // 每条立即刷盘（调试用，生产可关）
    }
    void flush_() override {
        file_ << std::flush;
    }
private:
    std::ofstream file_;
};

// ====== 滚动文件 Sink（按大小切分） ======

/**
 * 当日志文件超出 max_size 时自动重命名并创建新文件。
 * 旧文件命名格式：basename_20260708_221530.log
 */
class rotating_file_sink : public base_sink<std::mutex> {
public:
    rotating_file_sink(const std::string& base_name, size_t max_size = 1024 * 1024)
        : base_name_(base_name), max_size_(max_size) {
        open_new_file_();
    }

    void sink_it_(level lvl, const std::string& msg) override {
        file_ << msg;
        bytes_written_ += msg.size();

        if (bytes_written_ >= max_size_) {
            file_.close();
            rotate_();
            bytes_written_ = 0;
            open_new_file_();
        }
    }

    void flush_() override {
        file_ << std::flush;
    }

private:
    void open_new_file_() {
        file_.open(base_name_ + ".log", std::ios::app);
    }

    void rotate_() {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_platform(&t, &tm_buf);
        char ts[32];
        std::strftime(ts, sizeof(ts), "%Y%m%d-%H%M%S", &tm_buf);

        auto old = base_name_ + ".log";
        auto rotated = base_name_ + "_" + ts + ".log";
        std::rename(old.c_str(), rotated.c_str());
    }

    std::string base_name_;
    size_t max_size_;
    size_t bytes_written_ = 0;
    std::ofstream file_;
};

/**
 * 按天切分日志文件
 *
 * 每次写入前检查日期是否变化。跨天了就关掉昨天的文件，
 * 用新日期开一个新文件（如 chat_20260715.log）。
 * 比 rotating_file_sink 更简单——不需要比较文件大小，
 * 只需要比较日期字符串。
 */
class daily_rolling_sink : public base_sink<std::mutex> {
public:
    /** @param base_name 日志文件前缀，如 "chat" → chat_20260715.log */
    daily_rolling_sink(const std::string& base_name)
        : base_name_(base_name) {
        last_date_ = today_();
        open_new_file_();
    }

    /** 写消息前先检查是否跨天，跨天了就切文件 */
    void sink_it_(level lvl, const std::string& msg) override {
        std::string today = today_();

        if (today != last_date_) {
            file_.close();     // 关旧文件
            last_date_ = today;
            open_new_file_();   // 开新文件
        }
        file_ << msg;
        file_ << std::flush;    // 每条刷盘，调试用
    }

    void flush_() override {
        file_ << std::flush;
    }

private:
    void open_new_file_() {
        file_.open(file_name_(last_date_), std::ios::app);
    }


    std::string today_() const {
        auto now = std::chrono::system_clock::now();
        auto t = std::chrono::system_clock::to_time_t(now);
        std::tm tm_buf;
        localtime_platform(&t, &tm_buf);
        char buf[16];
        std::strftime(buf, sizeof(buf), "%Y%m%d", &tm_buf);
        return buf;
    }

    std::string file_name_(const std::string& date) const {
        return base_name_ + "_" + date + ".log";
    }

    std::string base_name_;
    std::string last_date_;
    std::ofstream file_;
};



/**
 * 带 ANSI 颜色的控制台 Sink
 *
 * 原理：在消息前后嵌入 ANSI 转义码（\033[XXm），
 * 终端看到这些码不会打印字符，而是切换前景色。
 * \033[0m 表示复位颜色，防止后续输出被染色。
 *
 * 颜色映射：
 *   trace = 灰, debug = 青, info = 绿, warn = 黄, error = 红, crit = 亮红
 *
 * 注意：Windows 传统 cmd 不支持 ANSI，Linux/Mac/WSL 正常显示。
 */
class color_stdout_sink : public base_sink<> {
public:
    /** 着色 → 输出消息 → 复位 */
    void sink_it_(level lvl, const std::string& msg) override {
        std::cout << color_code_(lvl)
                  << msg
                  << "\033[0m";
    }

    void flush_() override {
        std::cout << std::flush;
    }

private:
    /** @return 日志级别对应的 ANSI 前景色码 */
    const char* color_code_(level lvl) const {
        switch(lvl) {
            case level::trace: return "\033[90m";  // 灰
            case level::debug: return "\033[36m";  // 青
            case level::info:  return "\033[32m";  // 绿
            case level::warn:  return "\033[33m";  // 黄
            case level::err:   return "\033[31m";  // 红
            case level::crit:  return "\033[91m";  // 亮红
            default:           return "";
        }
    }
};
