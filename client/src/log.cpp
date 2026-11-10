// log.cpp — thread-safe file logger for Nyx03DL.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/core/log.cpp), GPL-3.0.
// Changes: file name nyx03dl.log, namespace nyx::log, portable fallback
// (localtime + ./nyx03dl.log) for non-Windows builds (unit harness).
#include "log.h"

#include <cstdio>
#include <cstdarg>
#include <mutex>
#include <string>

#ifdef _WIN32
#include <windows.h>
#else
#include <ctime>
#endif

namespace nyx {
namespace log {
namespace {

std::mutex g_mutex;
std::string g_path;
bool g_ready = false;

// Resolve "<dir of gta_sa.exe>/nyx03dl.log" on Windows, "./nyx03dl.log" elsewhere.
std::string ResolveLogPath() {
#ifdef _WIN32
    char exe[MAX_PATH] = {0};
    DWORD n = GetModuleFileNameA(nullptr, exe, MAX_PATH);
    std::string dir;
    if (n > 0) {
        std::string full(exe, n);
        size_t slash = full.find_last_of("\\/");
        dir = (slash == std::string::npos) ? std::string() : full.substr(0, slash + 1);
    }
    return dir + "nyx03dl.log";
#else
    return "./nyx03dl.log";
#endif
}

}  // namespace

void Init() {
    std::lock_guard<std::mutex> lock(g_mutex);
    g_path = ResolveLogPath();
    if (FILE* f = std::fopen(g_path.c_str(), "w")) {
#ifdef _WIN32
        SYSTEMTIME st;
        GetLocalTime(&st);
        std::fprintf(f, "==== nyx03dl log opened %04d-%02d-%02d %02d:%02d:%02d ====\n",
                     st.wYear, st.wMonth, st.wDay, st.wHour, st.wMinute, st.wSecond);
#else
        std::time_t t = std::time(nullptr);
        std::fprintf(f, "==== nyx03dl log opened %s ====\n", std::ctime(&t));
#endif
        std::fclose(f);
        g_ready = true;
    }
}

void Write(const char* level, const char* fmt, ...) {
    std::lock_guard<std::mutex> lock(g_mutex);
    if (!g_ready) return;

    FILE* f = std::fopen(g_path.c_str(), "a");
    if (!f) return;

#ifdef _WIN32
    SYSTEMTIME st;
    GetLocalTime(&st);
    std::fprintf(f, "[%02d:%02d:%02d.%03d][%lu][%s] ", st.wHour, st.wMinute, st.wSecond,
                 st.wMilliseconds, GetCurrentThreadId(), level ? level : "----");
#else
    std::time_t t = std::time(nullptr);
    struct tm tmv;
    localtime_r(&t, &tmv);
    std::fprintf(f, "[%02d:%02d:%02d][%s] ", tmv.tm_hour, tmv.tm_min, tmv.tm_sec,
                 level ? level : "----");
#endif

    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);

    std::fputc('\n', f);
    std::fclose(f);
}

}  // namespace log
}  // namespace nyx
