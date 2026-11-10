// nyx03dl — SA-MP 0.3.7 R2 server plugin: custom skins ala 0.3DL (M1).
//
// Gamemode registers skins with SetCostumNyx03DL(skinid, dff, txd); the
// plugin reads the files from scriptfiles/nyx03dl/, computes CRC32 +
// size, assigns a base ped automatically, and serves a manifest + the
// files over a small built-in HTTP server (default port 8777) for the
// client .asi (M2) to poll and download. See README.md and
// ../ARCHITECTURE.md.
//
// Threading: natives run on the server AMX (main) thread; the HTTP
// server runs on its own thread. Shared state lives in nyx::Registry
// (nyx03dl_core.h), guarded by a mutex. sampgdk natives (SetPlayerSkin,
// IsPlayerConnected) are ONLY called on the main thread: from the AMX
// natives themselves and from ProcessTick(). The HTTP thread never
// calls a sampgdk native — it only reads registry snapshots.
//
// Build: build.sh (zig, no MSVC). Config: nyx03dl.cfg next to the DLL.

// Windows headers FIRST: sampgdk.h renames some Windows API functions
// (GetTickCount, SelectObject, ...) to sampgdk_* macros, which collide
// with the real declarations if windows.h is parsed afterwards.
#include <winsock2.h>
#include <ws2tcpip.h>
#include <windows.h>

#include <cstdio>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>
#include <set>
#include <atomic>
#include <thread>
#include <chrono>

#include "sampgdk.h"
#include "nyx03dl_core.h"

#ifdef _MSC_VER
#pragma comment(lib, "ws2_32.lib")
#endif

// --------------------------------------------------------------- config --

static int g_http_port = 8777;
static std::vector<int> g_base_pool;  // empty = use default pool
static bool g_config_found = false;

static std::string trim(const std::string &s) {
    size_t a = 0, b = s.size();
    while (a < b && isspace((unsigned char)s[a])) a++;
    while (b > a && isspace((unsigned char)s[b - 1])) b--;
    return s.substr(a, b - a);
}

static void readConfig() {
    FILE *f = fopen("nyx03dl.cfg", "r");
    if (f == nullptr) return;  // defaults apply; Load() logs this
    g_config_found = true;
    char line[1024];
    while (fgets(line, sizeof(line), f) != nullptr) {
        std::string s = trim(line);
        if (s.empty() || s[0] == '#' || s[0] == ';') continue;
        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string key = trim(s.substr(0, eq));
        std::string val = trim(s.substr(eq + 1));
        if (key == "nyx_http_port") {
            int p = atoi(val.c_str());
            if (p > 0 && p <= 65535) g_http_port = p;
        } else if (key == "nyx_base_pool") {
            std::vector<int> pool;
            if (nyx::parse_base_pool(val, pool) > 0) g_base_pool = pool;
        }
    }
    fclose(f);
}

// ------------------------------------------------------------- registry --

static nyx::Registry g_registry;
static std::set<int> g_seen_players;  // playerids with a skin mapping

// ------------------------------------------------------------- AMX util --

// Fetch a string argument from an AMX native call. Returns false when
// the address is invalid.
//
// BUGFIX v1.0.1: Pawn strings are stored in AMX memory as an array of
// cells (one cell per character), NOT as a C char* string. The old code
// did `out = (const char *)addr`, which on little-endian x86 read only
// the first cell's low byte ("skin1.dff" -> "s", then a 0x00 byte ends
// the string). That is why the server log showed
// SetCostumNyx03DL(20000, "s", "s") FAILED: bad dff filename.
// The correct conversion is amx_GetString(), which unpacks cell-by-cell
// into a C string (forwarded through pAMXFunctions by amxplugin.c,
// same as amx_GetAddr/amx_Register already used here).
static bool amx_string_param(AMX *amx, cell param, std::string &out) {
    cell *addr = nullptr;
    if (amx_GetAddr(amx, param, &addr) != AMX_ERR_NONE || addr == nullptr)
        return false;
    char buf[256];
    if (amx_GetString(buf, addr, 0, sizeof(buf)) != AMX_ERR_NONE)
        return false;
    out = buf;
    return true;
}

// -------------------------------------------------------------- natives --

// native SetCostumNyx03DL(skinid, dff[], txd[]) — spelling kept exactly
// as requested by the user ("Costum"). Returns the assigned base ped id,
// or -1 on failure.
static cell AMX_NATIVE_CALL n_SetCostumNyx03DL(AMX *amx, const cell *params) {
    if (params[0] < 3 * (cell)sizeof(cell)) return -1;
    int skinid = (int)params[1];
    std::string dff, txd;
    if (!amx_string_param(amx, params[2], dff) ||
        !amx_string_param(amx, params[3], txd))
        return -1;
    std::string error;
    int baseid = g_registry.register_skin(skinid, dff, txd,
                                          nyx::kModelDir, error);
    if (baseid < 0) {
        sampgdk::logprintf("[nyx03dl] SetCostumNyx03DL(%d, \"%s\", \"%s\") "
                           "FAILED: %s",
                           skinid, dff.c_str(), txd.c_str(), error.c_str());
        return -1;
    }
    nyx::SkinEntry e;
    bool is_dff = false;
    g_registry.find_file(dff, e, is_dff);
    sampgdk::logprintf("[nyx03dl] skin %d registered: base %d, "
                       "dff \"%s\" crc %u size %u, txd \"%s\" crc %u size %u",
                       skinid, baseid, e.dff.c_str(),
                       (unsigned)e.dff_crc32, (unsigned)e.dff_size,
                       e.txd.c_str(), (unsigned)e.txd_crc32,
                       (unsigned)e.txd_size);
    return baseid;
}

// native Nyx_SetPlayerSkin(playerid, skinid) — 1 on success, 0 on
// failure. Applies the BASE ped through the real SetPlayerSkin so only
// a valid 0.3.7 skin id ever goes over the wire.
static cell AMX_NATIVE_CALL n_Nyx_SetPlayerSkin(AMX *amx, const cell *params) {
    (void)amx;
    if (params[0] < 2 * (cell)sizeof(cell)) return 0;
    int playerid = (int)params[1];
    int skinid = (int)params[2];
    if (playerid < 0 || playerid >= nyx::kMaxPlayers) return 0;
    if (!IsPlayerConnected(playerid)) return 0;
    int baseid = g_registry.get_base_skin(skinid);
    if (baseid < 0) return 0;  // skin not registered
    if (!g_registry.set_player_skin(playerid, skinid)) return 0;
    g_registry.set_player_connected(playerid, true);
    g_seen_players.insert(playerid);
    if (!SetPlayerSkin(playerid, baseid)) {
        g_registry.erase_player(playerid);
        return 0;
    }
    return 1;
}

// native Nyx_GetBaseSkin(skinid) — base ped id, or -1 when not registered.
static cell AMX_NATIVE_CALL n_Nyx_GetBaseSkin(AMX *amx, const cell *params) {
    (void)amx;
    if (params[0] < 1 * (cell)sizeof(cell)) return -1;
    return g_registry.get_base_skin((int)params[1]);
}

static AMX_NATIVE_INFO g_natives[] = {
    {"SetCostumNyx03DL", n_SetCostumNyx03DL},
    {"Nyx_SetPlayerSkin", n_Nyx_SetPlayerSkin},
    {"Nyx_GetBaseSkin", n_Nyx_GetBaseSkin},
    {nullptr, nullptr},
};

// ----------------------------------------------------------- HTTP server --

static std::atomic<bool> g_stop(false);
static std::thread g_http_thread;
static SOCKET g_listen_sock = INVALID_SOCKET;

static bool send_all(SOCKET s, const char *data, size_t len) {
    while (len > 0) {
        int sent = send(s, data, (int)len, 0);
        if (sent <= 0) return false;
        data += sent;
        len -= (size_t)sent;
    }
    return true;
}

static bool send_response(SOCKET s, int code, const char *reason,
                          const char *content_type,
                          const std::string &body) {
    char header[512];
    snprintf(header, sizeof(header),
             "HTTP/1.0 %d %s\r\n"
             "Content-Type: %s\r\n"
             "Content-Length: %u\r\n"
             "Connection: close\r\n\r\n",
             code, reason, content_type, (unsigned)body.size());
    if (!send_all(s, header, strlen(header))) return false;
    if (!body.empty() && !send_all(s, body.data(), body.size()))
        return false;
    return true;
}

// Handles one accepted HTTP connection (HTTP/1.0, connection closed
// after the response). Logs only errors — no per-request spam.
static void handle_http_client(SOCKET client) {
    std::string req;
    char buf[4096];
    while (req.size() < 8192) {
        int n = recv(client, buf, sizeof(buf), 0);
        if (n <= 0) break;
        req.append(buf, n);
        if (req.find("\r\n\r\n") != std::string::npos) break;
    }
    // Parse "METHOD /path HTTP/x.y".
    size_t sp1 = req.find(' ');
    size_t sp2 = (sp1 == std::string::npos)
                     ? std::string::npos
                     : req.find(' ', sp1 + 1);
    if (sp1 == std::string::npos || sp2 == std::string::npos) {
        send_response(client, 400, "Bad Request", "text/plain", "bad request");
        return;
    }
    std::string method = req.substr(0, sp1);
    std::string path = req.substr(sp1 + 1, sp2 - sp1 - 1);
    size_t q = path.find('?');  // tolerate a query string; ignore it
    if (q != std::string::npos) path = path.substr(0, q);

    if (method != "GET") {
        sampgdk::logprintf("[nyx03dl] HTTP 405 %s %s", method.c_str(),
                           path.c_str());
        send_response(client, 405, "Method Not Allowed", "text/plain",
                      "only GET is supported");
        return;
    }

    if (path == "/nyx03dl/manifest") {
        send_response(client, 200, "OK", "application/json",
                      nyx::build_manifest_json(g_registry.snapshot_skins()));
        return;
    }
    if (path == "/nyx03dl/players") {
        send_response(client, 200, "OK", "application/json",
                      nyx::build_players_json(g_registry.snapshot_players()));
        return;
    }
    const std::string file_prefix = "/nyx03dl/file/";
    if (path.compare(0, file_prefix.size(), file_prefix) == 0) {
        std::string name = path.substr(file_prefix.size());
        // Path traversal defence (also enforced at registration).
        if (name.empty() || name.find('/') != std::string::npos ||
            name.find('\\') != std::string::npos ||
            name.find("..") != std::string::npos) {
            sampgdk::logprintf("[nyx03dl] HTTP 403 file \"%s\"",
                               name.c_str());
            send_response(client, 403, "Forbidden", "text/plain",
                          "invalid file name");
            return;
        }
        nyx::SkinEntry entry;
        bool is_dff = false;
        if (!g_registry.find_file(name, entry, is_dff)) {
            sampgdk::logprintf("[nyx03dl] HTTP 404 file \"%s\" (not "
                               "registered)",
                               name.c_str());
            send_response(client, 404, "Not Found", "text/plain",
                          "file not registered");
            return;
        }
        std::string full = std::string(nyx::kModelDir) + name;
        FILE *f = fopen(full.c_str(), "rb");
        if (f == nullptr) {
            sampgdk::logprintf("[nyx03dl] HTTP 404 file \"%s\" (gone from "
                               "disk)",
                               name.c_str());
            send_response(client, 404, "Not Found", "text/plain",
                          "file not found");
            return;
        }
        fseek(f, 0, SEEK_END);
        long size = ftell(f);
        fseek(f, 0, SEEK_SET);
        if (size < 0) size = 0;
        char header[512];
        snprintf(header, sizeof(header),
                 "HTTP/1.0 200 OK\r\n"
                 "Content-Type: application/octet-stream\r\n"
                 "Content-Length: %ld\r\n"
                 "Connection: close\r\n\r\n",
                 size);
        bool ok = send_all(client, header, strlen(header));
        char chunk[8192];
        size_t n;
        while (ok && (n = fread(chunk, 1, sizeof(chunk), f)) > 0)
            ok = send_all(client, chunk, n);
        if (!ok)
            sampgdk::logprintf("[nyx03dl] HTTP send failed for \"%s\"",
                               name.c_str());
        fclose(f);
        return;
    }

    sampgdk::logprintf("[nyx03dl] HTTP 404 path \"%s\"", path.c_str());
    send_response(client, 404, "Not Found", "text/plain", "not found");
}

static void http_thread_main() {
    g_listen_sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_listen_sock == INVALID_SOCKET) {
        sampgdk::logprintf("[nyx03dl] HTTP socket() failed, manifest "
                           "server disabled");
        return;
    }
    int reuse = 1;
    setsockopt(g_listen_sock, SOL_SOCKET, SO_REUSEADDR,
               (const char *)&reuse, sizeof(reuse));
    sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons((u_short)g_http_port);
    if (bind(g_listen_sock, (sockaddr *)&addr, sizeof(addr)) != 0) {
        sampgdk::logprintf("[nyx03dl] HTTP bind(port %d) FAILED — "
                           "manifest server disabled (port in use?)",
                           g_http_port);
        closesocket(g_listen_sock);
        g_listen_sock = INVALID_SOCKET;
        return;
    }
    if (listen(g_listen_sock, 8) != 0) {
        sampgdk::logprintf("[nyx03dl] HTTP listen() FAILED");
        closesocket(g_listen_sock);
        g_listen_sock = INVALID_SOCKET;
        return;
    }
    sampgdk::logprintf("[nyx03dl] HTTP manifest server listening on port "
                       "%d (/nyx03dl/manifest)",
                       g_http_port);
    while (!g_stop.load()) {
        fd_set rfds;
        FD_ZERO(&rfds);
        FD_SET(g_listen_sock, &rfds);
        timeval tv;
        tv.tv_sec = 0;
        tv.tv_usec = 200000;  // 200 ms so the stop flag is polled
        int ready = select(0, &rfds, nullptr, nullptr, &tv);
        if (ready < 0) break;  // socket closed from Unload()
        if (ready == 0) continue;
        SOCKET client = accept(g_listen_sock, nullptr, nullptr);
        if (client == INVALID_SOCKET) {
            if (!g_stop.load())
                std::this_thread::sleep_for(std::chrono::milliseconds(50));
            continue;
        }
        handle_http_client(client);
        closesocket(client);
    }
}

// ------------------------------------------------------- plugin exports --

// sscanf's plugincommon.h predates the ProcessTick flag. The official
// SA-MP SDK value for "this plugin wants ProcessTick()" is 0x20000.
#ifndef SUPPORTS_PROCESS_TICK
#define SUPPORTS_PROCESS_TICK 0x20000
#endif

PLUGIN_EXPORT unsigned int PLUGIN_CALL Supports() {
    return sampgdk::Supports() | SUPPORTS_AMX_NATIVES |
           SUPPORTS_PROCESS_TICK;
}

PLUGIN_EXPORT bool PLUGIN_CALL Load(void **ppData) {
    // amxplugin.c (sdk/amx/) forwards amx_Register/amx_GetAddr through
    // the server's AMX function table; point it at the table first.
    // (sampgdk keeps its own copy and does NOT set this global.)
    extern void *pAMXFunctions;
    pAMXFunctions = ppData[PLUGIN_DATA_AMX_EXPORTS];
    readConfig();
    if (!g_base_pool.empty()) {
        int n = g_registry.set_base_pool(g_base_pool);
        if (n == 0) {
            sampgdk::logprintf("[nyx03dl] WARNING: nyx_base_pool in "
                               "nyx03dl.cfg has no usable ped ids, using "
                               "default pool");
            g_registry.set_base_pool(nyx::default_base_pool());
        }
    }
    if (!sampgdk::Load(ppData)) return false;
    WSADATA wsaData;
    if (WSAStartup(MAKEWORD(2, 2), &wsaData) != 0) {
        sampgdk::logprintf("[nyx03dl] WSAStartup failed");
        return false;
    }
    sampgdk::logprintf("[nyx03dl] loaded (HTTP port %d%s, base pool %d "
                       "slots)",
                       g_http_port,
                       g_config_found ? "" : ", no nyx03dl.cfg — defaults",
                       (int)g_registry.base_pool().size());
    g_stop.store(false);
    g_http_thread = std::thread(http_thread_main);
    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL Unload() {
    g_stop.store(true);
    if (g_listen_sock != INVALID_SOCKET) {
        closesocket(g_listen_sock);  // unblocks select()/accept()
        g_listen_sock = INVALID_SOCKET;
    }
    if (g_http_thread.joinable()) g_http_thread.join();
    WSACleanup();
    sampgdk::logprintf("[nyx03dl] unloaded");
    sampgdk::Unload();
}

PLUGIN_EXPORT int PLUGIN_CALL AmxLoad(AMX *amx) {
    return amx_Register(amx, g_natives, -1);
}

PLUGIN_EXPORT int PLUGIN_CALL AmxUnload(AMX *amx) {
    (void)amx;
    return AMX_ERR_NONE;
}

// sampgdk forwards gamemode callbacks to plugin exports of the same
// name. Erase the mapping immediately on disconnect (ProcessTick's
// polling below is the safety net if this callback ever does not fire).
PLUGIN_EXPORT bool PLUGIN_CALL OnPlayerDisconnect(int playerid, int reason) {
    (void)reason;
    g_registry.erase_player(playerid);
    g_seen_players.erase(playerid);
    return true;
}

PLUGIN_EXPORT void PLUGIN_CALL ProcessTick() {
    sampgdk::ProcessTick();
    // Refresh connectedness on the main thread (the only thread where
    // sampgdk natives may be called). Throttled to ~2x/second; the
    // /players endpoint serves the snapshot, never a live native call.
    static unsigned long last_poll = 0;
    unsigned long now = GetTickCount();
    if (now - last_poll < 500) return;
    last_poll = now;
    for (std::set<int>::iterator it = g_seen_players.begin();
         it != g_seen_players.end();) {
        int playerid = *it;
        if (!IsPlayerConnected(playerid)) {
            g_registry.set_player_connected(playerid, false);
            ++it;
        } else {
            g_registry.set_player_connected(playerid, true);
            ++it;
        }
    }
}
