#ifdef _WIN32

#include "platform/windows/autostart.h"

#include <windows.h>

#include <string>

namespace zibby::platform::windows {

namespace {

std::string win32ErrorMessage(DWORD code) {
    LPSTR buffer = nullptr;
    const DWORD flags = FORMAT_MESSAGE_ALLOCATE_BUFFER | FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS;
    const DWORD langId = MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT);
    const DWORD len = FormatMessageA(flags, nullptr, code, langId, reinterpret_cast<LPSTR>(&buffer), 0, nullptr);

    std::string msg;
    if (len > 0 && buffer != nullptr) {
        msg.assign(buffer, buffer + len);
        while (!msg.empty() && (msg.back() == '\r' || msg.back() == '\n' || msg.back() == ' ' || msg.back() == '\t')) {
            msg.pop_back();
        }
    }
    if (buffer != nullptr) {
        LocalFree(buffer);
    }

    if (msg.empty()) {
        return std::string("code=") + std::to_string(code);
    }
    return std::string("code=") + std::to_string(code) + ": " + msg;
}

AutostartResult ok() {
    AutostartResult r;
    r.ok = true;
    return r;
}

AutostartResult fail(const std::string& e) {
    AutostartResult r;
    r.ok = false;
    r.error = e;
    return r;
}

} // namespace

AutostartResult enableAutostartRunKey(const std::string& valueName, const std::string& commandLine) {
    HKEY key = nullptr;
    const auto status = RegCreateKeyExA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        nullptr,
        0,
        KEY_SET_VALUE,
        nullptr,
        &key,
        nullptr);

    if (status != ERROR_SUCCESS) {
        return fail(win32ErrorMessage(static_cast<DWORD>(status)));
    }

    const auto setStatus = RegSetValueExA(
        key,
        valueName.c_str(),
        0,
        REG_SZ,
        reinterpret_cast<const BYTE*>(commandLine.c_str()),
        static_cast<DWORD>(commandLine.size() + 1));

    RegCloseKey(key);

    if (setStatus != ERROR_SUCCESS) {
        return fail(win32ErrorMessage(static_cast<DWORD>(setStatus)));
    }

    return ok();
}

AutostartResult disableAutostartRunKey(const std::string& valueName) {
    HKEY key = nullptr;
    const auto status = RegOpenKeyExA(
        HKEY_CURRENT_USER,
        "Software\\Microsoft\\Windows\\CurrentVersion\\Run",
        0,
        KEY_SET_VALUE,
        &key);

    if (status != ERROR_SUCCESS) {
        // If key doesn't exist, treat as success.
        return ok();
    }

    const auto delStatus = RegDeleteValueA(key, valueName.c_str());
    RegCloseKey(key);

    if (delStatus == ERROR_FILE_NOT_FOUND) {
        return ok();
    }
    if (delStatus != ERROR_SUCCESS) {
        return fail(win32ErrorMessage(static_cast<DWORD>(delStatus)));
    }

    return ok();
}

} // namespace zibby::platform::windows

#endif
