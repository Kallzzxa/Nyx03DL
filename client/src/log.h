// log.h — thread-safe file logger for Nyx03DL.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/core/log.h), GPL-3.0.
// Changes: log file renamed to nyx03dl.log, namespace nyx::log, and a
// portable (non-Windows) fallback so the same file compiles into the Linux
// unit harness. Writes next to gta_sa.exe on Windows, ./nyx03dl.log elsewhere.
#pragma once

namespace nyx {
namespace log {

// Opens (truncates) the log file. Safe to call once from the monitor thread.
void Init();

// printf-style. Thread-safe; each line is timestamped and tagged with `level`.
void Write(const char* level, const char* fmt, ...);

}  // namespace log
}  // namespace nyx

#define NYX_LOGI(...) ::nyx::log::Write("INFO", __VA_ARGS__)
#define NYX_LOGW(...) ::nyx::log::Write("WARN", __VA_ARGS__)
#define NYX_LOGE(...) ::nyx::log::Write("ERR ", __VA_ARGS__)
#define NYX_LOGD(...) ::nyx::log::Write("DBG ", __VA_ARGS__)
