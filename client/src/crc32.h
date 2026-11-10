// crc32.h — CRC-32/IEEE for Nyx03DL.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/model/crc32.h), GPL-3.0.
// Changes: namespace nyx::crc (was crc), table built with std::call_once so
// concurrent calls from the monitor thread and the download worker are safe.
//
// Polynomial 0xEDB88320 (reflected), init 0xFFFFFFFF, final xor 0xFFFFFFFF.
// Test vector: CRC32("123456789") == 0xCBF43926.
#pragma once

#include <cstddef>
#include <cstdint>

namespace nyx {
namespace crc {

uint32_t Buffer(const void* data, size_t len);
uint32_t File(const char* path);  // 0 if the file can't be opened

}  // namespace crc
}  // namespace nyx
