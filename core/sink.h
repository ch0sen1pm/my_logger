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
