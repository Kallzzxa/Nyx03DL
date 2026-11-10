// skinload.h — ready-model manager for Nyx03DL (M3).
//
// Written for Nyx03DL (M3). Owns the RenderWare-loaded copy of each
// custom skin (clump + txd) and answers "is this skin loaded and ready
// to swap?". It is the Nyx replacement for the reference's artwork
// subsystem (net/artwork.*): instead of mirroring samp.dll's native
// 0.3DL manager, skins are sourced from NyxState (manifest + cache
// readiness, filled by the M2 monitor) and loaded LAZILY — only when a
// player actually wears the skin (IsCustomAssigned).
//
// GAME-THREAD ONLY: every function here either calls rwmodel (game code)
// or is called from render::Tick / the skin-change hook. The monitor
// thread NEVER calls into this module; it only writes NyxState, which
// this module reads as snapshots.
//
// Precondition on the Free* calls: the caller must have restored any
// base template that still points at the freed clump first
// (render::RestoreAll / RestoreBase). render::Tick guarantees the order:
// restore pass, then free pass.
#pragma once

#include <string>

#include "game/rwmodel.h"
#include "nyx_state.h"

namespace skinload {

// One loaded custom skin. `loaded` is empty until EnsureLoaded succeeds.
struct Entry {
    int skinid = 0;   // 20000..30000
    int baseid = 0;   // 0..311
    rwmodel::Loaded loaded;
    std::string dffPath;  // cache path (for ReloadClump)
    std::string txdPath;
};

// Load the skin's files from the cache if (a) the manifest says both
// files are ready and (b) it is not already loaded. A half-loaded entry
// (txd ok, clump lost) is repaired with ReloadClump instead of a full
// re-load. Returns true iff clump+txd are both loaded afterwards.
bool EnsureLoaded(const nyx::Model& m);

// True iff `skinid` is fully loaded; fills baseid/clump on success.
bool GetReady(int skinid, int& baseidOut, void*& clumpOut);

// Visit every entry that is fully loaded (clump+txd present).
// Visitor: void(int skinid, int baseid, void* clump, void* ctx).
using ReadyVisitor = void (*)(int skinid, int baseid, void* clump, void* ctx);
void ForEachReady(ReadyVisitor visit, void* ctx);

// The streamer destroyed a clump we lent to a base template: forget the
// stale pointer (NEVER free it — the streamer already did) and, if
// `reload`, read a fresh clump from the cache. Mirrors the reference's
// artwork::OnCustomClumpLost.
void OnClumpLost(void* clump, bool reload);

// Free one skin's RW objects and drop its entry. Caller must have
// restored its base template first (see header note).
void FreeSkin(int skinid);

// Free ALL loaded skins and drop all entries. Caller must have called
// render::RestoreAll() first.
void FreeAll();

// Free every loaded skin that no player currently wears (per NyxState).
// Caller must have restored those bases first (render::Tick does).
void FreeUnassigned();

}  // namespace skinload
