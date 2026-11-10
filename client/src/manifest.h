// manifest.h — hand-rolled minimal JSON parser for the Nyx03DL manifest.
//
// Written for Nyx03DL (M2). No external JSON library is used on purpose:
// the schema is fixed and tiny, and the .asi must stay dependency-free.
//
// Manifest schema (from the M1 plugin, GET /nyx03dl/manifest):
//   [ {"skinid":20000, "baseid":264, "dff":"skin_a.dff", "txd":"skin_a.txd",
//      "dff_crc32":688229491, "txd_crc32":3748153194,
//      "dff_size":256, "txd_size":90}, ... ]
//
// CONTRACT: CRC32 values are UNSIGNED DECIMAL numbers, NOT hex. They are
// parsed with base 10 (strtoull-style); a hex-looking value such as
// "0x29058C73" or a quoted CRC is rejected. Numbers must fit in uint32
// (0..4294967295). Malformed JSON is rejected with an error string — the
// parser never crashes on bad input.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace nyx {

// One model entry from the manifest. CRCs/sizes are the unsigned decimal
// values exactly as sent by the server.
struct ManifestModel {
    int skinid = 0;   // 20000..30000
    int baseid = 0;   // 0..311
    std::string dff;  // file name as registered on the server, e.g. "skin_a.dff"
    std::string txd;  // e.g. "skin_a.txd"
    uint32_t dffCrc = 0;
    uint32_t txdCrc = 0;
    uint32_t dffSize = 0;
    uint32_t txdSize = 0;
};

// Parse a manifest JSON document. Returns false (and fills `err` with a
// human-readable reason) on any malformed input. `out` is left untouched
// on failure and fully replaced on success.
bool ParseManifest(const std::string& json, std::vector<ManifestModel>& out,
                   std::string& err);

}  // namespace nyx
