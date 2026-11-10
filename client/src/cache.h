// cache.h — downloaded-model cache for Nyx03DL.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/model/cache.h), GPL-3.0.
// Changes:
//  * Cache directory is a SINGLE content-addressed dir for all servers:
//    Documents/GTA San Andreas User Files/SAMP/cache/nyx03dl
//    (the reference used one dir per plugin, .../cache/customskin037; the
//    open.mp layout ...\cache\<ip_port>\ is deliberately NOT used — a file
//    with the same CRC is the same file no matter which server served it,
//    so it must not be re-downloaded per server).
//  * File names are "<CRC hex, 8 digits, uppercase>.<dff|txd>", exactly as
//    in the reference (e.g. CBF43926.dff).
//  * SetDirOverride() exists only so the Linux unit harness can point the
//    cache at a scratch dir; the .asi never calls it.
#pragma once

#include <cstdint>
#include <string>

namespace nyx {
namespace cache {

// Cache directory (created on first call, trailing separator). Empty on failure.
// On Windows: Documents/GTA San Andreas User Files/SAMP/cache/nyx03dl
// Otherwise (unit harness): the override dir, or ./nyx03dl-cache-test/
const std::string& Dir();

// Test-only: force Dir() to return `dir` (must end with '/' or '\\').
void SetDirOverride(const std::string& dir);

// File name for a checksum, e.g. "CBF43926.dff" / "CBF43926.txd".
std::string FileNameFor(uint32_t checksum, bool isDff);

// Absolute path where a dff/txd with the given checksum is stored.
std::string PathFor(uint32_t checksum, bool isDff);

// True if a cached file for `checksum` exists and its CRC32 matches.
bool HasValid(uint32_t checksum, bool isDff);

}  // namespace cache
}  // namespace nyx
