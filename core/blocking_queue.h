// blocking_queue.h — 线程安全阻塞队列
//
// 用于异步日志的生产者-消费者模式。
// 前端线程 enqueue 格式化后的消息，后台 worker 线程 dequeue 并写 sink。

#pragma once
#include <queue>
#include <mutex>
#include <condition_variable>
#include <optional>

/**
 * @tparam T 队列元素类型
 */
template <typename T>
class blocking_queue {
public:
    /** 入队，并唤醒一个等待的消费者 */
    void enqueue(T item) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            q_.push(std::move(item));
        }
        cv_.notify_one();
    }

    /**
     * 出队（阻塞等待直到有数据或 stopped）
     * @return 数据，如果 stopped 且队列空则返回 std::nullopt
     */
    std::optional<T> dequeue() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !q_.empty() || stopped_; });
        if (stopped_ && q_.empty()) {
            return std::nullopt;
        }
        T item = std::move(q_.front());
        q_.pop();
        return item;
    }

    /** 停止队列，唤醒所有等待的消费者 */
    void stop() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

private:
    std::queue<T> q_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stopped_ = false;
};
