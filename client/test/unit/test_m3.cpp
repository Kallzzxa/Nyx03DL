// test_m3.cpp — Linux unit harness for the portable Nyx03DL M3 units.
//
// Compiles the Windows-independent parts natively on Linux (no WinINet,
// no samp.dll, no GTA addresses):
//   g++ -std=c++17 -O2 -Wall -Wextra -Isrc test/unit/test_m3.cpp
//       src/players.cpp src/nyx_state.cpp src/cache.cpp src/log.cpp
//       src/crc32.cpp -o /tmp/test_m3 && /tmp/test_m3
// Run from client/.
//
// What is tested here (pure logic, no game):
//   1. /players JSON parser: normal documents (multi-player, whitespace,
//      empty object), and rejections (truncated, array top level,
//      non-numeric key, playerid 1000, negative / out-of-range skinid,
//      duplicate key, string value, trailing garbage, value > uint32).
//   2. NyxState player mapping: SetPlayerSkins / SkinForPlayer /
//      IsCustomAssigned gate / PlayerSkins snapshot / ClearPlayerSkins.
//   3. Restore-request flag transitions: Clear() sets it exactly once,
//      TakeRestoreRequest consumes it, RequestRestore re-arms it.
//   4. The renderer's "stale base" decision as pure state logic: a skin
//      that stops being assigned (mapping shrinks / clears) flips
//      IsCustomAssigned from true to false, which is what makes Tick
//      restore that base template.
//
// What is NOT tested here (cannot run without Windows + GTA SA):
//   rwmodel (RenderWare calls at absolute GTA addresses), render
//   (clump-field writes + urmem hooks), skinhook (ScrSetPlayerSkin
//   hook), and the monitor's HTTP polling. Those are verified only by
//   a clean zig build + honest build-only status in the README.
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#include "nyx_state.h"
#include "players.h"

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
    // 1. Players parser: normal documents.
    {
        std::map<int, int> m;
        std::string e;
        bool ok = nyx::ParsePlayers("{\"0\": 20000, \"5\": 20001}", m, e);
        printf("parse normal: ok=%d err='%s' size=%zu\n", (int)ok, e.c_str(), m.size());
        check(ok, "players normal document parses");
        check(m.size() == 2, "players normal document has 2 entries");
        check(m[0] == 20000 && m[5] == 20001, "players entries 0->20000, 5->20001");
    }
    {
        std::map<int, int> m;
        std::string e;
        check(nyx::ParsePlayers("{}", m, e) && m.empty(),
              "players empty object parses to zero entries");
    }
    {
        std::map<int, int> m;
        std::string e;
        bool ok = nyx::ParsePlayers("{\n  \"999\" : 30000 \n}", m, e);
        check(ok && m.size() == 1 && m[999] == 30000,
              "players whitespace + boundary ids 999->30000 parse");
    }
    {
        // Keys with leading zeros are still decimal player ids.
        std::map<int, int> m;
        std::string e;
        bool ok = nyx::ParsePlayers("{\"007\": 20000}", m, e);
        check(ok && m[7] == 20000, "players key \"007\" parses as player 7");
    }

    // 1b. Players parser: rejections — every one must fail, never crash,
    // and must leave `out` untouched.
    auto expectReject = [](const char* name, const std::string& bad) {
        std::map<int, int> m;
        m[0] = 20000;  // sentinel: must survive a failed parse untouched
        std::string e;
        bool ok = nyx::ParsePlayers(bad, m, e);
        printf("reject %-34s ok=%d err='%s'\n", name, (int)ok, e.c_str());
        check(!ok, name);
        check(m.size() == 1 && m[0] == 20000, "rejected parse leaves out untouched");
    };
    expectReject("reject truncated JSON", "{\"0\": 20000");
    expectReject("reject top-level array", "[{\"0\": 20000}]");
    expectReject("reject top-level bare number", "20000");
    expectReject("reject empty document", "");
    expectReject("reject non-numeric key", "{\"abc\": 20000}");
    expectReject("reject negative key", "{\"-1\": 20000}");
    expectReject("reject playerid 1000", "{\"1000\": 20000}");
    expectReject("reject playerid 9999", "{\"9999\": 20000}");
    expectReject("reject negative skinid", "{\"0\": -20000}");
    expectReject("reject skinid 1999", "{\"0\": 1999}");
    expectReject("reject skinid 30001", "{\"0\": 30001}");
    expectReject("reject skinid 0", "{\"0\": 0}");
    expectReject("reject string skinid", "{\"0\": \"20000\"}");
    expectReject("reject skinid above uint32", "{\"0\": 4294967296}");
    expectReject("reject duplicate playerid", "{\"0\": 20000, \"0\": 20001}");
    expectReject("reject missing colon", "{\"0\" 20000}");
    expectReject("reject missing value", "{\"0\": }");
    expectReject("reject trailing garbage", "{\"0\": 20000} garbage");
    expectReject("reject trailing comma", "{\"0\": 20000,}");
    expectReject("reject bad escape in key", "{\"\\q\": 20000}");

    // 2. NyxState player mapping.
    nyx::NyxState& st = nyx::GlobalState();
    st.Clear();  // also consumes any stale restore flag state for a clean start
    (void)st.TakeRestoreRequest();
    check(st.SkinForPlayer(0) == 0, "SkinForPlayer unknown player is 0");
    check(!st.IsCustomAssigned(20000), "IsCustomAssigned false on empty mapping");
    check(st.PlayerSkins().empty(), "PlayerSkins empty initially");

    st.SetPlayerSkins({{0, 20000}, {5, 20001}, {999, 20000}});
    check(st.SkinForPlayer(0) == 20000, "SkinForPlayer(0) == 20000");
    check(st.SkinForPlayer(5) == 20001, "SkinForPlayer(5) == 20001");
    check(st.SkinForPlayer(999) == 20000, "SkinForPlayer(999) == 20000");
    check(st.SkinForPlayer(1) == 0, "SkinForPlayer(1) unassigned is 0");
    check(st.IsCustomAssigned(20000), "IsCustomAssigned(20000) true (2 wearers)");
    check(st.IsCustomAssigned(20001), "IsCustomAssigned(20001) true (1 wearer)");
    check(!st.IsCustomAssigned(20002), "IsCustomAssigned(20002) false (nobody)");
    {
        std::map<int, int> snap = st.PlayerSkins();
        check(snap.size() == 3, "PlayerSkins snapshot has 3 entries");
        snap[0] = 29999;  // mutating the snapshot must not affect state
        check(st.SkinForPlayer(0) == 20000, "PlayerSkins snapshot is a copy");
    }

    // Mapping replacement: player 0 changes skin, player 5 takes it off.
    st.SetPlayerSkins({{0, 20001}});
    check(st.SkinForPlayer(0) == 20001, "mapping replace: player 0 now 20001");
    check(st.SkinForPlayer(5) == 0, "mapping replace: player 5 now unassigned");
    check(!st.IsCustomAssigned(20000),
          "mapping replace: skin 20000 no longer assigned (gate flips off)");
    check(st.IsCustomAssigned(20001), "mapping replace: skin 20001 still assigned");

    st.ClearPlayerSkins();
    check(st.PlayerSkins().empty(), "ClearPlayerSkins empties mapping");
    check(!st.IsCustomAssigned(20001), "IsCustomAssigned false after ClearPlayerSkins");

    // 3. Restore-request flag transitions.
    check(!st.TakeRestoreRequest(), "no restore pending after clean start");
    st.SetPlayerSkins({{0, 20000}});
    st.Clear();  // disconnect: clears mapping AND requests a restore
    check(st.PlayerSkins().empty(), "Clear() drops player mapping");
    check(st.TakeRestoreRequest(), "Clear() sets restore request (first take true)");
    check(!st.TakeRestoreRequest(),
          "restore request consumed exactly once (second take false)");
    st.RequestRestore();
    check(st.TakeRestoreRequest(), "RequestRestore re-arms the flag");
    check(!st.TakeRestoreRequest(), "re-armed flag consumed exactly once");

    // 4. Stale-base decision as pure state logic (what Tick relies on):
    // a skin assigned to a player, then the player disconnects (mapping
    // shrinks) -> gate flips off -> Tick restores that base template.
    st.SetPlayerSkins({{3, 20005}, {7, 20005}});
    check(st.IsCustomAssigned(20005), "gate on while two players wear 20005");
    st.SetPlayerSkins({{3, 20005}});  // player 7 leaves
    check(st.IsCustomAssigned(20005), "gate still on with one wearer left");
    st.SetPlayerSkins({});            // player 3 leaves too
    check(!st.IsCustomAssigned(20005),
          "gate off when last wearer leaves (Tick restores base)");

    printf("\n%d checks, %d failures\n", checks, failures);
    return failures ? 1 : 0;
}
