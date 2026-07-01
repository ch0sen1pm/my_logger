# my_logger

轻量级 header-only C++17 日志库，参考 [spdlog](https://github.com/gabime/spdlog) 架构从零手写。作为学习项目——每一行代码都为了理解工业级 logger 的底层设计。

A lightweight, header-only C++17 logging library inspired by spdlog. Built from scratch to understand how industrial-grade loggers work.

## Features

- **Header-only** — 单文件 `logger.h`，拖进项目就能用
- **7 log levels** — `trace / debug / info / warn / error / crit / off`
- **Custom pattern** — 自定义格式串，支持 13 个 flag
- **Multiple sinks** — `stdout_sink` / `file_sink`，可扩展自定义 sink
- **Thread-safe** — 模板化 mutex 策略（`std::mutex` 或 `null_mutex`），formatter 单独加锁
- **Multi-thread verified** — 多线程并发写入测试通过

## Quick Start

```cpp
#include "logger.h"

int main() {
    auto file = std::make_shared<file_sink>("app.log");
    logger log("myapp", file);
    log.set_level(level::trace);

    log.info("server started");
    log.warn("connection timeout");
    log.error("failed to connect");

    // 自定义 pattern
    log.set_pattern("[%H:%M:%S.%e] [%L] %v");
    log.debug("debug message");
}
```

## Pattern Flags

默认格式：`[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v`

| Flag | 含义 | 输出示例 |
|------|------|---------|
| `%n` | logger 名称 | `myapp` |
| `%l` | 日志级别（全名） | `info` |
| `%L` | 日志级别（简写） | `I` |
| `%v` | 消息正文 | `hello world` |
| `%t` | 线程 ID | `14032` |
| `%Y` | 年（4位） | `2026` |
| `%m` | 月（01-12） | `07` |
| `%d` | 日（01-31） | `01` |
| `%H` | 时（00-23） | `23` |
| `%M` | 分（00-59） | `15` |
| `%S` | 秒（00-59） | `30` |
| `%e` | 毫秒（3位） | `123` |
| `%%` | 字面量 `%` | `%` |

## Architecture

```
logger::log()
  → 构造 log_msg { name, level, payload, timestamp, thread_id }
  → pattern_formatter::format(msg)
      → 遍历 flag_formatter[] 拼出完整字符串
  → sink::log(formatted_msg)
      → base_sink<Mutex>::lock → sink_it_() → unlock
```

### Key Design

**Sink 多态体系**：`sink` 是抽象基类，`base_sink<Mutex>` 通过模板注入锁策略。<br>
**Formatter 虚函数分发**：每个 `%flag` 对应一个 `flag_formatter` 子类。pattern 串在构造时编译成 formatter 对象数组——运行时无字符串解析开销。<br>
**tm 缓存**：`localtime_s()` 仅在秒数变化时调用，非每次 log。专用 mutex 保护防止 data race。<br>
**Sink 只做输出**：sink 拿到的已经是完整格式化字符串，格式化职责完全由 `pattern_formatter` 负责——关注点分离。

## Build

```bash
g++ -std=c++17 main.cpp -o main.exe
```

无外部依赖，需 C++17（`std::make_unique`）。

## Roadmap

- [ ] Async logging（线程池 + 无锁队列）
- [ ] 编译期日志级别过滤（compile-time level filter）
- [ ] Rotating file sink（按大小/时间切日志）
- [ ] Registry（全局 logger 注册表）
- [ ] Benchmark

## License

MIT
