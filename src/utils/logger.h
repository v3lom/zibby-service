#pragma once

#include <fstream>
#include <mutex>
#include <string>

namespace zibby::utils {

enum class LogLevel {
    Debug,
    Info,
    Warn,
    Error
};

class Logger {
public:
    static Logger& instance();

    bool initialize(const std::string& logPath);
    void log(LogLevel level, const std::string& message);

private:
    Logger() = default;

    std::mutex mutex_;
    std::ofstream stream_;
};

} // namespace zibby::utils
