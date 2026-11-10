// skinhook.h — ScrSetPlayerSkin hook for Nyx03DL (M3).
//
// Written for Nyx03DL (M3). Hooks samp.dll's ScrSetPlayerSkin RPC
// handler (RVA 0x19190, __cdecl, RPCParameters*) ONLY as a pre-swap
// trigger: when a player's skin is about to change, the hook looks up
// the player in the /players mapping (polled by the monitor) and, if
// that player wears a custom skin, swaps the base template in FIRST —
// before the original handler rebuilds the ped — so the custom skin
// renders on the first assignment instead of the second.
//
// WHAT THIS HOOK DOES NOT DO (deliberate, unlike the reference's
// dlcompat.cpp): it NEVER rewrites the packet, NEVER touches
// numberOfBitsOfData, and NEVER stores its own mapping. The Nyx wire
// is plain 0.3.7 (u32 PlayerID, u32 skin = 64 bits); the mapping's
// only source of truth is GET /nyx03dl/players.
#pragma once

namespace skinhook {

// Install the ScrSetPlayerSkin hook. Guarded by samp::ActiveVersion()
// (no-op on builds without a verified hook address, e.g. 0.3.7-R1);
// idempotent — a second call is a no-op. False if the hook could not
// be installed (M3 skin-change pre-swap stays off; the per-frame Tick
// still applies skins, M2 keeps working).
bool Init();

// True once the hook is installed.
bool IsActive();

}  // namespace skinhook
