// crash_handler.h — 崩溃前自动刷盘
//
// 注册信号处理函数，在 SIGSEGV/SIGABRT/SIGINT 发生时
// 遍历所有已注册的 sink 并 flush()。
// 配合正常的 ofstream 析构——正常退出靠析构，异常退出靠这里。
//
// 用法：
//   crash_handler::instance().add(file_sink);
//   crash_handler::instance().install();

#pragma once
#include <vector>
#include <memory>
#include <csignal>
#include "sink.h"

/**
 * 崩溃刷盘器（单例）
 *
 * 原理：signal() 注册回调 → 崩溃时内核调 on_crash_() →
 * 遍历 sinks_ 调 flush() → 恢复默认处理 → 再发一次信号让进程正常挂
 */
class crash_handler {
public:
    /** @return 全局唯一实例 */
    static crash_handler& instance() {
        static crash_handler h;
        return h;
    }

    /** 注册一个 sink，崩溃时会被 flush */
    void add(std::shared_ptr<sink> s) {
        sinks_.push_back(s);
    }

    /** 安装信号处理（在 main 开头调一次） */
    void install() {
        signal(SIGSEGV, on_crash_);   // 段错误（空指针/野指针）
        signal(SIGABRT, on_crash_);   // abort() / 断言失败
        signal(SIGINT,  on_crash_);   // Ctrl+C
    }

private:
    crash_handler() = default;

    /** 信号处理回调：刷盘 → 恢复默认 → 再发一次让进程挂 */
    static void on_crash_(int sig) {
        for (auto& s : instance().sinks_) {
            s->flush();
        }
        signal(sig, SIG_DFL);   // 恢复默认处理
        raise(sig);              // 再发一次信号，让进程正常死
    }

    std::vector<std::shared_ptr<sink>> sinks_;
};