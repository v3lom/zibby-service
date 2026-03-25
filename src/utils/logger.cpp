#include "utils/logger.h"

#include <boost/filesystem.hpp>
#include <chrono>
#include <iomanip>
#include <iostream>

namespace zibby::utils {

namespace {
const char* toString(LogLevel level) {
    switch (level) {
    case LogLevel::Debug:
        return "DEBUG";
    case LogLevel::Info:
        return "INFO";
    case LogLevel::Warn:
        return "WARN";
    case LogLevel::Error:
        return "ERROR";
    }
    return "INFO";
}
} // namespace

Logger& Logger::instance() {
    static Logger logger;
    return logger;
}

bool Logger::initialize(const std::string& logPath) {
    std::lock_guard<std::mutex> lock(mutex_);
    const boost::filesystem::path path(logPath);
    if (!path.parent_path().empty()) {
        boost::filesystem::create_directories(path.parent_path());
    }
    stream_.open(logPath, std::ios::app);
    return stream_.is_open();
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard<std::mutex> lock(mutex_);

    auto now = std::chrono::system_clock::now();
    auto t = std::chrono::system_clock::to_time_t(now);

    std::tm tm{};
#ifdef _WIN32
    localtime_s(&tm, &t);
#else
    localtime_r(&t, &tm);
#endif

    if (stream_.is_open()) {
        stream_ << std::put_time(&tm, "%Y-%m-%d %H:%M:%S") << " [" << toString(level) << "] " << message << '\n';
        stream_.flush();
    }

    std::cout << "[" << toString(level) << "] " << message << std::endl;
}

} // namespace zibby::utils
