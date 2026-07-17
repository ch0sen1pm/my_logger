// json_config.h — JSON 文件初始化 logger
//
// 用一个 JSON 配置文件批量创建 logger，省去手动 new 每一个。
// 依赖 nlohmann/json（header-only，已放进 core/json.hpp）。
//
// JSON 格式：
// {
//   "app": { "level": "info", "pattern": "[%H:%M:%S] %v",
//            "sinks": ["file:app.log", "stdout"] },
//   "app.network": { "level": "debug", "sinks": ["stdout"] }
// }
// 名字按 '.' 分层 → 自动走 registry::create() 匹配父 logger。
//
// 用法：json_config::load("config.json");

#pragma once
#include "json.hpp"
#include "logger.h"
#include "registry.h"
#include "sink.h"
#include <fstream>
#include <iostream>

class json_config {
public:
    /** 从 JSON 文件批量创建 logger */
    static void load(const std::string& path) {
        std::ifstream f(path);
        if (!f.is_open()) {
            std::cerr << "[json_config] 无法打开 " << path << std::endl;
            return;
        }

        nlohmann::json j;           // ← nlohmann::json 类型（来自 json.hpp）
        f >> j;                     // 把文件内容读到 j 里

        // 遍历 JSON 的每个 key-value
        // name = logger 名称，cfg = 这个 logger 的配置（level / pattern / sinks）
        for (auto& [name, cfg] : j.items()) {
            // 1. 根据 "sinks" 数组创建 sink 对象
            auto sink = make_sink_(cfg);

            // 2. 用 registry::create 创建 logger（自动匹配父 logger）
            auto log = registry::instance().create(name, sink);

            // 3. 设 level（如果有）
            if (cfg.contains("level"))
                log->set_level(parse_level_(cfg["level"]));

            // 4. 设 pattern（如果有）
            if (cfg.contains("pattern"))
                log->set_pattern(cfg["pattern"]);

            std::cout << "[json_config] 创建 logger: " << name << std::endl;
        }
    }

private:
    /** 字符串 → level 枚举。如 "info" → level::info */
    static level parse_level_(const std::string& s) {
        if (s == "trace") return level::trace;
        if (s == "debug") return level::debug;
        if (s == "info")  return level::info;
        if (s == "warn")  return level::warn;
        if (s == "error") return level::err;
        if (s == "crit")  return level::crit;
        return level::info;  // 默认
    }

    /** 从 JSON 配置提取 sinks 数组，创建第一个 sink */
    static std::shared_ptr<sink> make_sink_(const nlohmann::json& cfg) {
        if (!cfg.contains("sinks") || cfg["sinks"].empty())
            return std::make_shared<stdout_sink>();  // 没配 sinks → 默认 stdout

        // 取第一个 sink 的规格字符串（如 "stdout"、"file:app.log"、"daily:chat"）
        std::string spec = cfg["sinks"][0].get<std::string>();
        return parse_sink_(spec);
    }

    /** 解析 sink 规格字符串 → sink 对象 */
    static std::shared_ptr<sink> parse_sink_(const std::string& spec) {
        if (spec == "stdout")
            return std::make_shared<color_stdout_sink>();   // 彩色控制台

        if (spec.rfind("file:", 0) == 0)                    // "file:xxx.log"
            return std::make_shared<file_sink>(spec.substr(5));  // 截取文件名

        if (spec.rfind("daily:", 0) == 0)                   // "daily:xxx"
            return std::make_shared<daily_rolling_sink>(spec.substr(6));

        return std::make_shared<stdout_sink>();  // 不认识的 → 默认 stdout
    }
};
