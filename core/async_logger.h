// async_logger.h — 异步日志器
//
// 包装一个 logger，前端线程只格式化消息并丢队列，
// 后台 worker 线程负责实际写 sink。大幅降低日志对前端的延迟影响。

#pragma once
#include "logger.h"
#include "blocking_queue.h"
#include <memory>
#include <thread>
#include <vector>

class async_logger {
public:
    /**
     * @param backend   被包装的同步 logger
     * @param n_threads 后台 worker 线程数（默认 1）
     */
    async_logger(std::shared_ptr<logger> backend, size_t n_threads = 1)
        : backend_(std::move(backend)) {
        for (size_t i = 0; i < n_threads; i++) {
            workers_.emplace_back(&async_logger::run_, this);
        }
    }

    /** 格式化消息 → 入队（前端线程调用） */
    void log(level lvl, const std::string& payload) {
        log_msg msg;
        msg.logger_name = backend_->name();
        msg.lvl = lvl;
        msg.payload = payload;
        msg.time = std::chrono::system_clock::now();
        msg.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());

        std::string formatted;
        backend_->format(msg, formatted);

        async_msg amsg;
        amsg.lvl = lvl;
        amsg.formatted = std::move(formatted);
        q_.enqueue(std::move(amsg));
    }

    void trace(const std::string& msg) { log(level::trace, msg); }
    void debug(const std::string& msg) { log(level::debug, msg); }
    void info(const std::string& msg)  { log(level::info,  msg); }
    void warn(const std::string& msg)  { log(level::warn,  msg); }
    void error(const std::string& msg) { log(level::err,   msg); }
    void crit(const std::string& msg)  { log(level::crit,  msg); }

    ~async_logger() {
        q_.stop();
        for (auto& t : workers_) {
            t.join();
        }
    }

private:
    /** worker 线程入口：出队 → 调 backend->log_formatted */
    void run_() {
        while (true) {
            auto msg = q_.dequeue();
            if (!msg) break;
            backend_->log_formatted(msg->lvl, msg->formatted);
        }
    }

    std::shared_ptr<logger> backend_;
    blocking_queue<async_msg> q_;
    std::vector<std::thread> workers_;
};
