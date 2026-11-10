// samp.h — samp.dll access layer for SA-MP 0.3.7-R5-1.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/core/samp.h), GPL-3.0.
// Changes: only the pieces M2 needs (base resolve + R5 fingerprint). No
// urmem, no DebugChat, no hook anchors — M2 performs no patching at all.
// Non-Windows builds get stubs that report "not loaded" (unit harness).
#pragma once

#include <cstdint>

namespace nyx {
namespace samp {

// RVA constants for 0.3.7-R5-1 (from the reference repo, which cites
// BlastHackNet/SAMP-API). ONLY valid for the R5-1 build — see IsR5Build().
namespace r5 {

// Global pointers-to-object (the value at base+rva is itself a pointer).
constexpr uintptr_t kRefNetGame = 0x26EB94;  // CNetGame**

// PE fingerprint of the validated 0.3.7-R5-1 samp.dll; build guard before
// any RVA is dereferenced.
constexpr uint32_t kImageSize = 0x0027E000;
constexpr uint32_t kTimeDateStamp = 0x6372C39E;

// CNetGame methods (thiscall).
constexpr uintptr_t kCNetGame_GetRakClient = 0xBBC0;  // -> RakClientInterface*
constexpr uintptr_t kCNetGame_Process = 0xB5B0;       // per-frame; M3 main-thread tick

// CChat (thiscall) — v1.1.0 download-status UI (chatui.*). Same RVAs
// the reference repo uses for its DebugChat.
constexpr uintptr_t kRefChat = 0x26EB80;         // CChat**
constexpr uintptr_t kCChat_AddMessage = 0x68170;  // (this, D3DCOLOR color, const char* text)

// ScrSetPlayerSkin(RPCParameters*), __cdecl. M3 hooks it ONLY as a
// pre-swap trigger (see skinhook.*): the 0.3.7 wire packet
// (u32 PlayerID, u32 skin = 64 bits) is NEVER rewritten.
constexpr uintptr_t kScrSetPlayerSkin = 0x19190;

}  // namespace r5

// Resolve samp.dll base and read its fingerprint. False if not loaded yet;
// safe to call repeatedly.
bool Init();

// samp.dll load base, or 0 before a successful Init().
uintptr_t Base();

// RVA -> absolute address in samp.dll.
inline uintptr_t Addr(uintptr_t rva) { return Base() + rva; }

// PE fingerprint of the loaded samp.dll (for verifying the build is really R5-1).
uint32_t ImageSize();
uint32_t TimeDateStamp();

// True if the loaded samp.dll matches the validated R5-1 fingerprint.
// The monitor thread refuses to use any RVA unless this is true.
bool IsR5Build();

// Log the fingerprint (+ first bytes at the GetRakClient anchor) so a
// mismatching build can be diagnosed from nyx03dl.log.
void DumpFingerprint();

}  // namespace samp
}  // namespace nyx
