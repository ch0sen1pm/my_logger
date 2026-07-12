// macros.h — 日志宏
//
// 封装 should_log 检查，避免在不需要日志时构造字符串。
// 用法：LOG_INFO(my_logger, "hello " + std::to_string(x));

#pragma once

#define LOG_TRACE(logger, msg) if ((logger)->should_log(level::trace)) (logger)->trace(msg)
#define LOG_DEBUG(logger, msg) if ((logger)->should_log(level::debug)) (logger)->debug(msg)
#define LOG_INFO(logger, msg)  if ((logger)->should_log(level::info))  (logger)->info(msg)
#define LOG_WARN(logger, msg)  if ((logger)->should_log(level::warn))  (logger)->warn(msg)
#define LOG_ERROR(logger, msg) if ((logger)->should_log(level::err))   (logger)->error(msg)
#define LOG_CRIT(logger, msg)  if ((logger)->should_log(level::crit))  (logger)->crit(msg)
