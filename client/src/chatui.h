// chatui.h — in-game download-status UI for Nyx03DL (v1.1.0).
//
// The monitor thread (background) enqueues short status lines; the game
// thread drains them by calling samp.dll's CChat::AddMessage — the same
// proven call the reference .asi uses for its DebugChat (RVA table below
// is from ricardoofnl/customskin-0.3.7-R5, GPL-3.0).
//
// Why a queue: CChat::AddMessage must run on the GAME thread. The
// monitor thread must never call into samp.dll / the game directly, so
// it only enqueues; render::Tick() (per-frame, game thread) drains.
//
// Why chat, not a custom HUD: a custom on-screen banner would need
// unverified GTA font/draw addresses; CChat is the one text path the
// reference already proved on the R5 build. The "2 seconds"
// centre-screen banner is done server-side instead — see the
// Nyx_UiTimer() GameText in the example .pwn files (GameTextForPlayer,
// 2000 ms, style 3), which fires as soon as Nyx_HasClient() turns true.
#pragma once

#include <cstdint>
#include <string>

namespace chatui {

// D3DCOLOR values (0xAARRGGBB) for the status lines.
constexpr uint32_t kColorInfo = 0xFFFFFF00;     // yellow: downloading
constexpr uint32_t kColorOk = 0xFF00C853;       // green: successfully downloaded
constexpr uint32_t kColorError = 0xFFFF6347;    // red: download failed

// Enqueue one line for the game thread to show. Thread-safe; may be
// called from the monitor thread. Lines are capped (oldest dropped)
// so a bug can never flood the chat.
void Enqueue(uint32_t color, const std::string& text);

// Show every queued line in the SA-MP chat. GAME THREAD ONLY — called
// from render::Tick(). Build-gated and silent when samp.dll / the chat
// object is not available yet.
void Drain();

}  // namespace chatui
