#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <mutex>

enum class level {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    err = 4,
    crit = 5,
    off = 6
};

inline const char* level_name(level lvl) {
    const char* names[] = {"trace", "debug", "info", "warn", "error", "crit", "off"};
    return names[static_cast<int>(lvl)];
}

struct null_mutex {
    void lock() {}
    void unlock() {}
};


class sink {
public: 
    virtual ~sink() = default;
    virtual void log(level lvl, const std::string& msg) = 0;
    virtual void flush() = 0;

    void set_level(level lvl) { level_ = lvl; }
    level get_level() const { return level_; }
    bool should_log(level msg_level) {
        return static_cast<int> (msg_level) >= static_cast<int>(level_);
    }

protected:
    level level_{ level::trace };
};

template <typename Mutex = null_mutex>
class base_sink : public sink {
public:
    void log(level lvl, const std::string& msg) final {
        std::lock_guard<Mutex> lock(mtx_);
        sink_it_(lvl, msg);
    }

    void flush() final {
        std::lock_guard<Mutex> lock(mtx_);
        flush_();
    }

protected:
    virtual void sink_it_(level lvl, const std::string& msg) = 0;
    virtual void flush_() = 0;
    Mutex mtx_;
};

class stdout_sink : public base_sink<> {
public:
    void sink_it_(level lvl, const std::string& msg) override {
        std::cout << "[" << level_name(lvl) << "]" << msg << std::endl;
    }

    void flush_() override {
        std::cout << std::flush;
    }
};


class file_sink : public base_sink<std::mutex> {
public:
    file_sink(const std::string& filename) {
        file_.open(filename);
    }

    void sink_it_(level lvl, const std::string& msg) override {
        file_ << "[" << level_name(lvl) << "]" << msg << std::endl;
    }

    void flush_() override {
        file_ << std::flush;
    }

private:
    std::ofstream file_;
};

class logger {
public:
    logger(std::string name, std::shared_ptr<sink> s)
    : name_(std::move(name)) {
        sinks_.push_back(std::move(s));
    }

    void set_level(level lvl) { level_ = lvl; }

    bool should_log(level msg_level) const {
        return static_cast<int>(msg_level) >= static_cast<int>(level_);
    }

    void log(level lvl, const std::string& msg) {
        if (!should_log(lvl)) {
            return ;
        }

        for (auto& s : sinks_) {
            if (s->should_log(lvl)) {
                s->log(lvl, msg);
            }
        }

    }

    void add_sink(std::shared_ptr<sink> s) {
        sinks_.push_back(std::move(s));
    }

    void trace(const std::string& msg) { log(level::trace, msg); }
    void debug(const std::string& msg) { log(level::debug, msg); }
    void info(const std::string& msg) { log(level::info, msg); }
    void warn(const std::string& msg) { log(level::warn, msg); }
    void error(const std::string& msg) { log(level::err, msg); }
    void crit(const std::string& msg) { log(level::crit, msg); }

private:
    std::string name_;
    level level_{ level::info };
    std::vector<std::shared_ptr<sink>> sinks_;
};
