// nyx03dl_core.cpp — portable core (see nyx03dl_core.h). No Windows /
// Winsock / sampgdk includes: this file compiles natively on Linux for
// the test harness and cross-compiles for Windows with zig.

#include "nyx03dl_core.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cctype>
#include <algorithm>

namespace nyx {

// ---------------------------------------------------------------- CRC32 --
// Table generated for the reflected polynomial 0xEDB88320 (CRC-32/IEEE).

static uint32_t crc_table_[256];
static bool crc_table_ready_ = false;

static void crc_table_init() {
    if (crc_table_ready_) return;
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? (0xEDB88320u ^ (c >> 1)) : (c >> 1);
        crc_table_[i] = c;
    }
    crc_table_ready_ = true;
}

uint32_t crc32_update(uint32_t crc, const unsigned char *data, size_t len) {
    crc_table_init();
    for (size_t i = 0; i < len; i++)
        crc = crc_table_[(crc ^ data[i]) & 0xFF] ^ (crc >> 8);
    return crc;
}

uint32_t crc32_compute(const unsigned char *data, size_t len) {
    return crc32_finish(crc32_update(crc32_begin(), data, len));
}

bool file_crc32_and_size(const std::string &path, uint32_t &crc_out,
                         uint32_t &size_out) {
    FILE *f = fopen(path.c_str(), "rb");
    if (f == nullptr) return false;
    uint32_t crc = crc32_begin();
    uint32_t size = 0;
    unsigned char buf[8192];
    size_t n;
    while ((n = fread(buf, 1, sizeof(buf), f)) > 0) {
        crc = crc32_update(crc, buf, n);
        size += (uint32_t)n;
    }
    bool ok = (ferror(f) == 0);
    fclose(f);
    if (!ok) return false;
    crc_out = crc32_finish(crc);
    size_out = size;
    return true;
}

// ------------------------------------------------------- filename rules --

static bool ends_with_ci(const std::string &name, const char *ext) {
    size_t nl = name.size(), el = strlen(ext);
    if (nl < el) return false;
    for (size_t i = 0; i < el; i++) {
        if (tolower((unsigned char)name[nl - el + i]) !=
            tolower((unsigned char)ext[i]))
            return false;
    }
    return true;
}

bool is_valid_model_filename(const std::string &name, const char *ext) {
    if (name.empty()) return false;
    if (name.find('/') != std::string::npos) return false;
    if (name.find('\\') != std::string::npos) return false;
    if (name.find("..") != std::string::npos) return false;
    if (name[0] == '.') return false;
    if (!ends_with_ci(name, ext)) return false;
    return true;
}

// ------------------------------------------------------------- registry --

Registry::Registry() { set_base_pool(default_base_pool()); }

std::vector<int> default_base_pool() {
    // 30 slots, ped ids 264-293. Top-end peds, rarely used as player
    // skins in RP gamemodes; 0 (CJ) and the common player skins are
    // deliberately absent. Identity per ped NOT verified in-game (no
    // GTA/Windows in the VM) — override via nyx_base_pool in
    // nyx03dl.cfg if any of these turns out to be in use on the server.
    std::vector<int> pool;
    for (int id = 264; id <= 293; id++) pool.push_back(id);
    return pool;
}

int Registry::set_base_pool(const std::vector<int> &pool) {
    std::lock_guard<std::mutex> lock(mutex_);
    base_pool_.clear();
    for (size_t i = 0; i < pool.size(); i++) {
        int id = pool[i];
        if (id <= kBaseMin || id > kBaseMax) continue;  // drop 0 (CJ) etc.
        if (std::find(base_pool_.begin(), base_pool_.end(), id) !=
            base_pool_.end())
            continue;  // drop duplicates
        base_pool_.push_back(id);
    }
    base_used_.assign(base_pool_.size(), 0);
    // Skins already registered keep their baseid; if a registered base
    // is still in the new pool, mark it used so it is not re-assigned.
    for (size_t s = 0; s < skins_.size(); s++) {
        for (size_t i = 0; i < base_pool_.size(); i++) {
            if (base_pool_[i] == skins_[s].baseid) base_used_[i] = 1;
        }
    }
    return (int)base_pool_.size();
}

std::vector<int> Registry::base_pool() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return base_pool_;
}

int Registry::register_skin(int skinid, const std::string &dff,
                            const std::string &txd,
                            const std::string &model_dir,
                            std::string &error) {
    if (skinid < kSkinMin || skinid > kSkinMax) {
        error = "skinid outside 20000-30000";
        return -1;
    }
    if (!is_valid_model_filename(dff, ".dff")) {
        error = "bad dff filename (need a plain basename ending .dff)";
        return -1;
    }
    if (!is_valid_model_filename(txd, ".txd")) {
        error = "bad txd filename (need a plain basename ending .txd)";
        return -1;
    }
    uint32_t dff_crc = 0, txd_crc = 0, dff_size = 0, txd_size = 0;
    if (!file_crc32_and_size(model_dir + dff, dff_crc, dff_size)) {
        error = "dff file not found/unreadable: " + model_dir + dff;
        return -1;
    }
    if (!file_crc32_and_size(model_dir + txd, txd_crc, txd_size)) {
        error = "txd file not found/unreadable: " + model_dir + txd;
        return -1;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    // Idempotent re-registration with the same files.
    for (size_t i = 0; i < skins_.size(); i++) {
        if (skins_[i].skinid == skinid) {
            if (skins_[i].dff == dff && skins_[i].txd == txd) {
                // Same files: refresh crc/size (files may have changed
                // on disk) and return the same baseid.
                skins_[i].dff_crc32 = dff_crc;
                skins_[i].txd_crc32 = txd_crc;
                skins_[i].dff_size = dff_size;
                skins_[i].txd_size = txd_size;
                return skins_[i].baseid;
            }
            // Same skinid, different files: update in place, keep base.
            skins_[i].dff = dff;
            skins_[i].txd = txd;
            skins_[i].dff_crc32 = dff_crc;
            skins_[i].txd_crc32 = txd_crc;
            skins_[i].dff_size = dff_size;
            skins_[i].txd_size = txd_size;
            return skins_[i].baseid;
        }
    }
    // New skin: take the next free base ped.
    for (size_t i = 0; i < base_pool_.size(); i++) {
        if (!base_used_[i]) {
            base_used_[i] = 1;
            SkinEntry e;
            e.skinid = skinid;
            e.baseid = base_pool_[i];
            e.dff = dff;
            e.txd = txd;
            e.dff_crc32 = dff_crc;
            e.txd_crc32 = txd_crc;
            e.dff_size = dff_size;
            e.txd_size = txd_size;
            skins_.push_back(e);
            return e.baseid;
        }
    }
    error = "base ped pool exhausted";
    return -1;
}

int Registry::get_base_skin(int skinid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < skins_.size(); i++)
        if (skins_[i].skinid == skinid) return skins_[i].baseid;
    return -1;
}

bool Registry::is_registered(int skinid) const {
    return get_base_skin(skinid) != -1;
}

std::vector<SkinEntry> Registry::snapshot_skins() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return skins_;
}

bool Registry::find_file(const std::string &name, SkinEntry &entry,
                         bool &is_dff) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < skins_.size(); i++) {
        if (skins_[i].dff == name) {
            entry = skins_[i];
            is_dff = true;
            return true;
        }
        if (skins_[i].txd == name) {
            entry = skins_[i];
            is_dff = false;
            return true;
        }
    }
    return false;
}

// ------------------------------------------------------- v1.3.0 vehicles --

int Registry::register_vehicle(int vehid, const std::string &dff,
                               const std::string &txd,
                               const std::string &model_dir,
                               std::string &error) {
    if (vehid < kVehMin || vehid > kVehMax) {
        error = "vehid outside 400-611";
        return 0;
    }
    if (!is_valid_model_filename(dff, ".dff")) {
        error = "bad dff filename (need a plain basename ending .dff)";
        return 0;
    }
    if (!is_valid_model_filename(txd, ".txd")) {
        error = "bad txd filename (need a plain basename ending .txd)";
        return 0;
    }
    uint32_t dff_crc = 0, txd_crc = 0, dff_size = 0, txd_size = 0;
    if (!file_crc32_and_size(model_dir + dff, dff_crc, dff_size)) {
        error = "dff file not found/unreadable: " + model_dir + dff;
        return 0;
    }
    if (!file_crc32_and_size(model_dir + txd, txd_crc, txd_size)) {
        error = "txd file not found/unreadable: " + model_dir + txd;
        return 0;
    }

    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < vehicles_.size(); i++) {
        if (vehicles_[i].vehid != vehid) continue;
        // Same files: refresh crc/size; different files: update in
        // place. Handling values are kept either way.
        vehicles_[i].dff = dff;
        vehicles_[i].txd = txd;
        vehicles_[i].dff_crc32 = dff_crc;
        vehicles_[i].txd_crc32 = txd_crc;
        vehicles_[i].dff_size = dff_size;
        vehicles_[i].txd_size = txd_size;
        return 1;
    }
    VehicleEntry e;
    e.vehid = vehid;
    e.dff = dff;
    e.txd = txd;
    e.dff_crc32 = dff_crc;
    e.txd_crc32 = txd_crc;
    e.dff_size = dff_size;
    e.txd_size = txd_size;
    vehicles_.push_back(e);
    return 1;
}

int Registry::set_vehicle_handling(int vehid, float maxspeed, float accel,
                                   float brake, std::string &error) {
    // 0 = leave stock. Caps are permissive sanity limits, not physics:
    // maxspeed in km/h (cfg units), accel/brake in cfg units.
    if (maxspeed < 0 || maxspeed > 500) {
        error = "maxspeed must be 0 (stock) or 0-500 km/h";
        return 0;
    }
    if (accel < 0 || accel > 50) {
        error = "accel must be 0 (stock) or 0-50";
        return 0;
    }
    if (brake < 0 || brake > 50) {
        error = "brake must be 0 (stock) or 0-50";
        return 0;
    }
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < vehicles_.size(); i++) {
        if (vehicles_[i].vehid != vehid) continue;
        vehicles_[i].maxspeed = maxspeed;
        vehicles_[i].accel = accel;
        vehicles_[i].brake = brake;
        return 1;
    }
    error = "vehicle not registered (call NyxReplaceVeh first)";
    return 0;
}

bool Registry::is_vehicle_registered(int vehid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < vehicles_.size(); i++)
        if (vehicles_[i].vehid == vehid) return true;
    return false;
}

std::vector<VehicleEntry> Registry::snapshot_vehicles() const {
    std::lock_guard<std::mutex> lock(mutex_);
    return vehicles_;
}

bool Registry::find_vehicle_file(const std::string &name,
                                 VehicleEntry &entry, bool &is_dff) const {
    std::lock_guard<std::mutex> lock(mutex_);
    for (size_t i = 0; i < vehicles_.size(); i++) {
        if (vehicles_[i].dff == name) {
            entry = vehicles_[i];
            is_dff = true;
            return true;
        }
        if (vehicles_[i].txd == name) {
            entry = vehicles_[i];
            is_dff = false;
            return true;
        }
    }
    return false;
}

bool Registry::set_player_skin(int playerid, int skinid) {
    if (playerid < 0 || playerid >= kMaxPlayers) return false;
    std::lock_guard<std::mutex> lock(mutex_);
    bool ok = false;
    for (size_t i = 0; i < skins_.size(); i++)
        if (skins_[i].skinid == skinid) ok = true;
    if (!ok) return false;
    player_skin_[playerid] = skinid;
    return true;
}

int Registry::get_player_skin(int playerid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, int>::const_iterator it = player_skin_.find(playerid);
    return (it == player_skin_.end()) ? -1 : it->second;
}

void Registry::erase_player(int playerid) {
    std::lock_guard<std::mutex> lock(mutex_);
    player_skin_.erase(playerid);
    player_connected_.erase(playerid);
}

void Registry::set_player_connected(int playerid, bool connected) {
    if (playerid < 0 || playerid >= kMaxPlayers) return;
    std::lock_guard<std::mutex> lock(mutex_);
    player_connected_[playerid] = connected;
}

bool Registry::is_player_connected(int playerid) const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, bool>::const_iterator it = player_connected_.find(playerid);
    return (it != player_connected_.end()) ? it->second : false;
}

std::map<int, int> Registry::snapshot_players() const {
    std::lock_guard<std::mutex> lock(mutex_);
    std::map<int, int> out;
    for (std::map<int, int>::const_iterator it = player_skin_.begin();
         it != player_skin_.end(); ++it) {
        std::map<int, bool>::const_iterator c =
            player_connected_.find(it->first);
        if (c != player_connected_.end() && c->second)
            out[it->first] = it->second;
    }
    return out;
}

// ---------------------------------------------------------- config parse --

int parse_base_pool(const std::string &text, std::vector<int> &out) {
    out.clear();
    size_t i = 0, n = text.size();
    while (i < n) {
        while (i < n && (text[i] == ' ' || text[i] == '\t' ||
                         text[i] == ','))
            i++;
        if (i >= n) break;
        size_t start = i;
        if (text[i] == '-' || text[i] == '+') i++;
        while (i < n && isdigit((unsigned char)text[i])) i++;
        if (i == start || (i == start + 1 && (text[start] == '-' ||
                                               text[start] == '+')))
            break;
        int id = atoi(text.substr(start, i - start).c_str());
        if (id > kBaseMin && id <= kBaseMax) out.push_back(id);
        while (i < n && (text[i] == ' ' || text[i] == '\t' ||
                         text[i] == ','))
            i++;
    }
    return (int)out.size();
}

// --------------------------------------------------------------- JSON ----

std::string json_escape(const std::string &s) {
    std::string out;
    for (size_t i = 0; i < s.size(); i++) {
        char c = s[i];
        if (c == '"') out += "\\\"";
        else if (c == '\\') out += "\\\\";
        else if (c == '\n') out += "\\n";
        else if (c == '\r') out += "\\r";
        else if (c == '\t') out += "\\t";
        else if ((unsigned char)c < 0x20) {
            char buf[8];
            snprintf(buf, sizeof(buf), "\\u%04x", (unsigned char)c);
            out += buf;
        } else
            out += c;
    }
    return out;
}

std::string build_manifest_json(const std::vector<SkinEntry> &skins) {
    std::string out = "[";
    for (size_t i = 0; i < skins.size(); i++) {
        if (i > 0) out += ",";
        char buf[512];
        // CRC32 as UNSIGNED DECIMAL (contractual — see header).
        snprintf(buf, sizeof(buf),
                 "{\"skinid\":%d,\"baseid\":%d,\"dff\":\"%s\",\"txd\":\"%s\","
                 "\"dff_crc32\":%u,\"txd_crc32\":%u,"
                 "\"dff_size\":%u,\"txd_size\":%u}",
                 skins[i].skinid, skins[i].baseid,
                 json_escape(skins[i].dff).c_str(),
                 json_escape(skins[i].txd).c_str(),
                 (unsigned)skins[i].dff_crc32,
                 (unsigned)skins[i].txd_crc32,
                 (unsigned)skins[i].dff_size,
                 (unsigned)skins[i].txd_size);
        out += buf;
    }
    out += "]";
    return out;
}

std::string build_vehicles_json(const std::vector<VehicleEntry> &vehicles) {
    std::string out = "[";
    for (size_t i = 0; i < vehicles.size(); i++) {
        if (i > 0) out += ",";
        char buf[512];
        // CRC32 as UNSIGNED DECIMAL (same contract as the skin manifest).
        // Handling numbers with %g (0 = stock, left unset client-side).
        snprintf(buf, sizeof(buf),
                 "{\"vehid\":%d,\"dff\":\"%s\",\"txd\":\"%s\","
                 "\"dff_crc32\":%u,\"txd_crc32\":%u,"
                 "\"dff_size\":%u,\"txd_size\":%u,"
                 "\"maxspeed\":%g,\"accel\":%g,\"brake\":%g}",
                 vehicles[i].vehid,
                 json_escape(vehicles[i].dff).c_str(),
                 json_escape(vehicles[i].txd).c_str(),
                 (unsigned)vehicles[i].dff_crc32,
                 (unsigned)vehicles[i].txd_crc32,
                 (unsigned)vehicles[i].dff_size,
                 (unsigned)vehicles[i].txd_size,
                 (double)vehicles[i].maxspeed, (double)vehicles[i].accel,
                 (double)vehicles[i].brake);
        out += buf;
    }
    out += "]";
    return out;
}

std::string build_players_json(const std::map<int, int> &players) {
    std::string out = "{";
    bool first = true;
    for (std::map<int, int>::const_iterator it = players.begin();
         it != players.end(); ++it) {
        if (!first) out += ",";
        first = false;
        char buf[64];
        snprintf(buf, sizeof(buf), "\"%d\":%d", it->first, it->second);
        out += buf;
    }
    out += "}";
    return out;
}

}  // namespace nyx
