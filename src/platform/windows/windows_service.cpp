#ifdef _WIN32

#include "platform/windows/windows_service.h"

#include "core/service.h"
#include "utils/logger.h"

#include <windows.h>

#include <atomic>
#include <mutex>
#include <string>
#include <thread>

namespace zibby::platform::windows {

namespace {

constexpr DWORD kServiceStopWaitMs = 30000;

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

ServiceCommandResult ok() {
    ServiceCommandResult r;
    r.ok = true;
    return r;
}

ServiceCommandResult fail(const std::string& e) {
    ServiceCommandResult r;
    r.ok = false;
    r.error = e;
    return r;
}

// --- Service runtime (SCM callbacks) ---

std::mutex g_mutex;
SERVICE_STATUS_HANDLE g_statusHandle = nullptr;
SERVICE_STATUS g_status{};
std::atomic<bool> g_stopRequested{false};
std::thread g_worker;
std::unique_ptr<zibby::core::Service> g_service;

void setStatus(DWORD state, DWORD win32ExitCode = NO_ERROR, DWORD waitHintMs = 0) {
    std::lock_guard<std::mutex> lock(g_mutex);

    g_status.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
    g_status.dwCurrentState = state;
    g_status.dwWin32ExitCode = win32ExitCode;
    g_status.dwWaitHint = waitHintMs;

    if (state == SERVICE_START_PENDING) {
        g_status.dwControlsAccepted = 0;
    } else {
        g_status.dwControlsAccepted = SERVICE_ACCEPT_STOP | SERVICE_ACCEPT_SHUTDOWN;
    }

    static DWORD checkpoint = 1;
    if (state == SERVICE_RUNNING || state == SERVICE_STOPPED) {
        g_status.dwCheckPoint = 0;
        checkpoint = 1;
    } else {
        g_status.dwCheckPoint = checkpoint++;
    }

    if (g_statusHandle != nullptr) {
        SetServiceStatus(g_statusHandle, &g_status);
    }
}

void WINAPI serviceCtrlHandler(DWORD ctrl) {
    if (ctrl == SERVICE_CONTROL_STOP || ctrl == SERVICE_CONTROL_SHUTDOWN) {
        g_stopRequested.store(true);
        setStatus(SERVICE_STOP_PENDING, NO_ERROR, kServiceStopWaitMs);
        if (g_service) {
            g_service->requestStop();
        }
        return;
    }
}

void WINAPI serviceMain(DWORD /*argc*/, LPSTR* /*argv*/) {
    g_statusHandle = RegisterServiceCtrlHandlerA("zibby-service", serviceCtrlHandler);
    if (g_statusHandle == nullptr) {
        return;
    }

    setStatus(SERVICE_START_PENDING, NO_ERROR, 5000);

    g_worker = std::thread([]() {
        try {
            if (!g_service) {
                return;
            }
            const int code = g_service->run(true);
            (void)code;
        } catch (...) {
        }
    });

    setStatus(SERVICE_RUNNING);

    // Wait for worker to finish.
    if (g_worker.joinable()) {
        g_worker.join();
    }

    setStatus(SERVICE_STOPPED);
}

} // namespace

int runAsWindowsService(const zibby::core::Config& config) {
    g_service = std::make_unique<zibby::core::Service>(config);

    SERVICE_TABLE_ENTRYA table[] = {
        {const_cast<LPSTR>("zibby-service"), serviceMain},
        {nullptr, nullptr}};

    if (!StartServiceCtrlDispatcherA(table)) {
        const auto code = GetLastError();
        return static_cast<int>(code);
    }

    return 0;
}

ServiceCommandResult installWindowsService(const std::string& serviceName, const std::string& displayName, const std::string& exePathWithArgs) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CREATE_SERVICE);
    if (scm == nullptr) {
        return fail(win32ErrorMessage(GetLastError()));
    }

    SC_HANDLE svc = CreateServiceA(
        scm,
        serviceName.c_str(),
        displayName.c_str(),
        SERVICE_ALL_ACCESS,
        SERVICE_WIN32_OWN_PROCESS,
        SERVICE_AUTO_START,
        SERVICE_ERROR_NORMAL,
        exePathWithArgs.c_str(),
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);

    if (svc == nullptr) {
        const auto err = GetLastError();
        CloseServiceHandle(scm);
        return fail(win32ErrorMessage(err));
    }

    SERVICE_DESCRIPTIONA desc;
    desc.lpDescription = const_cast<LPSTR>("zibby-service backend (v3lom) - https://github.com/v3lom/zibby-service");
    ChangeServiceConfig2A(svc, SERVICE_CONFIG_DESCRIPTION, &desc);

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok();
}

ServiceCommandResult uninstallWindowsService(const std::string& serviceName) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm == nullptr) {
        return fail(win32ErrorMessage(GetLastError()));
    }

    SC_HANDLE svc = OpenServiceA(scm, serviceName.c_str(), DELETE | SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (svc == nullptr) {
        const auto err = GetLastError();
        CloseServiceHandle(scm);
        return fail(win32ErrorMessage(err));
    }

    SERVICE_STATUS status;
    if (ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
        // best-effort wait
        for (int i = 0; i < 60; ++i) {
            SERVICE_STATUS_PROCESS sp{};
            DWORD bytesNeeded = 0;
            if (!QueryServiceStatusEx(svc, SC_STATUS_PROCESS_INFO, reinterpret_cast<LPBYTE>(&sp), sizeof(sp), &bytesNeeded)) {
                break;
            }
            if (sp.dwCurrentState == SERVICE_STOPPED) {
                break;
            }
            Sleep(500);
        }
    }

    if (!DeleteService(svc)) {
        const auto err = GetLastError();
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return fail(win32ErrorMessage(err));
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok();
}

ServiceCommandResult startWindowsService(const std::string& serviceName) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm == nullptr) {
        return fail(win32ErrorMessage(GetLastError()));
    }

    SC_HANDLE svc = OpenServiceA(scm, serviceName.c_str(), SERVICE_START | SERVICE_QUERY_STATUS);
    if (svc == nullptr) {
        const auto err = GetLastError();
        CloseServiceHandle(scm);
        return fail(win32ErrorMessage(err));
    }

    if (!StartServiceA(svc, 0, nullptr)) {
        const auto err = GetLastError();
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return fail(win32ErrorMessage(err));
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok();
}

ServiceCommandResult stopWindowsService(const std::string& serviceName) {
    SC_HANDLE scm = OpenSCManagerA(nullptr, nullptr, SC_MANAGER_CONNECT);
    if (scm == nullptr) {
        return fail(win32ErrorMessage(GetLastError()));
    }

    SC_HANDLE svc = OpenServiceA(scm, serviceName.c_str(), SERVICE_STOP | SERVICE_QUERY_STATUS);
    if (svc == nullptr) {
        const auto err = GetLastError();
        CloseServiceHandle(scm);
        return fail(win32ErrorMessage(err));
    }

    SERVICE_STATUS status;
    if (!ControlService(svc, SERVICE_CONTROL_STOP, &status)) {
        const auto err = GetLastError();
        CloseServiceHandle(svc);
        CloseServiceHandle(scm);
        return fail(win32ErrorMessage(err));
    }

    CloseServiceHandle(svc);
    CloseServiceHandle(scm);
    return ok();
}

} // namespace zibby::platform::windows

#endif
