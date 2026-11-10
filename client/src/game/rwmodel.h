// rwmodel.h — resident RenderWare model loader (dff + txd from cache).
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/game/rwmodel.h),
// GPL-3.0. Only the M3 pieces are kept; the calling convention / delta
// handling is unchanged from the reference.
//
// GAME-THREAD ONLY: every function here calls into gta_sa.exe code and
// touches RenderWare state. Never call from the monitor/worker threads.
#pragma once

namespace rwmodel {

struct Loaded {
    void* clump = nullptr;  // RpClump*
    void* txd = nullptr;    // RwTexDictionary*
    bool ok() const { return clump && txd; }
};

// Load the txd (for texture binding) then the clump. Safe on junk files:
// no chunk found -> null pointers, never a crash. `ok()` false means the
// load was incomplete (caller must not hand a half-loaded model to render).
Loaded LoadModel(const char* dffPath, const char* txdPath);

// Destroy loaded rw objects (RpClumpDestroy + RwTexDictionaryDestroy).
// Only call when NO base template still points at `m.clump` — i.e. after
// render::RestoreAll(), or for a model that was never swapped in.
void Free(Loaded& m);

// Load a fresh clump (binding m.txd) into m.clump WITHOUT freeing the old
// one — used after the streamer already destroyed it. Returns ok.
bool ReloadClump(Loaded& m, const char* dffPath);

// Ask the streamer to load a model (CStreaming::RequestModel), keeping a
// custom skin's base ped resident. Idempotent-ish; render.cpp throttles
// repeat calls per base (see kRequestRetryFrames there).
void RequestModel(int modelId);

}  // namespace rwmodel
