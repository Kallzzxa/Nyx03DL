// crc32.cpp — CRC-32/IEEE implementation.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/model/crc32.cpp), GPL-3.0.
// Changes: namespace nyx::crc, std::call_once for the table (the original
// built it lazily behind a plain bool, which is a data race when the monitor
// thread and the download worker both hash files).
#include "crc32.h"

#include <cstdio>
#include <mutex>

namespace nyx {
namespace crc {
namespace {

uint32_t g_table[256];

void BuildTable() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        g_table[i] = c;
    }
}

std::once_flag g_tableOnce;

void EnsureTable() { std::call_once(g_tableOnce, BuildTable); }

inline uint32_t Update(uint32_t crc, const uint8_t* p, size_t n) {
    for (size_t i = 0; i < n; ++i)
        crc = g_table[(crc ^ p[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

}  // namespace

uint32_t Buffer(const void* data, size_t len) {
    EnsureTable();
    return Update(0xFFFFFFFFu, static_cast<const uint8_t*>(data), len) ^ 0xFFFFFFFFu;
}

uint32_t File(const char* path) {
    EnsureTable();
    FILE* f = std::fopen(path, "rb");
    if (!f) return 0;
    uint32_t crc = 0xFFFFFFFFu;
    unsigned char buf[65536];
    size_t r;
    while ((r = std::fread(buf, 1, sizeof(buf), f)) > 0)
        crc = Update(crc, buf, r);
    std::fclose(f);
    return crc ^ 0xFFFFFFFFu;
}

}  // namespace crc
}  // namespace nyx
