// monitor.h — Nyx03DL monitor thread (M2).
//
// Written for Nyx03DL (M2). Polls the RakClient connection state, probes
// the Nyx manifest over HTTP when connected, and drives the background
// downloader. Touches NO game state: only samp.dll reads (fingerprint
// gate + RakClient vtable), WinINet, and files under the cache dir.
//
// State machine (see README for the diagram):
//   IDLE -> PROBING -> NYX (manifest known) -> DOWNLOADING -> READY
//   PROBING --probe fails--> SLEEP --30 s--> PROBING
//   any connected state --disconnect--> IDLE
#pragma once

namespace nyx {

// Runs forever (until the process exits). Called once from the monitor
// thread created in DllMain, after samp.dll has been resolved and the
// The samp.dll build fingerprint has been verified.
void MonitorLoop();

}  // namespace nyx
