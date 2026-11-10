// nyx03dl_core.h — portable core of the Nyx03DL server plugin (M1).
//
// This file has NO Windows / Winsock / sampgdk dependencies, so it can be
// compiled natively on Linux for the test harness (test-crc32.cpp).
// The Windows plugin (nyx03dl.cpp) links this same code.
//
// Contents:
//   - CRC-32/IEEE (poly 0xEDB88320 reflected, init 0xFFFFFFFF, final xor
//     0xFFFFFFFF) — the same checksum open.mp artwork / the reference
//     customskin .asi use. Test vector: CRC32("123456789") == 0xCBF43926.
//   - In-memory skin registry + base-ped pool + player->skin mapping,
//     all guarded by one std::mutex (natives run on the AMX thread, the
//     HTTP server runs on its own thread).
//   - JSON builders for the /nyx03dl/manifest and /nyx03dl/players
//     endpoints. CRC32 values are emitted as UNSIGNED DECIMAL numbers
//     (not hex) — M2 (the .asi) must rely on this.

#ifndef NYX03DL_CORE_H
#define NYX03DL_CORE_H

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>
#include <map>
#include <mutex>

namespace nyx {

// Skin-id range for custom skins (agreed in ARCHITECTURE.md). 1999 must
// NOT be used: it is inside the game model range (0-19999).
static const int kSkinMin = 20000;
static const int kSkinMax = 30000;

// Valid SA-MP ped (base skin) ids.
static const int kBaseMin = 0;
static const int kBaseMax = 311;

// Max players tracked (SA-MP 0.3.7 server limit is 1000).
static const int kMaxPlayers = 1000;

// Directory (relative to the server working directory) holding the
// .dff/.txd files. Same layout the gamemode/user is told to use.
static const char kModelDir[] = "scriptfiles/nyx03dl/";

// ---------------------------------------------------------------- CRC32 --
// CRC-32/IEEE, byte-at-a-time table implementation.
uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t len);
inline uint32_t crc32_begin() { return 0xFFFFFFFFu; }
inline uint32_t crc32_finish(uint32_t crc) { return crc ^ 0xFFFFFFFFu; }
uint32_t crc32_compute(const unsigned char *data, size_t len);
// CRC32 + size of a whole file. Returns false when the file cannot be
// opened/read; on success fills crc_out (finalised) and size_out.
bool file_crc32_and_size(const std::string &path, uint32_t &crc_out,
                         uint32_t &size_out);

// ------------------------------------------------------- filename rules --
// A registered/downloadable file name must be a plain basename: no '/',
// no '\\', no "..", not empty, and with the expected extension
// (".dff" / ".txd", case-insensitive). Used both at registration time
// and by the HTTP file endpoint (path traversal defence).
bool is_valid_model_filename(const std::string &name, const char *ext);

// ------------------------------------------------------------- registry --

struct SkinEntry {
    int skinid = -1;
    int baseid = -1;
    std::string dff;  // basename only, e.g. "skin_a.dff"
    std::string txd;  // basename only, e.g. "skin_a.txd"
    uint32_t dff_crc32 = 0;
    uint32_t txd_crc32 = 0;
    uint32_t dff_size = 0;
    uint32_t txd_size = 0;
};

class Registry {
public:
    Registry();

    // Base-ped pool. `pool` must contain valid ped ids (0-311); id 0 (CJ)
    // is silently dropped. Replaces any previous pool and frees the
    // used-flags (skins already registered keep their baseid).
    // Returns the number of usable slots.
    int set_base_pool(const std::vector<int> &pool);
    std::vector<int> base_pool() const;

    // Register a skin. Reads dff/txd from <model_dir>/<basename>,
    // computes CRC32 + size, assigns the next free base ped.
    //   model_dir: directory prefix, e.g. "scriptfiles/nyx03dl/"
    //   error: filled with a human-readable reason on failure.
    // Returns the assigned baseid, or -1 on failure (file missing,
    // skinid outside 20000-30000, pool exhausted, bad/empty filename).
    // Idempotent: re-registering the same skinid with the same files
    // returns the same baseid. Re-registering with different files
    // updates the entry in place and KEEPS the old baseid.
    int register_skin(int skinid, const std::string &dff,
                      const std::string &txd, const std::string &model_dir,
                      std::string &error);

    int get_base_skin(int skinid) const;  // -1 when not registered
    bool is_registered(int skinid) const;
    std::vector<SkinEntry> snapshot_skins() const;

    // Find a registered file by basename. Returns true and fills `entry`
    // + `is_dff` when `name` equals the dff or txd of some skin.
    bool find_file(const std::string &name, SkinEntry &entry,
                   bool &is_dff) const;

    // ---- player -> skin mapping ----
    // Returns false when the skin is not registered or playerid is out
    // of range. Does NOT touch game state (the plugin glue calls the
    // real SetPlayerSkin with the baseid separately, on the AMX thread).
    bool set_player_skin(int playerid, int skinid);
    int get_player_skin(int playerid) const;  // -1 when none
    void erase_player(int playerid);
    // Connectedness is maintained by the plugin glue (ProcessTick polls
    // IsPlayerConnected on the AMX thread; OnPlayerDisconnect erases).
    // /players only exposes entries flagged connected.
    void set_player_connected(int playerid, bool connected);
    bool is_player_connected(int playerid) const;
    std::map<int, int> snapshot_players() const;  // connected only

private:
    mutable std::mutex mutex_;
    std::vector<SkinEntry> skins_;
    std::vector<int> base_pool_;
    std::vector<char> base_used_;  // parallel to base_pool_
    std::map<int, int> player_skin_;       // playerid -> skinid
    std::map<int, bool> player_connected_; // playerid -> connected flag
};

// Parse a comma-separated base-pool config value ("264,265,...") into
// ped ids. Invalid entries (non-numeric, outside 0-311) are skipped.
// Returns the number of ids parsed.
int parse_base_pool(const std::string &text, std::vector<int> &out);

// Default base pool: ped ids at the top end of the ped range that are
// rarely used as player skins in RP gamemodes. Chosen best-effort — the
// exact in-game identity of each ped was NOT verified in the VM (no
// GTA/Windows here). Never contains 0 (CJ) or the common player skins.
// Override with nyx_base_pool in nyx03dl.cfg. 30 slots.
std::vector<int> default_base_pool();

// --------------------------------------------------------------- JSON ----
// Manifest JSON: array of
//   {"skinid":..,"baseid":..,"dff":"..","txd":"..",
//    "dff_crc32":<uint decimal>,"txd_crc32":<uint decimal>,
//    "dff_size":..,"txd_size":..}
// CRC32 values are UNSIGNED DECIMAL (not hex) — contractual for M2.
std::string build_manifest_json(const std::vector<SkinEntry> &skins);
// Players JSON: {"<playerid>":<skinid>,...} (connected players only).
std::string build_players_json(const std::map<int, int> &players);
std::string json_escape(const std::string &s);

}  // namespace nyx

#endif  // NYX03DL_CORE_H
