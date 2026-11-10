// samp.h — samp.dll access layer for SA-MP 0.3.7 (R5-1 and R1).
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/core/samp.h), GPL-3.0.
// Changes: only the pieces M2 needs (base resolve + fingerprint). No
// urmem, no DebugChat, no hook anchors — M2 performs no patching at all.
// Non-Windows builds get stubs that report "not loaded" (unit harness).
//
// v1.2.0: per-build RVA table. The .asi used to be R5-1 only; a user log
// proved the gate was the whole story on 0.3.7-R1 (SizeOfImage 0x330000,
// TimeDateStamp 0x5542F47A — .asi exited before doing anything). R1
// addresses come from BlastHackNet/SAMP-API (multiver branch,
// include/src sampapi/0.3.7-R1: CNetGame.cpp, CChat.cpp) — the same
// source the R5-1 table was cross-checked against (its R5-1 values match
// the reference repo exactly), plus SAMP-UDF-for-AutoHotKey for the chat
// pointer (0x21A0E4, agrees). Two R1 addresses could NOT be verified
// from any public table and are NOT guessed:
//   - CNetGame::Process: SAMP-API's R1 CNetGame.cpp does not define it.
//     The per-frame tick instead hooks CNetGame::UpdateNetwork (0xAD70,
//     SAMP-API R1), which Process() calls once per frame.
//   - ScrSetPlayerSkin: no public R1 address found. The hook is simply
//     not installed on R1 (scrSetPlayerSkin == 0); the per-frame Tick
//     still swaps the template, so a custom skin shows on the NEXT
//     /setskin after the swap (repeat the command once). Do NOT invent
//     an address here — a wrong hook address crashes the game on every
//     skin RPC.
#pragma once

#include <cstdint>

namespace nyx {
namespace samp {

// One RVA table per supported samp.dll build. netGameProcess is the
// per-frame hook target (R5: CNetGame::Process; R1: CNetGame::UpdateNetwork).
// scrSetPlayerSkin == 0 means "no hook on this build" (see header note).
// hostAddressOff / serverPortOff are CNetGame field offsets (packed
// struct, SAMP-API headers) used to read the server address WITHOUT
// calling RakClient::GetServerID — v1.2.0 crashed on R1 right after
// connect, in the monitor poll, with the log stopping before "connected
// to" (i.e. inside GetServerAddress, the only vtable call there whose
// slot was never verified on R1). Field reads cannot call the wrong
// function; the offsets are verified below in samp.cpp.
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
