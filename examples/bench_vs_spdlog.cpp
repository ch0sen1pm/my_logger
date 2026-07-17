// bench_vs_spdlog.cpp — my_logger vs spdlog 性能对比
#include "logger.h"
#include "core/rate_limiter.h"
#include <spdlog/spdlog.h>
#include <spdlog/sinks/basic_file_sink.h>
#include <chrono>
#include <iostream>

int main() {
    const int N = 100000;
    using clock = std::chrono::steady_clock;

    // ===== my_logger =====
    auto my_sink = std::make_shared<file_sink>("bench_my.log");
    auto my_log = std::make_shared<logger>("bench", my_sink);
    my_log->set_level(level::info);

    auto t1 = clock::now();
    for (int i = 0; i < N; i++)
        my_log->info("bench message");
    auto t2 = clock::now();
    auto my_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    // ===== spdlog =====
    auto spd = spdlog::basic_logger_mt("bench_spd", "bench_spd.log");
    spd->set_level(spdlog::level::info);

    t1 = clock::now();
    for (int i = 0; i < N; i++)
        spd->info("bench message");
    t2 = clock::now();
    auto spd_ms = std::chrono::duration_cast<std::chrono::milliseconds>(t2 - t1).count();

    // ===== 输出结果 =====
    std::cout << "\n========== Benchmark: " << N << " msgs ==========\n";
    std::cout << "my_logger: " << my_ms << "ms → "
              << (N * 1000LL / (my_ms ? my_ms : 1)) << " msg/s\n";
    std::cout << "spdlog:    " << spd_ms << "ms → "
              << (N * 1000LL / (spd_ms ? spd_ms : 1)) << " msg/s\n";
    std::cout << "ratio:     " << (double)spd_ms / my_ms << "x "
              << (my_ms < spd_ms ? "(my_logger faster)" : "(spdlog faster)") << "\n";
}
