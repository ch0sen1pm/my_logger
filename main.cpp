#include "logger.h"
#include <thread>
#include <chrono>
#include <iostream>
#include <functional>

void bench(const std::string& name, std::function<void()> fn, int total) {
    auto start = std::chrono::steady_clock::now();
    fn();
    auto end = std::chrono::steady_clock::now();

    auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(end - start).count();

    std::cout << "[" << name << "] " << total << " msgs in " << ms << "ms"
                << " → " << (total * 1000LL / (ms ? ms : 1)) << " msg/s" << std::endl;
}

int main() {
    const int N = 100000;

    auto file = std::make_shared<file_sink>("bench.log");
    file->set_level(level::off);

    auto backend = std::make_shared<logger>("bench", file);
    backend->set_level(level::info);
    registry::instance().register_logger(backend);
    auto log = registry::instance().get("bench");

    // 1.同步单线程
    bench("sync 1-thread", [&]() {
        for (int i = 0; i < N; i ++) {
            LOG_INFO(log, "bench message");
        }
    }, N);

    // 2. 同步多线程 (4 threads)
    bench("sync 4-thread", [&]() {
        std::vector<std::thread> ts;
        for (int t = 0; t < 4; t++) {
            ts.emplace_back([&, t]() {
                for (int i = 0; i < N / 4; i++) {
                    LOG_INFO(log, "bench message");
                }
            });
        }
        for (auto& t : ts) t.join();
    }, N);

    // 3. 异步单线程
    {
        async_logger alog(log);
        bench("async 1-thread", [&]() {
            for (int i = 0; i < N; i++) {
                alog.info("bench message");
            }
        }, N);
    }

    // 4. 异步多线程 (4 threads)
    {
        async_logger alog(log, 4);
        bench("async 4-thread", [&]() {
            std::vector<std::thread> ts;
            for (int t = 0; t < 4; t++) {
                ts.emplace_back([&, t]() {
                    for (int i = 0; i < N / 4; i++) {
                        alog.info("bench message");
                    }
                });
            }
            for (auto& t : ts) t.join();
        }, N);
    }


    return 0;
}