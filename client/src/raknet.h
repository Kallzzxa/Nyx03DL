// raknet.h — minimal RakClient access for Nyx03DL (polling only, no hooks).
//
// Written for Nyx03DL (M2), from reference/customskin-0.3.7-R5
// vendor/raknet/rakclient.h + src/net/raknet.cpp (GPL-3.0).
//
// WHY RAW VTABLE CALLS INSTEAD OF THE C++ INTERFACE:
// samp.dll is built with MSVC; this .asi is built with zig/MinGW (Clang,
// Itanium C++ ABI). A C++ class compiled under the Itanium ABI lays out
// its vtable differently from MSVC (Itanium emits TWO destructor slots,
// MSVC ONE), so calling `rak->IsConnected()` through a locally-defined
// C++ interface would dispatch to the wrong slot. The reference .asi
// could use the interface directly because it was built with MSVC.
// We therefore read the vtable pointer ourselves and call the two slots
// we need as plain __thiscall function pointers. Only the slot INDICES
// below depend on the MSVC layout; they were counted by hand from
// vendor/raknet/rakclient.h — see raknet.cpp for the full enumeration.
//
//   IsConnected : vtable slot 18
//   GetServerID : vtable slot 39
//
// Both are UNVERIFIED at runtime (no Windows/GTA in this VM) — if the
// count is off by one, IsConnected() will read the wrong function and the
// monitor will misbehave. The R5 fingerprint gate + the log lines in the
// monitor are the safety net: a wrong slot shows up immediately as
// "connected" flapping or a crash-free but useless probe loop, never as
// a write to game memory (M2 only ever READS through these calls).
#pragma once

#include <cstdint>
#include <string>

namespace nyx {
namespace rak {

// Server address as seen by RakNet.
struct ServerAddress {
    std::string ip;        // dotted quad, e.g. "127.0.0.1"
    uint16_t gamePort = 0; // SA-MP game port, e.g. 7777
    bool valid = false;
    // "ip:gameport" — identity of the current server (used by NyxState).
    std::string Key() const { return ip + ":" + std::to_string(gamePort); }
};

// Resolve the live RakClientInterface* via CNetGame (R5 RVA table).
// Returns nullptr when no CNetGame exists yet (not connected / menu).
// This only READS samp.dll memory; it installs no hook.
void* ResolveRakClient();

// True if the RakClient reports a live connection.
bool IsConnected(void* rakClient);

// Server address of the current connection (from GetServerID()).
// Returns false if it cannot be determined.
bool GetServerAddress(void* rakClient, ServerAddress& out);

}  // namespace rak
}  // namespace nyx
