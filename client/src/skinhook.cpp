// skinhook.cpp — ScrSetPlayerSkin hook for Nyx03DL (M3).
//
// Written for Nyx03DL (M3). Pattern adapted from
// ricardoofnl/customskin-0.3.7-R5 (src/net/dlcompat.cpp), GPL-3.0 —
// with the DL packet rewriting REMOVED: Nyx has no CustomSkin field on
// the wire, so the hook is read-only w.r.t. the packet and only calls
// render::EnsureSwapForCustom before the original handler runs.
#include "skinhook.h"

#include "game/render.h"
#include "log.h"
#include "nyx_state.h"
#include "samp.h"

#include <windows.h>
#include <urmem.hpp>

#include <cstdint>

namespace skinhook {
namespace {

// Minimal RPCParameters layout, mirroring vendor/raknet/rakclient.h
// (reference repo): input pointer + bit count are the only fields read
// here; the rest is present so the struct size matches the real one.
#pragma pack(push, 1)
struct PlayerID {
    uint32_t binaryAddress;
    uint16_t port;
};
struct RPCParameters {
    unsigned char* input;
    unsigned int numberOfBitsOfData;
    PlayerID sender;
    void* recipient;
    void* replyToSender;
};
#pragma pack(pop)

urmem::hook g_skinHook;
bool g_installed = false;

// ScrSetPlayerSkin(RPCParameters*), __cdecl. 0.3.7 wire, byte-aligned:
// u32 PlayerID, u32 skin (64 bits total). Read-only: the packet is
// passed to the original handler untouched.
void __cdecl Hooked_ScrSetPlayerSkin(RPCParameters* p) {
    if (p && p->input && p->numberOfBitsOfData >= 64) {
        uint32_t playerId = *reinterpret_cast<uint32_t*>(p->input + 0);
        // uint32_t skin = *reinterpret_cast<uint32_t*>(p->input + 4); // base skin, not needed
        if (playerId <= 999) {
            int custom = nyx::GlobalState().SkinForPlayer(static_cast<int>(playerId));
            if (custom != 0) {
                // Swap the base template NOW, before SA-MP rebuilds the
                // ped: otherwise it clones the un-swapped template and
                // the custom only shows on a second assignment.
                render::EnsureSwapForCustom(custom);
            }
        }
    }
    g_skinHook.call<urmem::calling_convention::cdeclcall, void>(p);
}

}  // namespace

bool Init() {
    if (g_installed) return true;
    if (!nyx::samp::IsR5Build()) {
        NYX_LOGE("skinhook: not R5 build; skipping");
        return false;
    }
    g_skinHook.install(nyx::samp::Addr(nyx::samp::r5::kScrSetPlayerSkin),
                       urmem::get_func_addr(&Hooked_ScrSetPlayerSkin),
                       urmem::hook::type::jmp);
    g_installed = true;
    NYX_LOGI("skinhook: hooked ScrSetPlayerSkin @ 0x%08X",
             static_cast<unsigned>(nyx::samp::Addr(nyx::samp::r5::kScrSetPlayerSkin)));
    return true;
}

bool IsActive() { return g_installed; }

}  // namespace skinhook
