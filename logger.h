#pragma once
#include <iostream>
#include <string>
#include <vector>
#include <memory>
#include <fstream>
#include <mutex>
#include <thread>
#include <chrono>
#include <ctime>
#include <queue>
#include <condition_variable>
#include <optional>

template <typename T>
class blocking_queue {
public:
    void enqueue(T item) {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            q_.push(std::move(item));
        }
        cv_.notify_one();
    }

    std::optional<T> dequeue() {
        std::unique_lock<std::mutex> lock(mtx_);
        cv_.wait(lock, [this] { return !q_.empty() || stopped_; });
        if (stopped_ && q_.empty()) {
            return std::nullopt;
        }
        T item = std::move(q_.front());
        q_.pop();
        return item;
    }

    void stop() {
        {
            std::lock_guard<std::mutex> lock(mtx_);
            stopped_ = true;
        }
        cv_.notify_all();
    }

private:
    std::queue<T> q_;
    std::mutex mtx_;
    std::condition_variable cv_;
    bool stopped_ = false;
};



enum class level {
    trace = 0,
    debug = 1,
    info = 2,
    warn = 3,
    err = 4,
    crit = 5,
    off = 6
};

struct log_msg {
    std::string logger_name;
    level lvl;
    std::string payload;
    std::chrono::system_clock::time_point time;
    size_t thread_id;
};

inline const char* level_name(level lvl) {
    const char* names[] = {"trace", "debug", "info", "warn", "error", "crit", "off"};
    return names[static_cast<int>(lvl)];
}

struct async_msg {
    level lvl;
    std::string formatted;
};

inline void pad2(int val, std::string& dest) {
    dest += static_cast<char>('0' + val / 10);
    dest += static_cast<char>('0' + val % 10);
}

inline void pad3(int val, std::string& dest) {
    dest += static_cast<char>('0' + val / 100);
    dest += static_cast<char>('0' + (val / 10) % 10);
    dest += static_cast<char>('0' + val % 10);
}

class flag_formatter {
public:
    virtual ~flag_formatter() = default;
    virtual void format(const log_msg& msg, const std::tm& tm_time, 
                        std::string& dest) = 0;
};

class literal_flag : public flag_formatter {
    std::string text_;
public:
    explicit literal_flag(std::string t) : text_(std::move(t)) {}
    void format(const log_msg&, const std::tm&, std::string& dest) override {
        dest += text_;
    }
};

class name_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += msg.logger_name;
    }
};

class level_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += level_name(msg.lvl);
    }
};

class short_level_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += level_name(msg.lvl)[0];
    }
};

class message_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += msg.payload;
    }
};

class thread_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        dest += std::to_string(msg.thread_id);
    }
};

class year_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        dest += std::to_string(tm.tm_year + 1900);
    }
};

class month_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_mon + 1, dest);
    }
};

class day_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_mday, dest);
    }
};

class hour_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_hour, dest);
    }
};

class min_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_min, dest);
    }
};

class sec_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm& tm, std::string& dest) override {
        pad2(tm.tm_sec, dest);
    }
};


class msec_flag : public flag_formatter {
public:
    void format(const log_msg& msg, const std::tm&, std::string& dest) override {
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
            msg.time.time_since_epoch()
        ) % 1000;
        pad3(static_cast<int>(ms.count()), dest);
    }
};

class percent_flag : public flag_formatter {
public:
    void format(const log_msg&, const std::tm&, std::string& dest) override {
        dest += '%';
    }
};

class pattern_formatter {
public:
    explicit pattern_formatter(std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v")
    : pattern_(std::move(pattern)) {
        compile_pattern_(pattern_);
    }

    void format(const log_msg& msg, std::string& dest) {
        std::lock_guard<std::mutex> lock(fmt_mutex_);
        auto secs = std::chrono::duration_cast<std::chrono::seconds>(
            msg.time.time_since_epoch()
        );
        if (secs != last_log_secs_) {
            std::time_t t = std::chrono::system_clock::to_time_t(msg.time);
            localtime_s(&cached_tm_, &t);
            last_log_secs_ = secs;
        }

        for (auto& f : formatters_) {
            f->format(msg, cached_tm_, dest);
        }
        dest += '\n';
    }
    void set_pattern(std::string pattern) {
        pattern_ = std::move(pattern);
        compile_pattern_(pattern_);
    }
private:
    void compile_pattern_(const std::string& pattern) {
        formatters_.clear();
        std::string literal;

        for (auto it = pattern.begin(); it != pattern.end(); ++ it) {
            if (*it == '%' && std::next(it) != pattern.end()) {
                if (!literal.empty()) {
                    formatters_.push_back(
                        std::make_unique<literal_flag>(std::move(literal))
                    );
                    literal.clear();
                }
                char flag = *++it;
                formatters_.push_back(create_flag_(flag));
            } else {
                literal += *it;
            }
        }

        if (!literal.empty()) {
            formatters_.push_back(
                std::make_unique<literal_flag>(std::move(literal))
            );
        }
    }

    std::unique_ptr<flag_formatter> create_flag_(char flag) {
        switch (flag) {
            case 'n': return std::make_unique<name_flag>();
            case 'l': return std::make_unique<level_flag>();
            case 'L': return std::make_unique<short_level_flag>();
            case 'v': return std::make_unique<message_flag>();
            case 't': return std::make_unique<thread_flag>();
            case 'Y': return std::make_unique<year_flag>();
            case 'm': return std::make_unique<month_flag>();
            case 'd': return std::make_unique<day_flag>();
            case 'H': return std::make_unique<hour_flag>();
            case 'M': return std::make_unique<min_flag>();
            case 'S': return std::make_unique<sec_flag>();
            case 'e': return std::make_unique<msec_flag>();
            case '%': return std::make_unique<percent_flag>();
            default:
                return std::make_unique<literal_flag>(
                    std::string("%") + flag
                );
        }
    }



    std::string pattern_;
    std::vector<std::unique_ptr<flag_formatter>> formatters_;
    std::tm cached_tm_{};
    std::chrono::seconds last_log_secs_{0};
    std::mutex fmt_mutex_;
};


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
        std::cout << msg;
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
        file_ << msg;
    }

    void flush_() override {
        file_ << std::flush;
    }

private:
    std::ofstream file_;
};

class logger {
public:
    logger(std::string name, std::shared_ptr<sink> s, 
           std::string pattern = "[%Y-%m-%d %H:%M:%S.%e] [%n] [%l] %v")
        : name_(std::move(name)),
          formatter_(std::make_unique<pattern_formatter>(std::move(pattern))) {
            sinks_.push_back(std::move(s));
        }

    void set_level(level lvl) { level_ = lvl; }

    const std::string& name() const { return name_; }



    void set_pattern(std::string p) { formatter_->set_pattern(std::move(p)); }

    void format(log_msg& msg, std::string& dest) {
        formatter_->format(msg, dest);
    }

    bool should_log(level msg_level) const {
        return static_cast<int>(msg_level) >= static_cast<int>(level_);
    }
    

    void log(level lvl, const std::string& payload) {
        if (!should_log(lvl)) {
            return ;
        }

        log_msg msg;
        msg.logger_name = name_;
        msg.lvl = lvl;
        msg.payload = payload;
        msg.time = std::chrono::system_clock::now();
        msg.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());

        std::string formatted;
        formatter_->format(msg, formatted);

        log_formatted(lvl, formatted);

    }

    
    void log_formatted(level lvl, const std::string& formatted) {
        for (auto& s : sinks_) {
            if (s->should_log(lvl)) {
                s->log(lvl, formatted);
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
    std::unique_ptr<pattern_formatter> formatter_;
    std::string name_;
    level level_{ level::info };
    std::vector<std::shared_ptr<sink>> sinks_;
};

class async_logger {
public:
    async_logger(std::shared_ptr<logger> backend)
        : backend_(std::move(backend)),
        worker_(&async_logger::run_, this) {} 

    void log(level lvl, const std::string& payload) {
        log_msg msg;
        msg.logger_name = backend_->name();
        msg.lvl = lvl;
        msg.payload = payload;
        msg.time = std::chrono::system_clock::now();
        msg.thread_id = std::hash<std::thread::id>{}(std::this_thread::get_id());

        std::string formatted;
        backend_->format(msg, formatted);

        async_msg amsg;
        amsg.lvl = lvl;
        amsg.formatted = std::move(formatted);
        q_.enqueue(std::move(amsg));
    }

    void trace(const std::string& msg) { log(level::trace, msg); }
    void debug(const std::string& msg) { log(level::debug, msg); }
    void info(const std::string& msg) { log(level::info, msg); }
    void warn(const std::string& msg) { log(level::warn, msg); }
    void error(const std::string& msg) { log(level::err, msg); }
    void crit(const std::string& msg) { log(level::crit, msg); }

    ~async_logger() {
        q_.stop();
        worker_.join();
    }

private:
    void run_() {
        while (true) {
            auto msg = q_.dequeue();
            if (!msg) {
                break;
            }
            backend_->log_formatted(msg->lvl, msg->formatted);
        }
    }

    std::shared_ptr<logger> backend_;
    blocking_queue<async_msg> q_;
    std::thread worker_;
};
