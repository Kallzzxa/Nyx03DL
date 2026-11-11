// test-veh.cpp — Linux test harness for the Nyx03DL v1.3.0 vehicle core.
//
// Compiles nyx03dl_core.cpp natively on Linux (no Winsock / sampgdk):
//   g++ -std=c++11 -O2 -I. test-veh.cpp nyx03dl_core.cpp -o test-veh
//   ./test-veh
// Run from server/ (it reads test-fixtures/ relative to the CWD).
//
// Checks (each prints PASS/FAIL, exit code 0 only if all pass):
//   1. Registry: register vehicle 400 from fixtures, idempotent
//      re-register, different files update in place (handling kept),
//      out-of-range vehid (399 / 612) fails, missing file fails.
//   2. Handling: set on a registered vehicle, 0 = stock, negative /
//      absurd values rejected, unregistered vehicle rejected.
//   3. Vehicles JSON: exact expected string (CRC as unsigned decimal,
//      handling with %g).
//   4. find_vehicle_file: dff/txd of a registered vehicle are found;
//      a skin file is NOT found as a vehicle file.

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
    nyx::Registry reg;
    std::string err;

    // Fixtures are TEST-ONLY files (see test-fixtures/README.md) —
    // CRC/size reference values from Python binascii.crc32 (same as
    // test-crc32.cpp): skin_a.dff 0x29058C73/256, skin_a.txd
    // 0xDF68476A/90, skin_b.dff 0xF5E54AD0/1024, skin_b.txd 0x0F151593/80.
    const std::string dir = "test-fixtures/";

    // 1. Register vehicle 400.
    int ok = reg.register_vehicle(400, "skin_a.dff", "skin_a.txd", dir, err);
    check(ok == 1, "register vehicle 400 succeeds");
    check(reg.is_vehicle_registered(400), "vehicle 400 is registered");
    check(!reg.is_vehicle_registered(411), "vehicle 411 is not registered");

    // Idempotent re-register with the same files.
    ok = reg.register_vehicle(400, "skin_a.dff", "skin_a.txd", dir, err);
    check(ok == 1, "idempotent re-register vehicle 400 succeeds");

    // Out-of-range vehid / missing file.
    ok = reg.register_vehicle(399, "skin_a.dff", "skin_a.txd", dir, err);
    check(ok == 0, "vehid 399 rejected");
    ok = reg.register_vehicle(612, "skin_a.dff", "skin_a.txd", dir, err);
    check(ok == 0, "vehid 612 rejected");
    ok = reg.register_vehicle(411, "nope.dff", "skin_a.txd", dir, err);
    check(ok == 0, "missing dff file rejected");
    check(!reg.is_vehicle_registered(411), "failed register leaves 411 unregistered");

    // 2. Handling.
    ok = reg.set_vehicle_handling(400, 220.0f, 2.0f, 3.0f, err);
    check(ok == 1, "set handling on registered vehicle 400 succeeds");
    ok = reg.set_vehicle_handling(411, 220.0f, 2.0f, 3.0f, err);
    check(ok == 0, "set handling on unregistered vehicle rejected");
    ok = reg.set_vehicle_handling(400, -1.0f, 2.0f, 3.0f, err);
    check(ok == 0, "negative maxspeed rejected");
    ok = reg.set_vehicle_handling(400, 501.0f, 2.0f, 3.0f, err);
    check(ok == 0, "maxspeed > 500 rejected");
    ok = reg.set_vehicle_handling(400, 0.0f, 0.0f, 0.0f, err);
    check(ok == 1, "handling all-zero (reset to stock) accepted");

    // Re-apply handling, then re-register with different files: the
    // handling values must survive the in-place update.
    ok = reg.set_vehicle_handling(400, 220.5f, 2.5f, 3.5f, err);
    check(ok == 1, "set handling 220.5/2.5/3.5 succeeds");
    ok = reg.register_vehicle(400, "skin_b.dff", "skin_b.txd", dir, err);
    check(ok == 1, "re-register vehicle 400 with different files succeeds");

    // 3. Vehicles JSON (exact string; CRCs unsigned decimal, %g floats).
    std::string json = nyx::build_vehicles_json(reg.snapshot_vehicles());
    printf("vehicles JSON: %s\n", json.c_str());
    const char *expected =
        "[{\"vehid\":400,\"dff\":\"skin_b.dff\",\"txd\":\"skin_b.txd\","
        "\"dff_crc32\":4125444816,\"txd_crc32\":253040019,"
        "\"dff_size\":1024,\"txd_size\":80,"
        "\"maxspeed\":220.5,\"accel\":2.5,\"brake\":3.5}]";
    check(json == expected, "vehicles JSON matches expected string");

    // 4. find_vehicle_file.
    nyx::VehicleEntry vent;
    bool is_dff = false;
    check(reg.find_vehicle_file("skin_b.dff", vent, is_dff) && is_dff &&
              vent.vehid == 400,
          "find_vehicle_file finds registered dff");
    check(reg.find_vehicle_file("skin_b.txd", vent, is_dff) && !is_dff &&
              vent.vehid == 400,
          "find_vehicle_file finds registered txd");
    check(!reg.find_vehicle_file("skin_a.dff", vent, is_dff),
          "old dff (replaced by re-register) is not found");
    check(!reg.find_vehicle_file("notthere.dff", vent, is_dff),
          "unregistered file is not found");

    printf("%d check(s), %d failure(s)\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
