#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <regex>
#include <cctype>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

#ifdef _WIN32
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <sys/ioctl.h>
#include <termios.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

struct TermSize {
    int cols = 80;
    int rows = 24;
};

enum class Lang {
    En,
    Ru,
};

struct Strings {
    Lang lang = Lang::En;

    const char* title() const { return lang == Lang::Ru ? "Zibby • Сборщик/Установщик (TUI)" : "Zibby • Builder/Installer (TUI)"; }
    const char* subtitle() const { return lang == Lang::Ru ? "Стрелки: навигация • Space: переключить • Enter: выполнить" : "Arrows: navigate • Space: toggle • Enter: run"; }

    const char* itemBuildType() const { return lang == Lang::Ru ? "Тип сборки" : "Build type"; }
    const char* itemTests() const { return lang == Lang::Ru ? "Тесты" : "Tests"; }
    const char* itemCalls() const { return lang == Lang::Ru ? "Модуль звонков" : "Calls module"; }
    const char* itemPlugins() const { return lang == Lang::Ru ? "Плагины" : "Plugins"; }
    const char* itemPanel() const { return lang == Lang::Ru ? "Qt панель (Windows)" : "Qt panel (Windows)"; }

    const char* actionBuild() const { return lang == Lang::Ru ? "Собрать и упаковать" : "Build and package"; }
    const char* actionUpdate() const { return lang == Lang::Ru ? "Проверить/обновить исходники (git)" : "Check/update sources (git)"; }
    const char* actionDeps() const { return lang == Lang::Ru ? "Установить зависимости" : "Install dependencies"; }
    const char* actionExit() const { return lang == Lang::Ru ? "Выход" : "Exit"; }

    const char* hintKeys() const {
        return lang == Lang::Ru
            ? "Подсказка: U=обновить • B=собрать • D=зависимости • Q=выход"
            : "Hint: U=update • B=build • D=deps • Q=quit";
    }

    const char* statusReady() const { return lang == Lang::Ru ? "Готово" : "Ready"; }
    const char* statusRunning() const { return lang == Lang::Ru ? "Выполняется..." : "Running..."; }

    const char* msgNotGitRepo() const { return lang == Lang::Ru ? "Не похоже на git-репозиторий (.git не найден)" : "Not a git repository (.git not found)"; }
    const char* msgGitNotFound() const { return lang == Lang::Ru ? "git не найден в PATH" : "git not found on PATH"; }
    const char* msgNoUpstream() const { return lang == Lang::Ru ? "У ветки нет upstream (origin/main?)" : "Branch has no upstream (origin/main?)"; }

    const char* msgUpdateAvailable() const { return lang == Lang::Ru ? "Доступно обновление исходников" : "Source update available"; }
    const char* msgUpToDate() const { return lang == Lang::Ru ? "Исходники актуальны" : "Sources are up to date"; }

    const char* msgConfirmPull() const { return lang == Lang::Ru ? "Нажмите Enter чтобы сделать git pull, Esc чтобы отменить" : "Press Enter to git pull, Esc to cancel"; }
    const char* msgPulling() const { return lang == Lang::Ru ? "Обновляю (git pull)..." : "Updating (git pull)..."; }
    const char* msgPulled() const { return lang == Lang::Ru ? "Обновлено" : "Updated"; }

    const char* msgDepsDone() const { return lang == Lang::Ru ? "Зависимости: завершено" : "Dependencies: done"; }
    const char* msgBuildDone() const { return lang == Lang::Ru ? "Сборка: завершено" : "Build: done"; }
    const char* msgBuildFailed() const { return lang == Lang::Ru ? "Сборка: ошибка" : "Build: failed"; }

    const char* msgPressAny() const { return lang == Lang::Ru ? "Нажмите любую клавишу..." : "Press any key..."; }
};

struct Settings {
    std::string buildType = "Release";
    bool enableTests = true;
    bool enableCalls = false;
    bool enablePlugins = true;
    bool enablePanel = true;
};

struct Ansi {
    bool enabled = true;

    const char* reset() const { return enabled ? "\x1b[0m" : ""; }
    const char* bold() const { return enabled ? "\x1b[1m" : ""; }
    const char* dim() const { return enabled ? "\x1b[2m" : ""; }

    const char* fgCyan() const { return enabled ? "\x1b[36m" : ""; }
    const char* fgGreen() const { return enabled ? "\x1b[32m" : ""; }
    const char* fgYellow() const { return enabled ? "\x1b[33m" : ""; }
    const char* fgRed() const { return enabled ? "\x1b[31m" : ""; }
    const char* fgGray() const { return enabled ? "\x1b[90m" : ""; }

    const char* inv() const { return enabled ? "\x1b[7m" : ""; }

    const char* clear() const { return enabled ? "\x1b[2J\x1b[H" : ""; }
    const char* hideCursor() const { return enabled ? "\x1b[?25l" : ""; }
    const char* showCursor() const { return enabled ? "\x1b[?25h" : ""; }
};

enum class Key {
    None,
    Up,
    Down,
    Left,
    Right,
    Enter,
    Space,
    Esc,
    Quit,
    Build,
    Update,
    Deps,
};

#ifdef _WIN32

bool enableVirtualTerminal() {
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
        return false;
    }
    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode)) {
        return false;
    }
    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    return SetConsoleMode(hOut, mode) != 0;
}

bool isTtyStdout() {
    DWORD mode = 0;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
        return false;
    }
    return GetConsoleMode(hOut, &mode) != 0;
}

struct WinConsoleRaw {
    HANDLE hIn = nullptr;
    DWORD oldMode = 0;
    bool ok = false;

    WinConsoleRaw() {
        hIn = GetStdHandle(STD_INPUT_HANDLE);
        if (hIn == INVALID_HANDLE_VALUE || hIn == nullptr) {
            return;
        }
        if (!GetConsoleMode(hIn, &oldMode)) {
            return;
        }
        DWORD newMode = oldMode;
        newMode &= ~ENABLE_ECHO_INPUT;
        newMode &= ~ENABLE_LINE_INPUT;
        newMode &= ~ENABLE_PROCESSED_INPUT;
        newMode |= ENABLE_WINDOW_INPUT;
        if (!SetConsoleMode(hIn, newMode)) {
            return;
        }
        ok = true;
    }

    ~WinConsoleRaw() {
        if (ok) {
            SetConsoleMode(hIn, oldMode);
        }
    }
};

Key readKeyWin() {
    HANDLE hIn = GetStdHandle(STD_INPUT_HANDLE);
    if (hIn == INVALID_HANDLE_VALUE || hIn == nullptr) {
        return Key::None;
    }

    while (true) {
        INPUT_RECORD rec{};
        DWORD read = 0;
        if (!ReadConsoleInputW(hIn, &rec, 1, &read) || read == 0) {
            return Key::None;
        }
        if (rec.EventType != KEY_EVENT) {
            continue;
        }
        const KEY_EVENT_RECORD& k = rec.Event.KeyEvent;
        if (!k.bKeyDown) {
            continue;
        }

        switch (k.wVirtualKeyCode) {
        case VK_UP:
            return Key::Up;
        case VK_DOWN:
            return Key::Down;
        case VK_LEFT:
            return Key::Left;
        case VK_RIGHT:
            return Key::Right;
        case VK_RETURN:
            return Key::Enter;
        case VK_ESCAPE:
            return Key::Esc;
        case VK_SPACE:
            return Key::Space;
        default:
            break;
        }

        const wchar_t c = k.uChar.UnicodeChar;
        if (c == L'q' || c == L'Q') {
            return Key::Quit;
        }
        if (c == L'b' || c == L'B') {
            return Key::Build;
        }
        if (c == L'u' || c == L'U') {
            return Key::Update;
        }
        if (c == L'd' || c == L'D') {
            return Key::Deps;
        }

        // Ctrl+ key combos (best-effort)
        const DWORD ctrl = k.dwControlKeyState;
        const bool ctrlDown = (ctrl & (LEFT_CTRL_PRESSED | RIGHT_CTRL_PRESSED)) != 0;
        if (ctrlDown) {
            if (c == 0) {
                // fallthrough
            } else {
                if (c == L'u' || c == L'U') {
                    return Key::Update;
                }
                if (c == L'b' || c == L'B') {
                    return Key::Build;
                }
            }
        }
    }
}

TermSize termSizeWin() {
    TermSize s;
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
        return s;
    }
    CONSOLE_SCREEN_BUFFER_INFO info{};
    if (!GetConsoleScreenBufferInfo(hOut, &info)) {
        return s;
    }
    s.cols = std::max(40, static_cast<int>(info.srWindow.Right - info.srWindow.Left + 1));
    s.rows = std::max(12, static_cast<int>(info.srWindow.Bottom - info.srWindow.Top + 1));
    return s;
}

#else

bool isTtyStdout() {
    return ::isatty(STDOUT_FILENO) != 0;
}

struct UnixRawMode {
    termios old{};
    bool ok = false;

    UnixRawMode() {
        if (tcgetattr(STDIN_FILENO, &old) != 0) {
            return;
        }
        termios raw = old;
        raw.c_lflag &= static_cast<tcflag_t>(~(ECHO | ICANON));
        raw.c_iflag &= static_cast<tcflag_t>(~(IXON | ICRNL));
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        if (tcsetattr(STDIN_FILENO, TCSANOW, &raw) != 0) {
            return;
        }
        ok = true;
    }

    ~UnixRawMode() {
        if (ok) {
            tcsetattr(STDIN_FILENO, TCSANOW, &old);
        }
    }
};

Key readKeyUnix() {
    unsigned char c = 0;
    const auto n = ::read(STDIN_FILENO, &c, 1);
    if (n != 1) {
        return Key::None;
    }

    if (c == 0x1B) { // ESC
        unsigned char seq[2] = {0, 0};
        if (::read(STDIN_FILENO, &seq[0], 1) != 1) {
            return Key::Esc;
        }
        if (::read(STDIN_FILENO, &seq[1], 1) != 1) {
            return Key::Esc;
        }
        if (seq[0] == '[') {
            switch (seq[1]) {
            case 'A':
                return Key::Up;
            case 'B':
                return Key::Down;
            case 'C':
                return Key::Right;
            case 'D':
                return Key::Left;
            default:
                return Key::None;
            }
        }
        return Key::None;
    }

    if (c == '\n' || c == '\r') {
        return Key::Enter;
    }
    if (c == ' ') {
        return Key::Space;
    }

    if (c == 'q' || c == 'Q') {
        return Key::Quit;
    }
    if (c == 'b' || c == 'B') {
        return Key::Build;
    }
    if (c == 'u' || c == 'U') {
        return Key::Update;
    }
    if (c == 'd' || c == 'D') {
        return Key::Deps;
    }

    return Key::None;
}

TermSize termSizeUnix() {
    TermSize s;
    winsize w{};
    if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0) {
        if (w.ws_col > 0) {
            s.cols = std::max(40, static_cast<int>(w.ws_col));
        }
        if (w.ws_row > 0) {
            s.rows = std::max(12, static_cast<int>(w.ws_row));
        }
    }
    return s;
}

#endif

TermSize termSize() {
#ifdef _WIN32
    return termSizeWin();
#else
    return termSizeUnix();
#endif
}

Key readKey() {
#ifdef _WIN32
    return readKeyWin();
#else
    return readKeyUnix();
#endif
}

std::string readTextFilePrefix(const fs::path& p, std::size_t maxBytes) {
    std::ifstream in(p, std::ios::binary);
    if (!in.is_open()) {
        return {};
    }
    std::string s;
    s.resize(maxBytes);
    in.read(&s[0], static_cast<std::streamsize>(maxBytes));
    s.resize(static_cast<std::size_t>(in.gcount()));
    return s;
}

std::string detectVersionFromCMake(const fs::path& root) {
    const auto cmake = root / "CMakeLists.txt";
    const auto text = readTextFilePrefix(cmake, 16 * 1024);
    if (text.empty()) {
        return {};
    }
    std::regex re(R"(project\(\s*zibby-service\s+VERSION\s+([0-9]+(?:\.[0-9]+){1,3}))", std::regex::icase);
    std::smatch m;
    if (std::regex_search(text, m, re) && m.size() >= 2) {
        return m[1].str();
    }
    return {};
}

Lang detectLang() {
    const char* lang = std::getenv("LANG");
    if (lang != nullptr) {
        std::string v(lang);
        std::transform(v.begin(), v.end(), v.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
        if (v.rfind("ru", 0) == 0 || v.find("_ru") != std::string::npos || v.find("ru_") != std::string::npos) {
            return Lang::Ru;
        }
    }
#ifdef _WIN32
    const LANGID lid = GetUserDefaultUILanguage();
    const WORD primary = PRIMARYLANGID(lid);
    if (primary == LANG_RUSSIAN) {
        return Lang::Ru;
    }
#endif
    return Lang::En;
}

std::string shellQuote(const std::string& s) {
#ifdef _WIN32
    // cmd.exe quoting rules are messy; for our controlled arguments, basic "..." is sufficient.
    std::string out = "\"";
    for (const char c : s) {
        if (c == '"') {
            out += "\\\"";
        } else {
            out.push_back(c);
        }
    }
    out += "\"";
    return out;
#else
    std::string out = "'";
    for (const char c : s) {
        if (c == '\'') {
            out += "'\\''";
        } else {
            out.push_back(c);
        }
    }
    out += "'";
    return out;
#endif
}

std::string stderrToNull() {
#ifdef _WIN32
    return "2>nul";
#else
    return "2>/dev/null";
#endif
}

std::string execRead(const std::string& cmd) {
#ifdef _WIN32
    FILE* pipe = _popen(cmd.c_str(), "r");
#else
    FILE* pipe = popen(cmd.c_str(), "r");
#endif
    if (!pipe) {
        return {};
    }
    std::string out;
    std::array<char, 4096> buf{};
    while (true) {
        const std::size_t n = std::fread(buf.data(), 1, buf.size(), pipe);
        if (n == 0) {
            break;
        }
        out.append(buf.data(), n);
    }
#ifdef _WIN32
    _pclose(pipe);
#else
    pclose(pipe);
#endif
    return out;
}

int runSystem(const std::string& cmd) {
    return std::system(cmd.c_str());
}

bool commandExists(const std::string& name) {
#ifdef _WIN32
    const std::string cmd = "where " + name + " >nul 2>nul";
#else
    const std::string cmd = "command -v " + name + " >/dev/null 2>&1";
#endif
    return runSystem(cmd) == 0;
}

struct GitUpdateStatus {
    bool ok = false;
    bool updateAvailable = false;
    std::string error;
};

GitUpdateStatus gitCheckUpdates(const fs::path& root) {
    GitUpdateStatus st;

    if (!fs::exists(root / ".git")) {
        st.error = "not_git";
        return st;
    }
    if (!commandExists("git")) {
        st.error = "git_missing";
        return st;
    }

    // Fetch (best-effort). Even if fetch fails, we can still attempt status.
    (void)runSystem("git -C " + shellQuote(root.string()) + " fetch --tags --quiet");

    // Verify upstream exists
    const auto upstream = execRead(
        "git -C " + shellQuote(root.string()) + " rev-parse --abbrev-ref --symbolic-full-name @{u} " + stderrToNull());
    if (upstream.empty()) {
        st.error = "no_upstream";
        return st;
    }

    // Count commits we are behind.
    const auto behind = execRead(
        "git -C " + shellQuote(root.string()) + " rev-list --count HEAD..@{u} " + stderrToNull());
    const auto ahead = execRead(
        "git -C " + shellQuote(root.string()) + " rev-list --count @{u}..HEAD " + stderrToNull());

    auto toInt = [](const std::string& s) -> int {
        try {
            return std::stoi(s);
        } catch (...) {
            return 0;
        }
    };

    st.ok = true;
    st.updateAvailable = toInt(behind) > 0;

    // If we are ahead (local commits), still allow check but keep updateAvailable based on behind.
    (void)ahead;
    return st;
}

#ifdef _WIN32
bool spawnProcessDetached(const fs::path& exe, const std::vector<std::string>& args);
#endif

bool maybeSelfRebuildAndRestart(const fs::path& root, const Ansi& ansi) {
    // Best-effort: rebuild & restart after updating sources.
    // This is intended for the typical bootstrap build layout: build/bootstrap.
    (void)root;
    fs::path exePath;
#ifdef _WIN32
    (void)ansi;
    wchar_t exeBuf[MAX_PATH] = {0};
    if (GetModuleFileNameW(nullptr, exeBuf, MAX_PATH) > 0) {
        exePath = fs::path(exeBuf);
    }
#else
    // /proc/self/exe is more reliable than argv[0]
    char buf[4096] = {0};
    const auto n = ::readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n > 0) {
        buf[n] = '\0';
        exePath = fs::path(buf);
    }
#endif

    if (exePath.empty()) {
        return false;
    }

    const fs::path bootstrapDir = exePath.parent_path();
    if (!fs::exists(bootstrapDir / "CMakeCache.txt")) {
        return false;
    }

    (void)runSystem("cmake --build " + shellQuote(bootstrapDir.string()) + " --target zibby-build-tui");

#ifdef _WIN32
    const fs::path newExe = bootstrapDir / "zibby-build-tui.exe";
    if (!fs::exists(newExe)) {
        return false;
    }
    return spawnProcessDetached(newExe, {});
#else
    const fs::path newExe = bootstrapDir / "zibby-build-tui";
    if (!fs::exists(newExe)) {
        return false;
    }
    std::cout << ansi.showCursor() << ansi.reset();
    std::cout.flush();
    execl(newExe.c_str(), newExe.c_str(), nullptr);
    return true;
#endif
}

bool gitPullFastForward(const fs::path& root) {
    if (!commandExists("git")) {
        return false;
    }
    const int code = runSystem("git -C " + shellQuote(root.string()) + " pull --ff-only");
    return code == 0;
}

struct ScopeCursor {
    Ansi ansi;
    explicit ScopeCursor(const Ansi& a) : ansi(a) {
        std::cout << ansi.hideCursor();
        std::cout.flush();
    }
    ~ScopeCursor() {
        std::cout << ansi.showCursor() << ansi.reset();
        std::cout.flush();
    }
};

struct Screen {
    Ansi ansi;
    void clear() {
        if (ansi.enabled) {
            std::cout << ansi.clear();
        } else {
#ifdef _WIN32
            std::system("cls");
#else
            std::system("clear");
#endif
        }
    }
};

std::string boolBadge(bool v, const Ansi& a) {
    if (v) {
        return std::string(a.fgGreen()) + "[ON]" + a.reset();
    }
    return std::string(a.fgGray()) + "[OFF]" + a.reset();
}

std::string valueBadge(const std::string& v, const Ansi& a) {
    return std::string(a.fgCyan()) + "[" + v + "]" + a.reset();
}

struct UiModel {
    Settings settings;
    std::string status;
    bool statusIsError = false;
    bool updatedRepo = false;
};

void draw(const TermSize& ts, const Strings& s, const Ansi& a, const std::string& version, const UiModel& m, int selected) {
    (void)ts;

    auto line = [&](const std::string& text) {
        std::cout << text << "\n";
    };

    const std::string v = version.empty() ? "" : ("v" + version);

    line(std::string(a.bold()) + s.title() + (v.empty() ? "" : (std::string(" ") + a.fgGray() + v + a.reset())) + a.reset());
    line(std::string(a.dim()) + s.subtitle() + a.reset());
    line(std::string(a.dim()) + s.hintKeys() + a.reset());
    line("");

    struct Row {
        std::string label;
        std::string value;
        bool isAction;
    };

    const std::vector<Row> rows = {
        {s.itemBuildType(), valueBadge(m.settings.buildType, a), false},
        {s.itemTests(), boolBadge(m.settings.enableTests, a), false},
        {s.itemCalls(), boolBadge(m.settings.enableCalls, a), false},
        {s.itemPlugins(), boolBadge(m.settings.enablePlugins, a), false},
        {s.itemPanel(), boolBadge(m.settings.enablePanel, a), false},
        {"", "", false},
        {s.actionBuild(), "", true},
        {s.actionUpdate(), "", true},
        {s.actionDeps(), "", true},
        {s.actionExit(), "", true},
    };

    for (int i = 0; i < static_cast<int>(rows.size()); ++i) {
        const auto& r = rows[i];
        if (r.label.empty() && r.value.empty()) {
            line(std::string(a.fgGray()) + "────────────────────────────────────────" + a.reset());
            continue;
        }

        const bool sel = i == selected;
        if (sel) {
            std::cout << a.inv();
        }

        std::ostringstream row;
        row << "  " << r.label;
        if (!r.value.empty()) {
            row << " " << r.value;
        }

        if (r.isAction) {
            row << std::string("  ") + a.fgGray() + "(Enter)" + a.reset();
        } else {
            row << std::string("  ") + a.fgGray() + "(Space)" + a.reset();
        }

        std::cout << row.str();
        if (sel) {
            std::cout << a.reset();
        }
        std::cout << "\n";
    }

    line("");
    if (!m.status.empty()) {
        if (m.statusIsError) {
            line(std::string(a.fgRed()) + m.status + a.reset());
        } else {
            line(std::string(a.fgYellow()) + m.status + a.reset());
        }
    } else {
        line(std::string(a.fgGray()) + s.statusReady() + a.reset());
    }
}

fs::path findRepoRoot(fs::path start) {
    start = fs::absolute(start);
    fs::path cur = start;

    for (int i = 0; i < 8; ++i) {
        if (fs::exists(cur / "CMakeLists.txt")) {
            return cur;
        }
        if (cur.has_parent_path()) {
            cur = cur.parent_path();
        } else {
            break;
        }
    }
    return start;
}

std::string generatorArgs() {
    // Prefer Ninja if available.
    if (commandExists("ninja")) {
        return "-G Ninja ";
    }
    return {};
}

std::string vcpkgToolchainArg() {
#ifdef _WIN32
    const char* root = std::getenv("VCPKG_ROOT");
    if (root == nullptr || *root == '\0') {
        root = std::getenv("VCPKG_INSTALLATION_ROOT");
    }
    if (root == nullptr || *root == '\0') {
        return {};
    }
    fs::path tc = fs::path(root) / "scripts" / "buildsystems" / "vcpkg.cmake";
    if (!fs::exists(tc)) {
        return {};
    }
    return "-DCMAKE_TOOLCHAIN_FILE=" + shellQuote(tc.string()) + " ";
#else
    return {};
#endif
}

bool runConfigureBuildPack(const fs::path& root, const Settings& st, std::string* outError) {
    try {
        const fs::path buildDir = root / "build";
        fs::create_directories(buildDir);

        const std::string tests = st.enableTests ? "ON" : "OFF";
        const std::string calls = st.enableCalls ? "ON" : "OFF";
        const std::string plugins = st.enablePlugins ? "ON" : "OFF";
        const std::string panel = st.enablePanel ? "ON" : "OFF";

        std::ostringstream cfg;
        cfg << "cmake -S " << shellQuote(root.string()) << " -B " << shellQuote(buildDir.string()) << " "
            << generatorArgs() << vcpkgToolchainArg()
            << "-DCMAKE_BUILD_TYPE=" << st.buildType << " "
            << "-DZIBBY_ENABLE_TESTS=" << tests << " "
            << "-DZIBBY_ENABLE_CALLS=" << calls << " "
            << "-DZIBBY_ENABLE_PLUGINS=" << plugins << " "
            << "-DZIBBY_ENABLE_PANEL=" << panel;

        if (runSystem(cfg.str()) != 0) {
            if (outError) {
                *outError = "cmake configure failed";
            }
            return false;
        }

        std::ostringstream bld;
        bld << "cmake --build " << shellQuote(buildDir.string()) << " --config " << st.buildType;
        if (runSystem(bld.str()) != 0) {
            if (outError) {
                *outError = "cmake build failed";
            }
            return false;
        }

        if (st.enableTests) {
            std::ostringstream tst;
            tst << "ctest --test-dir " << shellQuote(buildDir.string()) << " -C " << st.buildType << " --output-on-failure";
            if (runSystem(tst.str()) != 0) {
                if (outError) {
                    *outError = "ctest failed";
                }
                return false;
            }
        }

        std::ostringstream pack;
        pack << "cpack --config " << shellQuote((buildDir / "CPackConfig.cmake").string()) << " -C " << st.buildType;
        if (runSystem(pack.str()) != 0) {
            if (outError) {
                *outError = "cpack failed";
            }
            return false;
        }

#ifdef _WIN32
        const fs::path gen = root / "scripts" / "generate_checksums.ps1";
        if (fs::exists(gen)) {
            std::ostringstream sum;
            sum << "powershell -ExecutionPolicy Bypass -File " << shellQuote(gen.string())
                << " -ArtifactsDir " << shellQuote((root / "installers").string());
            (void)runSystem(sum.str());
        }
#else
        const fs::path gen = root / "scripts" / "generate_checksums.sh";
        if (fs::exists(gen)) {
            std::ostringstream sum;
            sum << "bash " << shellQuote(gen.string()) << " " << shellQuote((root / "installers").string());
            (void)runSystem(sum.str());
        }
#endif

        return true;
    } catch (const std::exception& ex) {
        if (outError) {
            *outError = ex.what();
        }
        return false;
    }
}

bool runInstallDeps(const fs::path& root) {
#ifdef _WIN32
    const fs::path deps = root / "scripts" / "install_deps_windows.ps1";
    if (!fs::exists(deps)) {
        return false;
    }
    std::ostringstream cmd;
    cmd << "powershell -ExecutionPolicy Bypass -File " << shellQuote(deps.string());
    return runSystem(cmd.str()) == 0;
#else
    const fs::path deps = root / "scripts" / "install_deps_linux.sh";
    if (!fs::exists(deps)) {
        return false;
    }
    std::ostringstream cmd;
    cmd << "bash " << shellQuote(deps.string());
    return runSystem(cmd.str()) == 0;
#endif
}

#ifdef _WIN32
bool spawnProcessDetached(const fs::path& exe, const std::vector<std::string>& args) {
    std::wstring cmd;
    cmd += L"\"" + exe.wstring() + L"\"";
    for (const auto& a : args) {
        cmd += L" \"";
        std::wstring wa(a.begin(), a.end());
        cmd += wa;
        cmd += L"\"";
    }

    STARTUPINFOW si{};
    si.cb = sizeof(si);
    PROCESS_INFORMATION pi{};
    std::wstring cmdMutable = cmd;

    const BOOL ok = CreateProcessW(
        nullptr,
        cmdMutable.data(),
        nullptr,
        nullptr,
        FALSE,
        0,
        nullptr,
        nullptr,
        &si,
        &pi);

    if (!ok) {
        return false;
    }

    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return true;
}
#endif

} // namespace

int main(int argc, char** argv) {
    (void)argc;
    (void)argv;
    const fs::path start = fs::current_path();
    const fs::path root = findRepoRoot(start);

#ifdef _WIN32
    (void)enableVirtualTerminal();
#endif

    Ansi ansi;
    ansi.enabled = isTtyStdout();

    Strings strings;
    strings.lang = detectLang();

    UiModel model;
    model.status = "";

    const std::string version = detectVersionFromCMake(root);

#ifdef _WIN32
    WinConsoleRaw raw;
#else
    UnixRawMode raw;
#endif

    Screen screen;
    screen.ansi = ansi;

    ScopeCursor cursor(ansi);

    int selected = 0;
    const int maxIndex = 9;

    auto setStatus = [&](std::string s, bool err = false) {
        model.status = std::move(s);
        model.statusIsError = err;
    };

    while (true) {
        screen.clear();
        draw(termSize(), strings, ansi, version, model, selected);
        std::cout.flush();

        const Key k = readKey();
        if (k == Key::Up) {
            selected = std::max(0, selected - 1);
            continue;
        }
        if (k == Key::Down) {
            selected = std::min(maxIndex, selected + 1);
            continue;
        }
        if (k == Key::Quit || k == Key::Esc) {
            break;
        }

        auto toggleSettingAt = [&]() {
            switch (selected) {
            case 0:
                model.settings.buildType = (model.settings.buildType == "Debug") ? "Release" : "Debug";
                break;
            case 1:
                model.settings.enableTests = !model.settings.enableTests;
                break;
            case 2:
                model.settings.enableCalls = !model.settings.enableCalls;
                break;
            case 3:
                model.settings.enablePlugins = !model.settings.enablePlugins;
                break;
            case 4:
                model.settings.enablePanel = !model.settings.enablePanel;
                break;
            default:
                break;
            }
        };

        auto runUpdateFlow = [&]() {
            setStatus(strings.statusRunning());
            screen.clear();
            draw(termSize(), strings, ansi, version, model, selected);
            std::cout.flush();

            const auto st = gitCheckUpdates(root);
            if (!st.ok) {
                if (st.error == "not_git") {
                    setStatus(strings.msgNotGitRepo(), true);
                } else if (st.error == "git_missing") {
                    setStatus(strings.msgGitNotFound(), true);
                } else if (st.error == "no_upstream") {
                    setStatus(strings.msgNoUpstream(), true);
                } else {
                    setStatus("git: error", true);
                }
                return;
            }

            if (!st.updateAvailable) {
                setStatus(strings.msgUpToDate());
                return;
            }

            // Confirmation screen
            setStatus(std::string(strings.msgUpdateAvailable()) + ". " + strings.msgConfirmPull());
            screen.clear();
            draw(termSize(), strings, ansi, version, model, selected);
            std::cout.flush();

            while (true) {
                const Key kk = readKey();
                if (kk == Key::Enter) {
                    break;
                }
                if (kk == Key::Esc || kk == Key::Quit) {
                    setStatus(strings.statusReady());
                    return;
                }
            }

            setStatus(strings.msgPulling());
            screen.clear();
            draw(termSize(), strings, ansi, version, model, selected);
            std::cout.flush();

            if (!gitPullFastForward(root)) {
                setStatus("git pull failed", true);
                return;
            }

            model.updatedRepo = true;
            setStatus(strings.msgPulled());
        };

        auto runDepsFlow = [&]() {
            setStatus(strings.statusRunning());
            screen.clear();
            draw(termSize(), strings, ansi, version, model, selected);
            std::cout.flush();

            const bool ok = runInstallDeps(root);
            setStatus(ok ? strings.msgDepsDone() : "deps failed", !ok);
        };

        auto runBuildFlow = [&]() {
            setStatus(strings.statusRunning());
            screen.clear();
            draw(termSize(), strings, ansi, version, model, selected);
            std::cout.flush();

            std::string err;
            const bool ok = runConfigureBuildPack(root, model.settings, &err);
            if (ok) {
                setStatus(strings.msgBuildDone());
            } else {
                setStatus(std::string(strings.msgBuildFailed()) + (err.empty() ? "" : (": " + err)), true);
            }
        };

        if (k == Key::Space) {
            toggleSettingAt();
            continue;
        }

        if (k == Key::Build) {
            runBuildFlow();
            continue;
        }
        if (k == Key::Update) {
            runUpdateFlow();
            if (model.updatedRepo) {
                (void)maybeSelfRebuildAndRestart(root, ansi);
                return 0;
            }
            continue;
        }
        if (k == Key::Deps) {
            runDepsFlow();
            continue;
        }

        if (k == Key::Enter) {
            if (selected >= 0 && selected <= 4) {
                toggleSettingAt();
                continue;
            }
            if (selected == 6) {
                runBuildFlow();
                continue;
            }
            if (selected == 7) {
                runUpdateFlow();
                if (model.updatedRepo) {
                    (void)maybeSelfRebuildAndRestart(root, ansi);
                    return 0;
                }
                continue;
            }
            if (selected == 8) {
                runDepsFlow();
                continue;
            }
            if (selected == 9) {
                break;
            }
        }

    }

    screen.clear();
    return 0;
}
