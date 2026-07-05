#include "logger.h"
#include <thread>

int main() {
    auto file = std::make_shared<file_sink>("app.log");

    auto backend = std::make_shared<logger>("myapp", file);
    backend->set_level(level::trace);

    async_logger log(backend, 3);  // 3 workers

    std::vector<std::thread> threads;
    for (int i = 0; i < 3; i++) {
        threads.emplace_back([&log, i]() {
            for (int j = 0; j < 10; j++) {
                log.info("thread_" + std::to_string(i) + std::to_string(j));
            }
        });
    }

    for (auto& t : threads) {
        t.join();
    }

    return 0;
}
