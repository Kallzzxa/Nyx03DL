// download.h — background HTTP downloader for Nyx03DL.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/model/download.h), GPL-3.0.
// Same shape as the reference (WinINet Fetch, single detached worker thread,
// queue + Drain), with two changes:
//  * Result carries `skinid` as well as `crc`/`isDff`, so the monitor can
//    attribute a finished download to its model even if two files ever
//    share a CRC.
//  * The worker fetches to "<dest>.part" and only renames it into place
//    after the CRC check passes, so a failed/interrupted download can never
//    leave a corrupt file at the cache path (the reference wrote straight
//    to the destination).
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nyx {
namespace dl {

// Get `url` into `destPath`. Returns true if any bytes were written. Blocking.
// Timeouts are short-ish (15 s) so a stalled file server cannot wedge the
// worker forever; the monitor simply re-enqueues on the next probe cycle.
bool Fetch(const std::string& url, const std::string& destPath);

// A finished async download + CRC verification.
struct Result {
    int skinid = 0;     // model this file belongs to
    uint32_t crc = 0;   // expected checksum
    bool isDff = false; // echoed back from the enqueue call
    bool ok = false;    // fetched AND crc-verified against `crc`
};

// Queue an async download; the worker fetches, checks crc32 == crc, and
// posts a Result to Drain. Downloads run serially, in enqueue order.
void Enqueue(int skinid, const std::string& url, const std::string& destPath,
             uint32_t crc, bool isDff);

// Pop all finished results (thread-safe). Call from the monitor thread.
std::vector<Result> Drain();

}  // namespace dl
}  // namespace nyx
