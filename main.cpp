#include "logger.h"

int main() {
    auto rs = std::make_shared<rotating_file_sink>("app", 100);  // 100字节就切

    logger log("myapp", rs);
    log.set_level(level::trace);

    // 每条约40字节 → 写3条就会切一次
    log.info("hello world! this is a test message number 1");
    log.info("hello world! this is a test message number 2");
    log.info("hello world! this is a test message number 3");
    log.info("hello world! this is a test message number 4");
    log.info("hello world! this is a test message number 5");

    return 0;
}
