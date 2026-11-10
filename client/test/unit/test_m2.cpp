// test_m2.cpp — Linux unit harness for the portable Nyx03DL M2 units.
//
// Compiles the Windows-independent parts natively on Linux (no WinINet,
// no samp.dll, no RakNet):
//   g++ -std=c++17 -O2 -Wall -Wextra -Isrc test/unit/test_m2.cpp
//       src/crc32.cpp src/manifest.cpp src/cache.cpp src/log.cpp
//       -o /tmp/test_m2 && /tmp/test_m2
// Run from client/ (it reads test/fixtures/ relative to the CWD).
//
// Checks (each prints PASS/FAIL, exit code 0 only if all pass):
//   1. CRC-32/IEEE test vector: CRC32("123456789") == 0xCBF43926.
//   2. CRC32 of the fixture files matches independent reference values
//      (Python binascii.crc32, computed outside this C++ code).
//   3. Manifest parser: the example manifest.json (real decimal CRCs)
//      parses to the exact expected models; string escapes work.
//   4. Manifest parser rejects: truncated JSON, hex CRC, negative
//      number, CRC > uint32, missing field, unknown key, trailing
//      garbage, traversal file name, bad escape, skinid out of range.
//   5. Cache: file naming "<CRC hex uppercase>.<dff|txd>", HasValid
//      true for a correctly-named copy of a fixture, false for a
//      corrupted copy / missing file / zero CRC.

#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "cache.h"
#include "crc32.h"
#include "manifest.h"

namespace fs = std::filesystem;

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char* name) {
    checks++;
    if (ok) {
        printf("PASS: %s\n", name);
    } else {
        failures++;
        printf("FAIL: %s\n", name);
    }
}

static std::string ReadFile(const std::string& path) {
    std::ifstream in(path, std::ios::binary);
    std::ostringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

int main() {
    // 1. CRC-32 test vector (CRC-32/IEEE, "123456789" -> 0xCBF43926).
    const char* vec = "123456789";
    uint32_t v = nyx::crc::Buffer(vec, strlen(vec));
    printf("CRC32(\"123456789\") = 0x%08X (expected 0xCBF43926)\n", v);
    check(v == 0xCBF43926u, "CRC32 test vector 123456789 == 0xCBF43926");
    check(nyx::crc::Buffer(nullptr, 0) == 0x00000000u,
          "CRC32 of empty input == 0x00000000");
    check(nyx::crc::File("test/fixtures/no-such-file.dff") == 0,
          "CRC32 of missing file == 0");

    // 2. Fixture CRCs (reference values from Python binascii.crc32).
    check(nyx::crc::File("test/fixtures/skin_a.dff") == 0x29058C73u,
          "fixture skin_a.dff crc 0x29058C73");
    check(nyx::crc::File("test/fixtures/skin_a.txd") == 0xDF68476Au,
          "fixture skin_a.txd crc 0xDF68476A");
    check(nyx::crc::File("test/fixtures/skin_b.dff") == 0xF5E54AD0u,
          "fixture skin_b.dff crc 0xF5E54AD0");
    check(nyx::crc::File("test/fixtures/skin_b.txd") == 0x0F151593u,
          "fixture skin_b.txd crc 0x0F151593");

    // 3. Manifest parser: example manifest.json (decimal CRCs, real values).
    std::string json = ReadFile("test/fixtures/manifest.json");
    check(!json.empty(), "manifest.json fixture readable");
    std::vector<nyx::ManifestModel> models;
    std::string err;
    bool ok = nyx::ParseManifest(json, models, err);
    printf("parse manifest.json: ok=%d err='%s' models=%zu\n", (int)ok, err.c_str(),
           models.size());
    check(ok, "manifest.json parses");
    check(models.size() == 2, "manifest.json has 2 models");
    if (models.size() == 2) {
        check(models[0].skinid == 20000 && models[0].baseid == 264,
              "model 0 skinid 20000 baseid 264");
        check(models[0].dff == "skin_a.dff" && models[0].txd == "skin_a.txd",
              "model 0 file names");
        check(models[0].dffCrc == 0x29058C73u && models[0].txdCrc == 0xDF68476Au,
              "model 0 CRCs are decimal-parsed (0x29058C73 / 0xDF68476A)");
        check(models[0].dffSize == 256 && models[0].txdSize == 90,
              "model 0 sizes 256 / 90");
        check(models[1].skinid == 20001 && models[1].baseid == 265,
              "model 1 skinid 20001 baseid 265");
        check(models[1].dffCrc == 0xF5E54AD0u && models[1].txdCrc == 0x0F151593u,
              "model 1 CRCs are decimal-parsed (0xF5E54AD0 / 0x0F151593)");
        check(models[1].dffSize == 1024 && models[1].txdSize == 80,
              "model 1 sizes 1024 / 80");
    }

    // String escapes: \" and \u0022 inside a file name round-trip.
    // (A decoded backslash \\ in a file name is exercised below: it must
    // be rejected by the path-character check, which proves it decoded.)
    {
        std::string esc =
            "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"a\\\"b.dff\",\"txd\":"
            "\"e\\u0022f.txd\","
            "\"dff_crc32\":688229491,\"txd_crc32\":3748153194,\"dff_size\":256,"
            "\"txd_size\":90}]";
        std::vector<nyx::ManifestModel> m;
        std::string e;
        bool ok = nyx::ParseManifest(esc, m, e);
        printf("escape parse: ok=%d err='%s'\n", (int)ok, e.c_str());
        check(ok && m.size() == 1 && m[0].dff == "a\"b.dff" &&
                  m[0].txd == "e\"f.txd",
              "parser handles \\\" and \\u0022 escapes");
    }
    // Empty array is valid (server with no skins registered).
    {
        std::vector<nyx::ManifestModel> m;
        std::string e;
        check(nyx::ParseManifest("[]", m, e) && m.empty(),
              "empty manifest array parses to zero models");
    }

    // 4. Parser rejections — every one must fail, never crash.
    auto expectReject = [](const char* name, const std::string& bad) {
        std::vector<nyx::ManifestModel> m;
        std::string e;
        bool ok = nyx::ParseManifest(bad, m, e);
        printf("reject %-28s ok=%d err='%s'\n", name, (int)ok, e.c_str());
        check(!ok, name);
    };
    expectReject("reject backslash decoded from \\\\ escape",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"a\\\\b.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}]");
    expectReject("reject truncated JSON",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"skin_a.dff\"");
    expectReject("reject hex CRC",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"skin_a.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":\"0x29058C73\",\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}]");
    expectReject("reject negative number",
                 "[{\"skinid\":-20000,\"baseid\":264,\"dff\":\"skin_a.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}]");
    expectReject("reject CRC above uint32",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"skin_a.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":4294967296,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}]");
    expectReject("reject missing field",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"skin_a.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"dff_size\":256,"
                 "\"txd_size\":90}]");
    expectReject("reject unknown key",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"skin_a.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90,\"extra\":1}]");
    expectReject("reject trailing garbage",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"skin_a.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}] garbage");
    expectReject("reject traversal file name",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"../secret.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}]");
    expectReject("reject bad escape",
                 "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"a\\qb.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}]");
    expectReject("reject skinid 1999",
                 "[{\"skinid\":1999,\"baseid\":264,\"dff\":\"skin_a.dff\",\"txd\":"
                 "\"skin_a.txd\",\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
                 "\"dff_size\":256,\"txd_size\":90}]");
    expectReject("reject empty document", "");
    expectReject("reject top-level object, not array",
                 "{\"skinid\":20000}");

    // 5. Cache naming + HasValid.
    check(nyx::cache::FileNameFor(0xCBF43926u, true) == "CBF43926.dff",
          "cache file name CBF43926.dff");
    check(nyx::cache::FileNameFor(0xCBF43926u, false) == "CBF43926.txd",
          "cache file name CBF43926.txd");
    check(nyx::cache::FileNameFor(0x0F151593u, false) == "0F151593.txd",
          "cache file name zero-padded 0F151593.txd");
    check(!nyx::cache::HasValid(0, true), "HasValid(0) is false");
    check(!nyx::cache::HasValid(0x29058C73u, true),
          "HasValid false when file is absent");

    fs::path tmpDir = fs::current_path() / "test-tmp-cache";
    fs::remove_all(tmpDir);
    nyx::cache::SetDirOverride(tmpDir.string() + "/");
    (void)nyx::cache::Dir();  // force creation of the override dir
    // Correctly-named copy of the fixture -> valid.
    fs::copy_file("test/fixtures/skin_a.dff",
                  tmpDir / "29058C73.dff", fs::copy_options::overwrite_existing);
    check(nyx::cache::HasValid(0x29058C73u, true),
          "HasValid true for correctly-named fixture copy");
    check(nyx::cache::PathFor(0x29058C73u, true) ==
              tmpDir.string() + "/29058C73.dff",
          "PathFor returns override dir + CRC name");
    // Corrupt one byte -> invalid (must be re-downloaded).
    {
        fs::path p = tmpDir / "29058C73.dff";
        std::fstream f(p, std::ios::in | std::ios::out | std::ios::binary);
        char b = 0;
        f.seekp(0);
        f.get(b);
        f.seekp(0);
        f.put(static_cast<char>(b ^ 0xFF));
    }
    check(!nyx::cache::HasValid(0x29058C73u, true),
          "HasValid false after corrupting one byte");
    // Same bytes under the .txd name do not validate as a txd (wrong CRC).
    fs::copy_file("test/fixtures/skin_a.dff",
                  tmpDir / "29058C73.txd", fs::copy_options::overwrite_existing);
    check(!nyx::cache::HasValid(0xDF68476Au, false),
          "HasValid false when .txd slot holds dff bytes (CRC differs)");
    fs::remove_all(tmpDir);

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
