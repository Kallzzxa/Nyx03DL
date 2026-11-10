// render.cpp — clump-swap renderer for Nyx03DL (M3).
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/game/render.cpp),
// GPL-3.0. The swap mechanics are the reference's (template clump field
// +0x1C, original clump saved once, streamer-loss detection per frame);
// the differences are all Nyx-side:
//  * ready models come from skinload (NyxState + M2 cache), not from
//    the native 0.3DL artwork manager;
//  * the "is this skin worn?" gate reads the /players mapping in
//    NyxState (polled by the monitor), not a DL packet capture;
//  * when a skin stops being worn by ANY player, its base template is
//    restored (the reference only restored everything on reconnect);
//  * RequestModel is throttled per base (at most once per
//    kRequestRetryFrames while the template is empty) instead of every
//    frame, so a missing base does not spam the streamer.
//
// GAME-THREAD ONLY (see render.h). The ONLY writes to game memory in
// the whole .asi happen here: one pointer field (+0x1C) per swapped
// base template, plus the 5-byte hook patches installed by Init().
#include "game/render.h"

#include "game/gta.h"
#include "game/rwmodel.h"
#include "chatui.h"
#include "log.h"
#include "nyx_state.h"
#include "samp.h"
#include "skinload.h"

#include <windows.h>
#include <urmem.hpp>

#include <cstdint>
#include <unordered_map>
#include <vector>

namespace render {
namespace {

urmem::hook g_processHook;
bool g_installed = false;
int g_frame = 0;

// How often (in frames) a still-empty base template may re-request its
// model from the streamer. ~60 frames ~= 1 s at 60 fps.
constexpr int kRequestRetryFrames = 60;

// The swap we applied to one base model's template clump.
struct Swap {
    void* original = nullptr;  // the game's own template clump for this base
    void* custom = nullptr;    // the custom clump we pointed it at
    int customId = 0;          // custom skin id (20000..30000) that owns it
};
std::unordered_map<int, Swap> g_swapped;        // base model id -> swap
std::unordered_map<int, int> g_lastRequest;     // base model id -> frame of last RequestModel

uintptr_t GtaDelta() {
    static uintptr_t d = reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) - 0x400000;
    return d;
}

void* ModelInfo(int id) {
    if (id < 0 || id >= 20000) return nullptr;
    auto arr = reinterpret_cast<void**>(gta::ms_modelInfoPtrs + GtaDelta());
    if (!arr) return nullptr;
    return arr[id];
}

void** ClumpField(void* mi) {
    return reinterpret_cast<void**>(reinterpret_cast<char*>(mi) + gta::kModelInfo_RwObject);
}

// Restore ONE swapped base template. If the template still holds our
// clump, put the game's own clump back; if the streamer already replaced
// it, leave the fresh clump alone and tell skinload to drop the dangling
// pointer (no re-free). Erases the swap record either way.
void RestoreBase(int baseId) {
    auto it = g_swapped.find(baseId);
    if (it == g_swapped.end()) return;
    void* mi = ModelInfo(baseId);
    if (mi) {
        void** field = ClumpField(mi);
        if (*field == it->second.custom) {
            *field = it->second.original;
            NYX_LOGI("render: base %d template restored to original clump", baseId);
        } else {
            // streamer already destroyed our clump: don't clobber the fresh
            // one, and tell skinload to drop the dangling pointer so the
            // upcoming free won't double-free
            skinload::OnClumpLost(it->second.custom, /*reload*/ false);
        }
    } else {
        // Model slot vanished; the clump pointer is still ours to forget.
        skinload::OnClumpLost(it->second.custom, /*reload*/ false);
    }
    g_swapped.erase(it);
}

// Point a base template at a custom clump, only if a player wears it
// (limits bleed onto same-base peds). The base's own clump is saved once
// so RestoreAll / RestoreBase can put it back.
void SwapOne(int skinid, int baseid, void* clump) {
    void* mi = ModelInfo(baseid);
    if (!mi) return;  // base model slot empty
    void** field = ClumpField(mi);

    // Already swapped: check the streamer hasn't stolen our clump, so we always notice the loss.
    auto it = g_swapped.find(baseid);
    if (it != g_swapped.end()) {
        if (*field == it->second.custom) return;  // still installed
        // streamer unloaded the base and destroyed our clump: abandon the
        // dangling pointer, reload, and re-swap next frame
        skinload::OnClumpLost(it->second.custom, /*reload*/ true);
        g_swapped.erase(it);
        return;
    }

    // Keep the base resident before it is worn, so the first assignment
    // renders on the first try. Throttled: at most one request per base
    // per kRequestRetryFrames while the template is still empty.
    if (!*field) {
        auto rit = g_lastRequest.find(baseid);
        if (rit == g_lastRequest.end() || g_frame - rit->second >= kRequestRetryFrames) {
            rwmodel::RequestModel(baseid);
            g_lastRequest[baseid] = g_frame;
        }
        return;
    }

    if (!nyx::GlobalState().IsCustomAssigned(skinid)) return;  // loaded, unworn
    if (*field == clump) return;  // already ours but unrecorded (rare) — leave it

    g_swapped[baseid] = {*field, clump, skinid};  // remember the game's own clump once
    *field = clump;
    NYX_LOGI("render: base %d template clump -> custom %d %p", baseid, skinid, clump);
}

void SwapReadySkin(int skinid, int baseid, void* clump, void* /*ctx*/) {
    SwapOne(skinid, baseid, clump);
}

// CNetGame::Process(this), per frame — our main-thread heartbeat.
void __fastcall Hooked_Process(void* self, void* /*edx*/) {
    g_processHook.call<urmem::calling_convention::thiscall, void>(self);
    Tick();
}

}  // namespace

void RestoreAll() {
    if (g_swapped.empty()) return;
    // Copy the keys first: RestoreBase erases from g_swapped as it goes.
    std::vector<int> bases;
    bases.reserve(g_swapped.size());
    for (const auto& kv : g_swapped) bases.push_back(kv.first);
    for (int base : bases) RestoreBase(base);
    NYX_LOGI("render: restored %zu base template(s) to their original clump",
             bases.size());
}

bool EnsureSwapForCustom(int skinid) {
    std::vector<nyx::Model> models = nyx::GlobalState().Models();
    const nyx::Model* found = nullptr;
    for (const auto& m : models)
        if (m.skinid == skinid) {
            found = &m;
            break;
        }
    if (!found) return false;
    if (!found->dffReady || !found->txdReady) return false;
    if (!skinload::EnsureLoaded(*found)) return false;
    int baseid = 0;
    void* clump = nullptr;
    if (!skinload::GetReady(skinid, baseid, clump)) return false;
    SwapOne(skinid, baseid, clump);
    return true;
}

void Tick() {
    g_frame++;

    // v1.1.0: show queued download-status lines in the SA-MP chat
    // (game thread — the only thread allowed to call CChat::AddMessage).
    chatui::Drain();

    // Disconnect / server change requested a restore (flag set by the
    // monitor thread — the restore itself runs here, on the game thread).
    if (nyx::GlobalState().TakeRestoreRequest()) {
        NYX_LOGI("render: restore requested (disconnect/server change)");
        RestoreAll();
        skinload::FreeAll();
    }

    // Lazy-load + swap every skin a player currently wears.
    std::vector<nyx::Model> models = nyx::GlobalState().Models();
    for (const auto& m : models) {
        if (!m.dffReady || !m.txdReady) continue;
        if (!nyx::GlobalState().IsCustomAssigned(m.skinid)) continue;
        if (!skinload::EnsureLoaded(m)) continue;
    }
    skinload::ForEachReady(&SwapReadySkin, nullptr);

    // Restore bases whose skin is no longer worn by ANY player (player
    // changed skin, disconnected, or the skin left the manifest). Without
    // this the custom clump would keep bleeding onto same-base peds.
    {
        std::vector<int> stale;
        for (const auto& kv : g_swapped)
            if (!nyx::GlobalState().IsCustomAssigned(kv.second.customId))
                stale.push_back(kv.first);
        for (int base : stale) RestoreBase(base);
    }

    // Free loaded skins nobody wears (their bases were restored above,
    // so no template still points at these clumps).
    skinload::FreeUnassigned();
}

bool Init() {
    if (g_installed) return true;
    const auto* ver = nyx::samp::ActiveVersion();
    if (!ver) {
        NYX_LOGE("render: no supported samp.dll build; skipping");
        return false;
    }
    g_processHook.install(nyx::samp::Addr(ver->netGameProcess),
                          urmem::get_func_addr(&Hooked_Process),
                          urmem::hook::type::jmp);
    g_installed = true;
    NYX_LOGI("render: hooked %s @ 0x%08X (per-frame tick)",
             ver->tickName,
             static_cast<unsigned>(nyx::samp::Addr(ver->netGameProcess)));
    return true;
}

bool IsActive() { return g_installed; }

}  // namespace render
