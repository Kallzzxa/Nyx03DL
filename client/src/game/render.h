// render.h — clump-swap renderer for Nyx03DL (M3).
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/game/render.h),
// GPL-3.0. Same shape as the reference (per-frame Tick via a
// CNetGame::Process hook, SwapOne, RestoreAll, EnsureSwapForCustom),
// with the data source changed: the reference read ready models from
// its artwork subsystem (fed by the native 0.3DL manager); Nyx reads
// them from skinload (fed by NyxState + the M2 cache).
//
// GAME-THREAD ONLY: Tick / RestoreAll / EnsureSwapForCustom write game
// memory (base template clump fields, and — since v1.2.4 — one
// CPed::SetModelIndex rebuild per learned skin change, the same call
// the game's own skin RPC makes). They are called ONLY from the
// Process hook and the ScrSetPlayerSkin hook — both run on the game
// thread. The monitor thread requests a restore by setting a flag in
// NyxState (RequestRestore), which Tick consumes; it NEVER calls here
// directly.
//
// v1.2.4 retires the old "NEVER TOUCH A LIVE PED" rule: only swapping
// the template was not enough, because a ped that already exists never
// re-reads its template — a rebuild is required, and the second
// /setskin in v1.2.2 proved the rebuild is what shows the custom skin.
// The rebuild is deliberately narrow: local player only, once per
// mapping change, only after the template swap is in place, only on
// the game thread. DeleteRwObject / CreateRwObject are still never
// called directly — SetModelIndex does the rebuild itself.
#pragma once

namespace render {

// Install the per-frame tick (hook CNetGame::Process). Guarded by
// samp::IsSupportedBuild(); idempotent — a second call is a no-op. False if
// the hook could not be installed (M3 stays off, M2 keeps working).
bool Init();

// True once the Process hook is installed.
bool IsActive();

// Restore every swapped base template to its original clump. Game
// thread only (called from Tick). Call before freeing custom clumps
// (disconnect / GMX / server change).
void RestoreAll();

// Swap in one custom skin's clump right now — called the moment a skin
// is assigned (from the ScrSetPlayerSkin hook, before the ped rebuild)
// so it renders on the first try, not the second. Lazy-loads the skin
// from the cache if needed. No-op (false) if not downloaded yet.
bool EnsureSwapForCustom(int skinid);

// Per-frame work, called from the hooked CNetGame::Process AFTER the
// original runs: consume a pending restore request, lazy-load + swap
// every skin a player currently wears, restore bases whose skin is no
// longer worn, and free skins nobody wears. Game thread only.
void Tick();

}  // namespace render
