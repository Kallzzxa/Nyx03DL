// samp.h — samp.dll access layer for SA-MP 0.3.7 (R5-1 and R1).
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/core/samp.h), GPL-3.0.
// Changes: only the pieces M2 needs (base resolve + fingerprint). No
// urmem, no DebugChat, no hook anchors — M2 performs no patching at all.
// Non-Windows builds get stubs that report "not loaded" (unit harness).
#pragma once

#include <cstdint>

namespace nyx {
namespace samp {

// One RVA table per supported samp.dll build. netGameProcess is the
// per-frame hook target (R5: CNetGame::Process; R1: CNetGame::UpdateNetwork).
// scrSetPlayerSkin == 0 means "no hook on this build" (see header note).
// hostAddressOff / serverPortOff / gameStateOff are CNetGame field offsets
// (packed struct, SAMP-API headers) used to read the server address and
// the connection state WITHOUT calling RakClient at all — v1.2.0 crashed
// on R1 right after connect, in the monitor poll, with the log stopping
// before "connected to" (i.e. inside GetServerAddress, the only vtable
// call there whose slot was never verified on R1); v1.2.1 removed the
// GetServerID call, but the v1.2.1 test still never logged "connected",
// so the remaining vtable call (IsConnected, slot 18) is now also gone:
// on R1 it evidently never returns true. Field reads cannot call the
// wrong function; the offsets are verified in samp.cpp. NOTE: the
// GAME_MODE values differ per build (R1: CONNECTED=14, WAITJOIN=15;
// R5-1: CONNECTED=5, WAITJOIN=6) — hence stateConnected/stateWaitJoin.
struct VersionAddrs {
    const char* name;
    const char* tickName;
    uint32_t imageSize;
    uint32_t timeDateStamp;
    uintptr_t refNetGame;       // CNetGame**
    uintptr_t getRakClient;     // -> RakClientInterface*
    uintptr_t netGameProcess;   // per-frame (thiscall, no args)
    uintptr_t scrSetPlayerSkin; // (RPCParameters*), __cdecl; 0 = none
    uintptr_t refChat;          // CChat**
    uintptr_t chatAddMessage;   // (this, D3DCOLOR color, const char* text)
    uintptr_t hostAddressOff;   // CNetGame::m_szHostAddress (char[257])
    uintptr_t serverPortOff;    // CNetGame::m_nPort (int, packed)
    uintptr_t gameStateOff;     // CNetGame::m_nGameState (int, packed)
    uintptr_t playerPoolsOff;   // CNetGame::m_pPools (Pools*); 0 = unknown
                                // (v1.2.4 rebuild disabled on that build)
    int stateConnected;         // GAME_MODE_CONNECTED value for this build
    int stateWaitJoin;          // GAME_MODE_WAITJOIN value for this build
};

namespace versions {

// R5-1 (from the reference repo, cross-checked vs SAMP-API R5-1).
extern const VersionAddrs kR5_1;
// R1 (from SAMP-API R1; fingerprint from a real user's samp.dll, 2026-11-10).
extern const VersionAddrs kR1;

}  // namespace versions

// Resolve samp.dll base and read its fingerprint. False if not loaded yet;
// safe to call repeatedly.
bool Init();

// samp.dll load base, or 0 before a successful Init().
uintptr_t Base();

// RVA -> absolute address in samp.dll.
inline uintptr_t Addr(uintptr_t rva) { return Base() + rva; }

// PE fingerprint of the loaded samp.dll.
uint32_t ImageSize();
uint32_t TimeDateStamp();

// The table matching the loaded samp.dll, or nullptr if unsupported.
// Set by Init(); valid only after Init() succeeded.
const VersionAddrs* ActiveVersion();

// True if the loaded samp.dll matches one of the supported tables.
// Nothing reads samp.dll RVAs unless this is true.
bool IsSupportedBuild();

// True if the loaded build is specifically 0.3.7-R5-1.
bool IsR5Build();

// Log the fingerprint (+ first bytes at the GetRakClient anchor) so a
// mismatching build can be diagnosed from nyx03dl.log.
void DumpFingerprint();

}  // namespace samp
}  // namespace nyx
