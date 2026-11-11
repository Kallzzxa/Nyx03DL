// test_veh.cpp — Linux unit harness for the Nyx03DL v1.3.0 client
// vehicle units (portable parts only, no WinINet / samp.dll / GTA).
//
//   g++ -std=c++17 -O2 -Wall -Wextra -Isrc test/unit/test_veh.cpp
//       src/manifest.cpp src/game/handling.cpp src/log.cpp
//       -o /tmp/test_veh && /tmp/test_veh
// Run from client/.
//
// Checks (each prints PASS/FAIL, exit code 0 only if all pass):
//   1. ParseVehicles: a two-vehicle document (one with handling, one
//      without — defaults 0) parses to the exact expected values.
//   2. ParseVehicles rejects: vehid out of 400-611, missing field,
//      unknown key, negative handling number, zero CRC, trailing
//      garbage. An empty array parses to zero vehicles.
//   3. handling::FindEntry: with a fake manager block + fake model
//      info (handling id at +0x4A, vehicle id at entry +0x0), the
//      entry for vehid 400 is found at the right offset; a vehicle-id
//      mismatch, an out-of-range handling id, and null pointers all
//      return nullptr.

#include <cstdio>
#include <cstring>
#include <string>
#include <vector>

#include "game/handling.h"
#include "manifest.h"

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

int main() {
    // 1. Valid vehicles document.
    const char* doc =
        "[{\"vehid\":400,\"dff\":\"veh400.dff\",\"txd\":\"veh400.txd\","
        "\"dff_crc32\":688229491,\"txd_crc32\":3748153194,"
        "\"dff_size\":256,\"txd_size\":90,"
        "\"maxspeed\":220,\"accel\":2,\"brake\":3},"
        "{\"vehid\":411,\"dff\":\"veh411.dff\",\"txd\":\"veh411.txd\","
        "\"dff_crc32\":1,\"txd_crc32\":2,"
        "\"dff_size\":10,\"txd_size\":20}]";
    std::vector<nyx::VehicleModel> vehs;
    std::string err;
    bool ok = nyx::ParseVehicles(doc, vehs, err);
    check(ok, "ParseVehicles accepts a valid two-vehicle document");
    check(vehs.size() == 2, "two vehicles parsed");
    if (vehs.size() == 2) {
        check(vehs[0].vehid == 400 && vehs[0].dff == "veh400.dff" &&
                  vehs[0].txd == "veh400.txd" &&
                  vehs[0].dffCrc == 688229491u &&
                  vehs[0].txdCrc == 3748153194u && vehs[0].dffSize == 256 &&
                  vehs[0].txdSize == 90 && vehs[0].maxspeed == 220.0f &&
                  vehs[0].accel == 2.0f && vehs[0].brake == 3.0f,
              "vehicle 400 fields + handling exact");
        check(vehs[1].vehid == 411 && vehs[1].maxspeed == 0.0f &&
                  vehs[1].accel == 0.0f && vehs[1].brake == 0.0f,
              "vehicle 411 handling defaults to 0 (stock)");
    }

    // Fractional handling numbers.
    const char* frac =
        "[{\"vehid\":451,\"dff\":\"a.dff\",\"txd\":\"a.txd\","
        "\"dff_crc32\":1,\"txd_crc32\":2,\"dff_size\":1,\"txd_size\":1,"
        "\"maxspeed\":220.5,\"accel\":2.5,\"brake\":3.5}]";
    vehs.clear();
    ok = nyx::ParseVehicles(frac, vehs, err);
    check(ok && vehs.size() == 1 && vehs[0].maxspeed == 220.5f &&
              vehs[0].accel == 2.5f && vehs[0].brake == 3.5f,
          "fractional handling numbers parse");

    // 2. Rejections.
    vehs.clear();
    ok = nyx::ParseVehicles("[]", vehs, err);
    check(ok && vehs.empty(), "empty vehicles array parses to zero");
    const char* bad399 =
        "[{\"vehid\":399,\"dff\":\"a.dff\",\"txd\":\"a.txd\","
        "\"dff_crc32\":1,\"txd_crc32\":2,\"dff_size\":1,\"txd_size\":1}]";
    check(!nyx::ParseVehicles(bad399, vehs, err), "vehid 399 rejected");
    const char* bad612 =
        "[{\"vehid\":612,\"dff\":\"a.dff\",\"txd\":\"a.txd\","
        "\"dff_crc32\":1,\"txd_crc32\":2,\"dff_size\":1,\"txd_size\":1}]";
    check(!nyx::ParseVehicles(bad612, vehs, err), "vehid 612 rejected");
    const char* missing =
        "[{\"vehid\":400,\"dff\":\"a.dff\","
        "\"dff_crc32\":1,\"txd_crc32\":2,\"dff_size\":1,\"txd_size\":1}]";
    check(!nyx::ParseVehicles(missing, vehs, err),
          "missing txd field rejected");
    const char* unknown =
        "[{\"vehid\":400,\"dff\":\"a.dff\",\"txd\":\"a.txd\","
        "\"dff_crc32\":1,\"txd_crc32\":2,\"dff_size\":1,\"txd_size\":1,"
        "\"nosuch\":1}]";
    check(!nyx::ParseVehicles(unknown, vehs, err), "unknown key rejected");
    const char* neg =
        "[{\"vehid\":400,\"dff\":\"a.dff\",\"txd\":\"a.txd\","
        "\"dff_crc32\":1,\"txd_crc32\":2,\"dff_size\":1,\"txd_size\":1,"
        "\"maxspeed\":-5}]";
    check(!nyx::ParseVehicles(neg, vehs, err),
          "negative maxspeed rejected");
    const char* zerocrc =
        "[{\"vehid\":400,\"dff\":\"a.dff\",\"txd\":\"a.txd\","
        "\"dff_crc32\":0,\"txd_crc32\":2,\"dff_size\":1,\"txd_size\":1}]";
    check(!nyx::ParseVehicles(zerocrc, vehs, err), "zero CRC rejected");
    check(!nyx::ParseVehicles(std::string(doc) + " trailing", vehs, err),
          "trailing garbage rejected");

    // 3. handling::FindEntry on fake memory.
    // Manager block: 0x14 header + 210 entries x 0xE0. Entry #5 is
    // vehicle 400; entry #7 is vehicle 411.
    static char mgr[0x14 + 210 * 0xE0];
    memset(mgr, 0, sizeof(mgr));
    *reinterpret_cast<int*>(mgr + 0x14 + 5 * 0xE0 + 0x0) = 400;
    *reinterpret_cast<int*>(mgr + 0x14 + 7 * 0xE0 + 0x0) = 411;
    // Fake model info: handling id at +0x4A.
    static char mi400[0x50];
    memset(mi400, 0, sizeof(mi400));
    *reinterpret_cast<unsigned short*>(mi400 + 0x4A) = 5;
    static char mi411[0x50];
    memset(mi411, 0, sizeof(mi411));
    *reinterpret_cast<unsigned short*>(mi411 + 0x4A) = 7;
    void* e400 = handling::FindEntry(mgr, mi400, 400);
    check(e400 == mgr + 0x14 + 5 * 0xE0,
          "FindEntry(400) returns entry #5 at the right offset");
    void* e411 = handling::FindEntry(mgr, mi411, 411);
    check(e411 == mgr + 0x14 + 7 * 0xE0,
          "FindEntry(411) returns entry #7 at the right offset");
    // Same handling id but the entry belongs to another model.
    check(handling::FindEntry(mgr, mi400, 411) == nullptr,
          "vehicle-id mismatch returns nullptr (no wrong patch)");
    static char miBad[0x50];
    memset(miBad, 0, sizeof(miBad));
    *reinterpret_cast<unsigned short*>(miBad + 0x4A) = 250;
    check(handling::FindEntry(mgr, miBad, 400) == nullptr,
          "handling id >= 210 returns nullptr");
    check(handling::FindEntry(nullptr, mi400, 400) == nullptr,
          "null manager returns nullptr");
    check(handling::FindEntry(mgr, nullptr, 400) == nullptr,
          "null model info returns nullptr");

    printf("%d check(s), %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
