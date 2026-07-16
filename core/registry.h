// registry.h — 全局日志器注册表（单例）
//
// 集中管理所有 logger 实例，通过名称查找。
// 类似于 spdlog 的 registry。

#pragma once
#include "logger.h"
#include <unordered_map>
#include <mutex>

class registry {
public:
    /** @return 全局唯一 registry 实例 */
    static registry& instance() {
        static registry r;
        return r;
    }

    /** 注册一个 logger（名称重复则覆盖） */
    void register_logger(std::shared_ptr<logger> log) {
        std::lock_guard<std::mutex> lock(mtx_);
        loggers_[log->name()] = std::move(log);
    }

    /**
     * 根据名称查找 logger
     * @return 找到的 logger，不存在返回 nullptr
     */
    std::shared_ptr<logger> get(const std::string& name) {
        std::lock_guard<std::mutex> lock(mtx_);
        auto it = loggers_.find(name);
        if (it != loggers_.end()) return it->second;
        return nullptr;
    }

    std::shared_ptr<logger> create(const std::string& name,
                                    std::shared_ptr<sink> s) {
        auto log = std::make_shared<logger>(name, std::move(s));

        auto pos = name.rfind('.');
        if (pos != std::string::npos) {
            std::string parent_name = name.substr(0, pos);
            auto parent = get(parent_name);
            if (parent) {
                log->set_parent(parent);
            }
        }

        register_logger(log);
        return log;
    }

private:
    registry() = default;
    std::unordered_map<std::string, std::shared_ptr<logger>> loggers_;
    std::mutex mtx_;
};
