#pragma once

#include <cstdlib>
#include <string>

#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace zibby::utils {

inline bool isStdoutTty() {
#ifdef _WIN32
    DWORD mode = 0;
    HANDLE h = GetStdHandle(STD_OUTPUT_HANDLE);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) {
        return false;
    }
    return GetConsoleMode(h, &mode) != 0;
#else
    return ::isatty(STDOUT_FILENO) != 0;
#endif
}

inline bool isStderrTty() {
#ifdef _WIN32
    DWORD mode = 0;
    HANDLE h = GetStdHandle(STD_ERROR_HANDLE);
    if (h == INVALID_HANDLE_VALUE || h == nullptr) {
        return false;
    }
    return GetConsoleMode(h, &mode) != 0;
#else
    return ::isatty(STDERR_FILENO) != 0;
#endif
}

inline bool colorsAllowedByEnv() {
    // NO_COLOR is a de-facto standard.
    const char* noColor = std::getenv("NO_COLOR");
    if (noColor != nullptr) {
        return false;
    }
    return true;
}

struct Ansi {
    bool enabled = false;

    static Ansi forStdout() {
        Ansi a;
        a.enabled = colorsAllowedByEnv() && isStdoutTty();
        return a;
    }

    static Ansi forStderr() {
        Ansi a;
        a.enabled = colorsAllowedByEnv() && isStderrTty();
        return a;
    }

    const char* reset() const { return enabled ? "\x1b[0m" : ""; }
    const char* bold() const { return enabled ? "\x1b[1m" : ""; }

    const char* red() const { return enabled ? "\x1b[31m" : ""; }
    const char* green() const { return enabled ? "\x1b[32m" : ""; }
    const char* yellow() const { return enabled ? "\x1b[33m" : ""; }
    const char* cyan() const { return enabled ? "\x1b[36m" : ""; }
    const char* gray() const { return enabled ? "\x1b[90m" : ""; }

    std::string wrap(const char* code, const std::string& text) const {
        if (!enabled) {
            return text;
        }
        return std::string(code) + text + reset();
    }
};

} // namespace zibby::utils
