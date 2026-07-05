#include "logger.h"
#include <thread>

int main() {
    auto file = std::make_shared<file_sink>("app.log");

    // 通过 registry 注册
    auto backend = std::make_shared<logger>("myapp", file);
    backend->set_level(level::trace);
    registry::instance().register_logger(backend);

    // 任何地方都能拿到
    auto log = registry::instance().get("myapp");
    log->info("hello from registry!");

    // 也可以包成 async
    async_logger alog(log, 2);

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back([&alog, i]() {
            for (int j = 0; j < 5; j++) {
                alog.info("thread_" + std::to_string(i) + std::to_string(j));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
