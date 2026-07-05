#include "logger.h"
#include <thread>

int main() {
    auto file = std::make_shared<file_sink>("app.log");

    auto backend = std::make_shared<logger>("myapp", file);
    backend->set_level(level::info);  // trace/debug 会被过滤
    registry::instance().register_logger(backend);

    auto log = registry::instance().get("myapp");

    // 宏 — 参数都不会构造如果被过滤
    LOG_TRACE(log, "this will NOT run");    // trace < info → 被过滤
    LOG_DEBUG(log, "this will NOT run");    // debug < info → 被过滤
    LOG_INFO(log, "hello from macro!");
    LOG_WARN(log, "warning from macro!");

    return 0;
}
