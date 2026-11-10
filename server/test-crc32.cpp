// test-crc32.cpp — Linux test harness for the portable Nyx03DL core.
//
// Compiles nyx03dl_core.cpp natively on Linux (no Winsock / sampgdk):
//   g++ -std=c++11 -O2 -I. test-crc32.cpp nyx03dl_core.cpp -o test-crc32
//   ./test-crc32
//
// Checks (each prints PASS/FAIL, exit code 0 only if all pass):
//   1. CRC-32/IEEE test vector: CRC32("123456789") == 0xCBF43926.
//   2. CRC32 of the fixture files matches an independent reference
//      (Python binascii.crc32 values baked in below — computed outside
//      this C++ code, so a shared bug in the table would be caught).
//   3. Registry: register skin 20000 from fixtures, idempotent
//      re-register returns the same baseid, out-of-range skinid (1999)
//      fails, missing file fails, pool exhaustion fails.
//   4. Manifest JSON: exact expected string (CRC as unsigned decimal).
//   5. Players JSON: only connected players are exposed.
//   6. Filename validation: path traversal names rejected.

#include <cstdio>
#include <cstring>
#include <string>

#include "nyx03dl_core.h"

static int failures = 0;
static int checks = 0;

static void check(bool ok, const char *name) {
    checks++;
    if (ok) {
        printf("PASS: %s\n", name);
    } else {
        failures++;
        printf("FAIL: %s\n", name);
    }
}

int main() {
    // 1. CRC-32 test vector (CRC-32/IEEE, "123456789" -> 0xCBF43926).
    const char *vec = "123456789";
    uint32_t v = nyx::crc32_compute(
        reinterpret_cast<const unsigned char *>(vec), strlen(vec));
    printf("CRC32(\"123456789\") = 0x%08X (expected 0xCBF43926)\n", v);
    check(v == 0xCBF43926u, "CRC32 test vector 123456789 == 0xCBF43926");
    check(nyx::crc32_compute(nullptr, 0) == 0x00000000u,
          "CRC32 of empty input == 0x00000000");

    // 2. Fixture CRCs (reference values from Python binascii.crc32).
    //    Fixtures are TEST-ONLY files (see test-fixtures/README.md) —
    //    they are NOT real .dff/.txd models.
    uint32_t crc = 0, size = 0;
    bool ok = nyx::file_crc32_and_size("test-fixtures/skin_a.dff", crc, size);
    printf("skin_a.dff: crc=0x%08X (%u) size=%u\n", crc, crc, size);
    check(ok && crc == 0x29058C73u && size == 256,
          "fixture skin_a.dff crc 0x29058C73 size 256");
    ok = nyx::file_crc32_and_size("test-fixtures/skin_a.txd", crc, size);
    printf("skin_a.txd: crc=0x%08X (%u) size=%u\n", crc, crc, size);
    check(ok && crc == 0xDF68476Au && size == 90,
          "fixture skin_a.txd crc 0xDF68476A size 90");
    ok = nyx::file_crc32_and_size("test-fixtures/skin_b.dff", crc, size);
    printf("skin_b.dff: crc=0x%08X (%u) size=%u\n", crc, crc, size);
    check(ok && crc == 0xF5E54AD0u && size == 1024,
          "fixture skin_b.dff crc 0xF5E54AD0 size 1024");
    ok = nyx::file_crc32_and_size("test-fixtures/skin_b.txd", crc, size);
    printf("skin_b.txd: crc=0x%08X (%u) size=%u\n", crc, crc, size);
    check(ok && crc == 0x0F151593u && size == 80,
          "fixture skin_b.txd crc 0x0F151593 size 80");

    // 3. Registry behaviour.
    nyx::Registry reg;
    check((int)reg.base_pool().size() == 30, "default base pool has 30 slots");
    std::string err;
    int base = reg.register_skin(20000, "skin_a.dff", "skin_a.txd",
                                 "test-fixtures/", err);
    printf("register 20000 -> base %d (err: %s)\n", base, err.c_str());
    check(base == 264, "register skin 20000 assigns base 264 (first pool slot)");
    int base2 = reg.register_skin(20000, "skin_a.dff", "skin_a.txd",
                                  "test-fixtures/", err);
    check(base2 == base, "idempotent re-register returns same baseid");
    int base3 = reg.register_skin(20001, "skin_b.dff", "skin_b.txd",
                                  "test-fixtures/", err);
    check(base3 == 265, "register skin 20001 assigns base 265");
    check(reg.get_base_skin(20000) == 264, "Nyx_GetBaseSkin(20000) == 264");
    check(reg.get_base_skin(19999) == -1, "unregistered skin -> base -1");
    int bad = reg.register_skin(1999, "skin_a.dff", "skin_a.txd",
                                "test-fixtures/", err);
    check(bad == -1, "skinid 1999 rejected (outside 20000-30000)");
    bad = reg.register_skin(20002, "nope.dff", "skin_a.txd",
                            "test-fixtures/", err);
    check(bad == -1, "missing dff file rejected");
    bad = reg.register_skin(20002, "skin_a.dff", "skin_a.txd",
                            "test-fixtures/", err);
    check(bad == 266, "register after failures still works (base 266)");
    bad = reg.register_skin(20003, "../secret.dff", "skin_a.txd",
                            "test-fixtures/", err);
    check(bad == -1, "traversal dff name rejected at registration");
    bad = reg.register_skin(20003, "skin_a.dff", "skin_a.txd",
                            "test-fixtures/", err);
    check(bad == 267, "register skin 20003 assigns base 267");

    // Pool exhaustion: tiny pool of 1 slot.
    nyx::Registry small;
    std::vector<int> one;
    one.push_back(270);
    check(small.set_base_pool(one) == 1, "custom pool of 1 slot");
    int s1 = small.register_skin(20000, "skin_a.dff", "skin_a.txd",
                                 "test-fixtures/", err);
    int s2 = small.register_skin(20001, "skin_b.dff", "skin_b.txd",
                                 "test-fixtures/", err);
    check(s1 == 270 && s2 == -1, "pool exhausted -> -1");

    // 4. Manifest JSON (CRC as unsigned decimal — contractual for M2).
    std::string manifest =
        nyx::build_manifest_json(reg.snapshot_skins());
    printf("manifest: %s\n", manifest.c_str());
    const char *expected =
        "[{\"skinid\":20000,\"baseid\":264,\"dff\":\"skin_a.dff\","
        "\"txd\":\"skin_a.txd\",\"dff_crc32\":688229491,"
        "\"txd_crc32\":3748153194,\"dff_size\":256,\"txd_size\":90},"
        "{\"skinid\":20001,\"baseid\":265,\"dff\":\"skin_b.dff\","
        "\"txd\":\"skin_b.txd\",\"dff_crc32\":4125444816,"
        "\"txd_crc32\":253040019,\"dff_size\":1024,\"txd_size\":80},"
        "{\"skinid\":20002,\"baseid\":266,\"dff\":\"skin_a.dff\","
        "\"txd\":\"skin_a.txd\",\"dff_crc32\":688229491,"
        "\"txd_crc32\":3748153194,\"dff_size\":256,\"txd_size\":90},"
        "{\"skinid\":20003,\"baseid\":267,\"dff\":\"skin_a.dff\","
        "\"txd\":\"skin_a.txd\",\"dff_crc32\":688229491,"
        "\"txd_crc32\":3748153194,\"dff_size\":256,\"txd_size\":90}]";
    check(manifest == expected, "manifest JSON exact match (decimal CRCs)");

    // 5. Players JSON: only connected players exposed.
    check(reg.set_player_skin(0, 20000), "set player 0 -> skin 20000");
    check(reg.set_player_skin(5, 20001), "set player 5 -> skin 20001");
    check(!reg.set_player_skin(7, 29999),
          "set player skin to unregistered skin fails");
    check(!reg.set_player_skin(1000, 20000),
          "set player skin with playerid 1000 fails");
    reg.set_player_connected(0, true);
    reg.set_player_connected(5, false);  // disconnected: must be hidden
    std::string players = nyx::build_players_json(reg.snapshot_players());
    printf("players: %s\n", players.c_str());
    check(players == "{\"0\":20000}",
          "players JSON exposes only connected player 0");
    reg.erase_player(0);
    check(nyx::build_players_json(reg.snapshot_players()) == "{}",
          "players JSON empty after erase");

    // 6. Filename validation.
    check(nyx::is_valid_model_filename("skin_a.dff", ".dff"),
          "valid dff name accepted");
    check(!nyx::is_valid_model_filename("../skin_a.dff", ".dff"),
          "traversal '..' rejected");
    check(!nyx::is_valid_model_filename("sub/skin_a.dff", ".dff"),
          "slash in name rejected");
    check(!nyx::is_valid_model_filename("sub\\skin_a.dff", ".dff"),
          "backslash in name rejected");
    check(!nyx::is_valid_model_filename("skin_a.txd", ".dff"),
          "wrong extension rejected");
    check(!nyx::is_valid_model_filename("", ".dff"), "empty name rejected");
    check(nyx::is_valid_model_filename("SKIN_A.DFF", ".dff"),
          "uppercase extension accepted");

    // 7. find_file (HTTP file endpoint lookup).
    nyx::SkinEntry entry;
    bool is_dff = false;
    check(reg.find_file("skin_b.txd", entry, is_dff) && !is_dff &&
              entry.skinid == 20001,
          "find_file locates registered txd");
    check(!reg.find_file("other.dff", entry, is_dff),
          "find_file rejects unregistered file");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
