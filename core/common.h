// common.h — 日志级别、消息结构、平台适配工具

#pragma once
#include <string>
#include <chrono>
#include <ctime>

/// 跨平台 localtime：Windows 用 localtime_s，Linux 用 localtime_r
inline void localtime_platform(const std::time_t* t, std::tm* tm) {
#ifdef _WIN32
    localtime_s(tm, t);
#else
    localtime_r(t, tm);
#endif
}

/// 日志级别
enum class level {
    trace = 0,
    debug = 1,
    info  = 2,
    warn  = 3,
    err   = 4,
    crit  = 5,
    off   = 6
};

/// @return 日志级别的字符串名（trace/debug/info/...）
inline const char* level_name(level lvl) {
    const char* names[] = {"trace", "debug", "info", "warn", "error", "crit", "off"};
    return names[static_cast<int>(lvl)];
}

/// 日志消息（同步版：单线程格式化 + 写 sink）
struct log_msg {
    std::string logger_name;
    level lvl;
    std::string payload;
    std::chrono::system_clock::time_point time;
    size_t thread_id;
};

/// 异步日志消息（格式化后的字符串，丢队列等 worker 写）
struct async_msg {
    level lvl;
    std::string formatted;
};

/// 两位数补零，输出到 dest
inline void pad2(int val, std::string& dest) {
    dest += static_cast<char>('0' + val / 10);
    dest += static_cast<char>('0' + val % 10);
}

/// 三位数补零，输出到 dest（毫秒用）
inline void pad3(int val, std::string& dest) {
    dest += static_cast<char>('0' + val / 100);
    dest += static_cast<char>('0' + (val / 10) % 10);
    dest += static_cast<char>('0' + val % 10);
}
