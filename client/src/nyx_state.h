// nyx_state.h — shared state between the M2 monitor and the M3 renderer.
//
// Written for Nyx03DL (M2), extended in M3 with the player->skin
// mapping (polled from GET /nyx03dl/players) and the restore request
// flag. The monitor thread writes all of it; the M3 game thread reads
// it. Everything is mutex-guarded; Models() / PlayerSkins() return
// snapshots (copies), so M3 never holds the lock while touching game
// state. The restore flag is the ONLY cross-thread signal into the
// renderer: the monitor sets it on disconnect / server change, and the
// game thread consumes it in render::Tick — game memory is never
// written from the monitor thread.
//
// No game state is touched here — only the manifest, the server identity,
// the player mapping, cache-file readiness, and the restore flag.
#pragma once

#include <cstdint>
#include <map>
#include <mutex>
#include <string>
#include <vector>

namespace nyx {

// One model, as known to the client after a successful manifest probe.
struct Model {
    int skinid = 0;   // skins: 20000..30000; vehicles: vehid 400..611
    int baseid = 0;   // skins: 0..311; vehicles: same as skinid (replaces itself)
    uint32_t dffCrc = 0;
    uint32_t txdCrc = 0;
    uint32_t dffSize = 0;
    uint32_t txdSize = 0;
    std::string dffName;  // file name from the manifest, e.g. "skin_a.dff"
    std::string txdName;  // e.g. "skin_a.txd"
    std::string dffPath;  // absolute cache path, e.g. ...\cache\nyx03dl\29058C73.dff
    std::string txdPath;  // absolute cache path
    bool dffReady = false;  // cache file exists AND its CRC32 matches
    bool txdReady = false;
    // v1.3.0: vehicle replacement (from /nyx03dl/vehicles). A vehicle
    // model is swapped whenever it is loaded — no player mapping gates
    // it, unlike skins. Handling values are 0 when unset (= stock).
    bool isVehicle = false;
    float maxspeed = 0;  // km/h, handling.cfg units (0 = stock)
    float accel = 0;     // engine acceleration, cfg units (0 = stock)
    float brake = 0;     // brake deceleration, cfg units (0 = stock)
};

// Thread-safe holder for "what server are we on, what models does it have,
// which files are downloaded". Written by the monitor thread, read by M3.
class NyxState {
   public:
    // Replace the model list with a freshly probed manifest. Ready flags
    // are computed from the cache on the spot. `serverKey` is "ip:gameport".
    void SetManifest(const std::vector<Model>& models, const std::string& serverKey,
                     uint16_t httpPort);

    // v1.3.0: replace the VEHICLE entries in the model list with a
    // freshly probed /vehicles list (skin entries are kept untouched).
    // Ready flags are computed from the cache on the spot, like
    // SetManifest. Call only while on a Nyx server (same conditions as
    // the manifest probe).
    void SetVehicles(const std::vector<Model>& vehicles);

    // Forget everything about the current server (disconnect / server
    // change). Cache FILES are deliberately kept on disk.
    void Clear();

    // Snapshot of the current model list (copy; safe to hold without the lock).
    std::vector<Model> Models() const;

    // True if a manifest is loaded AND every model's .dff and .txd are
    // present in the cache with matching CRC32.
    bool IsReady() const;

    // True if any manifest is currently loaded (even with downloads pending).
    bool HasManifest() const;

    // "ip:gameport" of the current server, or "" when not on a Nyx server.
    std::string ServerKey() const;

    // HTTP port the manifest was probed from (gameport+1000, or 8777 fallback).
    uint16_t HttpPort() const;

    // Called by the monitor when a download result is drained: mark one
    // file ready (ok=true) or not-ready (ok=false, e.g. CRC mismatch —
    // the file will be re-enqueued on the next probe cycle).
    void MarkDownloaded(int skinid, bool isDff, bool ok);

    // Re-check every file against the cache on disk and refresh all ready
    // flags. Cheap (only a CRC pass over files that exist); the monitor
    // calls it after draining download results.
    void UpdateFromCache();

    // --- M3: player -> custom skin mapping (GET /nyx03dl/players) ---

    // Replace the player->skin mapping with a freshly polled one
    // (playerid 0..999 -> skinid 20000..30000). Called by the monitor
    // thread every ~2 s while on a Nyx server. A failed poll simply
    // does not call this, so the previous mapping stays in force.
    void SetPlayerSkins(const std::map<int, int>& skins);

    // Snapshot of the current player->skin mapping (copy).
    std::map<int, int> PlayerSkins() const;

    // Custom skin the player currently wears, or 0 if none / unknown.
    int SkinForPlayer(int playerid) const;

    // True iff at least one player currently wears `skinid`. The
    // renderer only swaps bases a player wears (prevents bleed onto
    // same-base peds) and restores bases nobody wears anymore.
    bool IsCustomAssigned(int skinid) const;

    // v1.3.0: true iff `skinid` is a replaced vehicle model (400-611
    // with isVehicle set). Vehicle templates swap unconditionally
    // once loaded — no player mapping gates them.
    bool IsVehicle(int skinid) const;

    // Forget the player->skin mapping (disconnect / server change).
    void ClearPlayerSkins();

    // --- M3: restore request (monitor -> game thread) ---

    // Ask the game thread to run render::RestoreAll() + free loaded
    // skins on its next Tick. Set by the monitor on disconnect /
    // server change (via Clear()). The flag is consumed exactly once.
    void RequestRestore();

    // Take (and clear) a pending restore request. True iff one was set.
    // Called ONLY from the game thread (render::Tick).
    bool TakeRestoreRequest();

   private:
    mutable std::mutex mtx_;
    std::vector<Model> models_;
    std::string serverKey_;
    uint16_t httpPort_ = 0;
    std::map<int, int> playerSkins_;  // playerid -> skinid (M3)
    bool restoreRequested_ = false;   // M3: Tick must RestoreAll + free
};

// Process-wide singleton (function-local static; safe to call from any thread).
NyxState& GlobalState();

}  // namespace nyx
