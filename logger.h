// my_logger — 从零手写的 C++ 日志库（仿 spdlog 架构）
//
// 架构分层：
//   core/common.h          — 日志级别、消息结构、跨平台适配
//   core/blocking_queue.h  — 线程安全阻塞队列（异步日志用）
//   core/formatter.h       — 策略模式：flag 标志 → 格式化链表
//   core/sink.h            — 输出目标：stdout / 文件 / 滚动文件
//   core/logger.h          — 日志记录器（formatter + sink 组合）
//   core/async_logger.h    — 异步日志器（后台 worker 线程写 sink）
//   core/registry.h        — 全局注册表（按名称查找 logger）
//   core/macros.h          — 日志宏（LOG_INFO 等）
//
// 使用示例：
//   auto sink = std::make_shared<stdout_sink>();
//   auto log  = std::make_shared<logger>("main", sink);
//   LOG_INFO(log, "hello world");

#pragma once

#include "core/common.h"
#include "core/blocking_queue.h"
#include "core/formatter.h"
#include "core/sink.h"
#include "core/logger.h"
#include "core/async_logger.h"
#include "core/registry.h"
#include "core/macros.h"
