// cache.cpp — downloaded-model cache for Nyx03DL.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/model/cache.cpp), GPL-3.0.
// Changes: single shared dir ...\SAMP\cache\nyx03dl\ (see cache.h), no
// logging dependency, portable directory creation for the Linux harness.
#include "cache.h"

#include "crc32.h"

#include <cstdio>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#else
#include <sys/stat.h>
#include <sys/types.h>
#endif

namespace nyx {
namespace cache {
namespace {

std::string g_dir;
std::string g_override;

// Create every component of a path (like mkdir -p), backslash-separated.
void MakeDirs(const std::string& path) {
#ifdef _WIN32
    for (size_t i = 0; i < path.size(); ++i) {
        if (path[i] == '\\' || path[i] == '/') {
            if (i > 0) CreateDirectoryA(path.substr(0, i).c_str(), nullptr);
        }
    }
    CreateDirectoryA(path.c_str(), nullptr);
#else
    // Linux (unit harness only): create components one by one.
    std::string cur;
    for (size_t i = 0; i < path.size(); ++i) {
        cur += path[i];
        if (path[i] == '/') mkdir(cur.c_str(), 0755);
    }
    if (!cur.empty() && cur.back() != '/') mkdir(cur.c_str(), 0755);
#endif
}

}  // namespace

void SetDirOverride(const std::string& dir) {
    g_override = dir;
    g_dir.clear();
}

const std::string& Dir() {
    if (!g_dir.empty()) return g_dir;

    if (!g_override.empty()) {
        MakeDirs(g_override);
        g_dir = g_override;
        return g_dir;
    }

#ifdef _WIN32
    char docs[MAX_PATH] = {0};
    if (SUCCEEDED(SHGetFolderPathA(nullptr, CSIDL_PERSONAL, nullptr, 0, docs))) {
        std::string base =
            std::string(docs) + "\\GTA San Andreas User Files\\SAMP\\cache\\nyx03dl";
        MakeDirs(base);
        g_dir = base + "\\";
    }
#else
    std::string base = "./nyx03dl-cache-test";
    MakeDirs(base);
    g_dir = base + "/";
#endif
    return g_dir;
}

std::string FileNameFor(uint32_t checksum, bool isDff) {
    char name[32];
    std::snprintf(name, sizeof(name), "%08X.%s", checksum, isDff ? "dff" : "txd");
    return std::string(name);
}

std::string PathFor(uint32_t checksum, bool isDff) {
    return Dir() + FileNameFor(checksum, isDff);
}

bool HasValid(uint32_t checksum, bool isDff) {
    if (checksum == 0) return false;
    if (Dir().empty()) return false;
    std::string path = PathFor(checksum, isDff);
    return crc::File(path.c_str()) == checksum;
}

}  // namespace cache
}  // namespace nyx
