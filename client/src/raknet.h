// raknet.h — minimal RakClient access for Nyx03DL (polling only, no hooks).
//
// Written for Nyx03DL (M2), from reference/customskin-0.3.7-R5
// vendor/raknet/rakclient.h + src/net/raknet.cpp (GPL-3.0).
//
// WHY A RAW VTABLE CALL FOR IsConnected:
// samp.dll is built with MSVC; this .asi is built with zig/MinGW (Clang,
// Itanium C++ ABI). A C++ class compiled under the Itanium ABI lays out
// its vtable differently from MSVC (Itanium emits TWO destructor slots,
// MSVC ONE), so calling `rak->IsConnected()` through a locally-defined
// C++ interface would dispatch to the wrong slot. The reference .asi
// could use the interface directly because it was built with MSVC.
// We therefore read the vtable pointer ourselves and call slot 18 as a
// plain __thiscall function pointer. The slot index was counted by hand
// from vendor/raknet/rakclient.h — see raknet.cpp.
//
// v1.2.1: the server address is NO LONGER read via RakClient::GetServerID
// (vtable slot 39). On 0.3.7-R1 that call crashed the game right after
// connect — the log stopped before "connected to", i.e. inside
// GetServerAddress, the only unverified vtable call there — with the
// fault in ntdll. The address now comes from CNetGame's own fields
// (m_szHostAddress + m_nPort, per-build offsets in samp.h), which are
// plain memory reads that cannot dispatch to a wrong function. Only
// IsConnected still uses the vtable, and its slot survived many polls
// in the menu on R1 before the crash, so it is the least suspicious one.
#pragma once

#include <cstdint>
#include <string>

namespace nyx {
namespace rak {

// Server address as seen by the client.
struct ServerAddress {
    std::string ip;        // dotted quad, e.g. "127.0.0.1"
    uint16_t gamePort = 0; // SA-MP game port, e.g. 7777
    bool valid = false;
    // "ip:gameport" — identity of the current server (used by NyxState).
    std::string Key() const { return ip + ":" + std::to_string(gamePort); }
};

// The live CNetGame* (from the per-build refNetGame pointer), or nullptr
// when no CNetGame exists yet (not connected / menu). Plain read only.
void* NetGame();

// Resolve the live RakClientInterface* via CNetGame (per-build RVA table).
// Returns nullptr when no CNetGame exists yet (not connected / menu).
// This only READS samp.dll memory; it installs no hook.
void* ResolveRakClient();

// True if the RakClient reports a live connection (vtable slot 18).
bool IsConnected(void* rakClient);

// Server address of the current connection, read from the CNetGame
// fields (m_szHostAddress + m_nPort, resolved via getaddrinfo so a
// hostname works too). Returns false if it cannot be determined.
bool GetServerAddress(void* netGame, ServerAddress& out);

}  // namespace rak
}  // namespace nyx
