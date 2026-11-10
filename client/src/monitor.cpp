// monitor.cpp — Nyx03DL monitor thread (M2, extended in M3).
//
// Written for Nyx03DL (M2). Polling only, no hooks: every ~500 ms it
// resolves the RakClient via CNetGame and asks IsConnected(). On a fresh
// connect it probes GET http://<ip>:<httpport>/nyx03dl/manifest; on
// success it enqueues any missing .dff/.txd into the background
// downloader and drains finished results. On disconnect it drops the
// model list (cache files stay on disk) and arms a re-probe for the
// next connect. A failed probe sleeps ~30 s and retries while still
// connected, which also covers GMX into a Nyx server later.
//
// M3 addition: while on a Nyx server (DOWNLOADING/READY) the monitor
// also polls GET /nyx03dl/players every ~2 s and stores the
// playerid->skinid mapping in NyxState for the game-thread renderer.
// The monitor NEVER writes game memory: on disconnect / server change
// it only clears NyxState (which sets the restore-request flag that
// render::Tick consumes on the game thread).
#include "monitor.h"

#include "cache.h"
#include "chatui.h"
#include "download.h"
#include "http.h"
#include "log.h"
#include "manifest.h"
#include "nyx_state.h"
#include "players.h"
#include "raknet.h"

#include <chrono>
#include <cstdio>
#include <map>
#include <string>
#include <vector>

#ifdef _WIN32
#include <windows.h>
#else
#include <thread>
#endif

namespace nyx {
namespace {

constexpr int kPollMs = 500;              // connection poll interval
constexpr int kProbeRetryMs = 30000;      // SLEEP -> re-probe interval
constexpr int kManifestTimeoutMs = 3000;  // HTTP connect/read timeout for probe
constexpr int kPlayersPollMs = 2000;      // /players poll interval (M3)
constexpr int kPlayersTimeoutMs = 3000;   // HTTP timeout for /players (M3)
constexpr uint16_t kHttpPortFallback = 8777;  // M1 default HTTP port

enum class State { IDLE, PROBING, SLEEP, DOWNLOADING, READY };

const char* StateName(State s) {
    switch (s) {
        case State::IDLE: return "IDLE";
        case State::PROBING: return "PROBING";
        case State::SLEEP: return "SLEEP";
        case State::DOWNLOADING: return "DOWNLOADING";
        case State::READY: return "READY";
    }
    return "?";
}

void SleepMs(int ms) {
#ifdef _WIN32
    Sleep(static_cast<DWORD>(ms));
#else
    std::this_thread::sleep_for(std::chrono::milliseconds(ms));
#endif
}

using Clock = std::chrono::steady_clock;

std::string ManifestUrl(const std::string& ip, uint16_t httpPort) {
    return "http://" + ip + ":" + std::to_string(httpPort) + "/nyx03dl/manifest";
}

std::string FileUrl(const std::string& ip, uint16_t httpPort, const std::string& name) {
    return "http://" + ip + ":" + std::to_string(httpPort) + "/nyx03dl/file/" + name;
}

std::string PlayersUrl(const std::string& ip, uint16_t httpPort) {
    return "http://" + ip + ":" + std::to_string(httpPort) + "/nyx03dl/players";
}

std::string HelloUrl(const std::string& ip, uint16_t httpPort) {
    return "http://" + ip + ":" + std::to_string(httpPort) + "/nyx03dl/hello";
}

// v1.1.0: announce "this client has nyx03dl and its downloads are
// READY" to the server (GET /nyx03dl/hello). The server records the TCP
// peer IP; Nyx_HasClient(playerid) matches it against GetPlayerIp.
// Sent only while READY — a client still downloading must NOT look
// present, or /setskin would be allowed before the skin can render.
bool SendHello(const rak::ServerAddress& srv, uint16_t httpPort) {
    long status = 0;
    std::string body;
    return http::Get(HelloUrl(srv.ip, httpPort), 3000, status, body);
}

// Poll GET /nyx03dl/players once (M3). On success the mapping is stored
// in NyxState. On ANY failure the previous mapping is kept untouched
// (a failed poll must never wipe skins) and false is returned; the
// caller logs only on success-state transitions to avoid log spam.
bool PollPlayers(const rak::ServerAddress& srv, uint16_t httpPort) {
    std::string url = PlayersUrl(srv.ip, httpPort);
    long status = 0;
    std::string body;
    if (!http::Get(url, kPlayersTimeoutMs, status, body)) return false;
    std::map<int, int> skins;
    std::string err;
    if (!ParsePlayers(body, skins, err)) {
        NYX_LOGW("players: %s -> HTTP 200 but bad JSON: %s", url.c_str(), err.c_str());
        return false;
    }
    GlobalState().SetPlayerSkins(skins);
    return true;
}

// Probe one HTTP port. On success fills `models` and returns true.
bool TryProbePort(const rak::ServerAddress& srv, uint16_t httpPort,
                  std::vector<ManifestModel>& models) {
    std::string url = ManifestUrl(srv.ip, httpPort);
    long status = 0;
    std::string body;
    bool ok = http::Get(url, kManifestTimeoutMs, status, body);
    if (!ok) {
        NYX_LOGW("probe: %s -> failed (http status %ld)", url.c_str(), status);
        return false;
    }
    std::string err;
    if (!ParseManifest(body, models, err)) {
        NYX_LOGW("probe: %s -> HTTP 200 but bad manifest: %s", url.c_str(), err.c_str());
        models.clear();
        return false;
    }
    NYX_LOGI("probe: %s -> OK, %zu model(s)", url.c_str(), models.size());
    return true;
}

// Probe the server's manifest, trying gameport+1000 first, then 8777.
// On success: store the models in the global state, enqueue every file
// whose cache entry is missing/invalid, and return the new state
// (READY if nothing needed downloading, else DOWNLOADING).
// On failure: return SLEEP (retry in ~30 s while still connected).
State DoProbe(const rak::ServerAddress& srv) {
    std::vector<uint16_t> ports;
    uint16_t preferred = static_cast<uint16_t>(srv.gamePort + 1000);
    ports.push_back(preferred);
    if (kHttpPortFallback != preferred) ports.push_back(kHttpPortFallback);

    std::vector<ManifestModel> manifest;
    uint16_t usedPort = 0;
    for (uint16_t p : ports) {
        if (TryProbePort(srv, p, manifest)) {
            usedPort = p;
            break;
        }
    }
    if (usedPort == 0) {
        NYX_LOGI("probe: %s is not a Nyx server (or manifest unreachable); "
                 "sleeping, will retry in ~30 s while connected",
                 srv.Key().c_str());
        return State::SLEEP;
    }

    std::vector<Model> models;
    models.reserve(manifest.size());
    for (const auto& m : manifest) {
        Model model;
        model.skinid = m.skinid;
        model.baseid = m.baseid;
        model.dffCrc = m.dffCrc;
        model.txdCrc = m.txdCrc;
        model.dffSize = m.dffSize;
        model.txdSize = m.txdSize;
        model.dffName = m.dff;
        model.txdName = m.txd;
        models.push_back(model);
    }
    GlobalState().SetManifest(models, srv.Key(), usedPort);

    int enqueued = 0;
    for (const auto& m : manifest) {
        if (!cache::HasValid(m.dffCrc, true)) {
            NYX_LOGI("cache miss: skin %d dff %s (crc %08X), enqueueing", m.skinid,
                     m.dff.c_str(), m.dffCrc);
            dl::Enqueue(m.skinid, FileUrl(srv.ip, usedPort, m.dff),
                        cache::PathFor(m.dffCrc, true), m.dffCrc, true);
            enqueued++;
        } else {
            NYX_LOGI("cache hit: skin %d dff %s (crc %08X)", m.skinid, m.dff.c_str(),
                     m.dffCrc);
        }
        if (!cache::HasValid(m.txdCrc, false)) {
            NYX_LOGI("cache miss: skin %d txd %s (crc %08X), enqueueing", m.skinid,
                     m.txd.c_str(), m.txdCrc);
            dl::Enqueue(m.skinid, FileUrl(srv.ip, usedPort, m.txd),
                        cache::PathFor(m.txdCrc, false), m.txdCrc, false);
            enqueued++;
        } else {
            NYX_LOGI("cache hit: skin %d txd %s (crc %08X)", m.skinid, m.txd.c_str(),
                     m.txdCrc);
        }
    }

    if (enqueued == 0) {
        NYX_LOGI("all %zu model(s) already cached and verified -> READY", models.size());
        // v1.1.0 UI: show the ready line in-game (chat, game thread drains it).
        chatui::Enqueue(chatui::kColorOk,
                        "[Nyx03DL] Custom skins ready (" +
                            std::to_string(models.size()) + " skin(s) from cache)");
        return State::READY;
    }
    NYX_LOGI("enqueued %d file(s) for download", enqueued);
    // v1.1.0 UI: show the downloading line in-game as soon as we start.
    chatui::Enqueue(chatui::kColorInfo,
                    "[Nyx03DL] Downloading custom skins... (" +
                        std::to_string(enqueued) + " file(s))");
    return State::DOWNLOADING;
}

}  // namespace

void MonitorLoop() {
    State state = State::IDLE;
    bool wasConnected = false;
    rak::ServerAddress curServer;
    Clock::time_point lastProbe = Clock::now();
    // /players polling (M3): due immediately once on a Nyx server.
    Clock::time_point lastPlayersPoll = Clock::now() - std::chrono::hours(1);
    bool playersOk = true;  // last poll outcome (for transition-only logging)
    // v1.1.0 hello (presence): due immediately once READY.
    Clock::time_point lastHello = Clock::now() - std::chrono::hours(1);
    bool helloOk = false;

    NYX_LOGI("monitor: started (poll %d ms, probe retry %d ms)", kPollMs, kProbeRetryMs);

    for (;;) {
        SleepMs(kPollMs);

        void* rak = rak::ResolveRakClient();
        bool connected = rak && rak::IsConnected(rak);

        if (!connected) {
            if (wasConnected) {
                NYX_LOGI("disconnected from %s (state %s -> IDLE); model list "
                         "dropped, cache files kept",
                         curServer.Key().c_str(), StateName(state));
                // Clear() also drops the player mapping and sets the
                // restore-request flag; the game thread restores the
                // swapped templates on its next Tick.
                GlobalState().Clear();
                state = State::IDLE;
                wasConnected = false;
                curServer = rak::ServerAddress{};
            }
            continue;
        }

        rak::ServerAddress srv;
        if (!rak::GetServerAddress(rak, srv)) {
            // Connected but the server address is not readable yet; keep
            // polling — never a reason to crash or to probe blindly.
            continue;
        }

        if (!wasConnected || srv.Key() != curServer.Key()) {
            if (wasConnected) {
                NYX_LOGI("server changed %s -> %s; dropping old model list",
                         curServer.Key().c_str(), srv.Key().c_str());
                // Same as disconnect: Clear() drops the mapping and asks
                // the game thread to restore templates.
                GlobalState().Clear();
            }
            wasConnected = true;
            curServer = srv;
            NYX_LOGI("connected to %s (game port %u)", srv.ip.c_str(),
                     static_cast<unsigned>(srv.gamePort));
            state = State::PROBING;
            state = DoProbe(srv);
            lastProbe = Clock::now();
            lastPlayersPoll = Clock::now() - std::chrono::hours(1);  // poll at once
            playersOk = true;
            continue;
        }

        // Still connected to the same server.
        if (state == State::SLEEP) {
            auto now = Clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastProbe)
                    .count() >= kProbeRetryMs) {
                NYX_LOGI("re-probing manifest for %s", srv.Key().c_str());
                state = State::PROBING;
                state = DoProbe(srv);
                lastProbe = Clock::now();
                lastPlayersPoll = Clock::now() - std::chrono::hours(1);
                playersOk = true;
            }
            continue;
        }

        if (state == State::DOWNLOADING || state == State::READY) {
            std::vector<dl::Result> done = dl::Drain();
            for (const auto& r : done) {
                NYX_LOGI("download %s: skin %d %s crc %08X",
                         r.ok ? "done" : "FAILED", r.skinid,
                         r.isDff ? "dff" : "txd", r.crc);
                GlobalState().MarkDownloaded(r.skinid, r.isDff, r.ok);
                // v1.1.0 UI: surface a failed download in-game instead of
                // failing silently (skin would just stay base).
                if (!r.ok) {
                    chatui::Enqueue(chatui::kColorError,
                                    "[Nyx03DL] Download failed for skin " +
                                        std::to_string(r.skinid) +
                                        " (" + (r.isDff ? "dff" : "txd") +
                                        "), will retry next connect.");
                }
            }
            if (!done.empty()) {
                GlobalState().UpdateFromCache();
                if (state == State::DOWNLOADING && GlobalState().IsReady()) {
                    NYX_LOGI("all files downloaded and verified -> READY "
                             "(server %s)",
                             GlobalState().ServerKey().c_str());
                    state = State::READY;
                    lastHello = Clock::now() - std::chrono::hours(1);  // hello at once
                    helloOk = false;
                    // v1.1.0 UI: the success line the user asked for.
                    chatui::Enqueue(chatui::kColorOk,
                                    "[Nyx03DL] Successfully downloaded " +
                                        std::to_string(GlobalState().Models().size()) +
                                        " custom skin(s)!");
                }
            }

            // v1.1.0: while READY, announce presence every ~10 s so the
            // server's Nyx_HasClient() stays true (TTL 60 s server-side).
            if (state == State::READY) {
                auto nowHello = Clock::now();
                if (std::chrono::duration_cast<std::chrono::milliseconds>(nowHello - lastHello)
                        .count() >= 10000) {
                    lastHello = nowHello;
                    bool ok = SendHello(srv, GlobalState().HttpPort());
                    if (ok && !helloOk) {
                        NYX_LOGI("hello: server now knows this client has nyx03dl "
                                 "(downloads ready)");
                    }
                    helloOk = ok;
                }
            }

            // M3: poll /players every ~2 s while on a Nyx server. A
            // failed poll keeps the previous mapping (never wipes it);
            // failures are logged only on transition to avoid spam.
            auto now = Clock::now();
            if (std::chrono::duration_cast<std::chrono::milliseconds>(now - lastPlayersPoll)
                    .count() >= kPlayersPollMs) {
                lastPlayersPoll = now;
                bool ok = PollPlayers(srv, GlobalState().HttpPort());
                if (ok && !playersOk) {
                    NYX_LOGI("players: poll recovered for %s", srv.Key().c_str());
                } else if (!ok && playersOk) {
                    NYX_LOGW("players: poll failed for %s (keeping previous mapping)",
                             srv.Key().c_str());
                }
                playersOk = ok;
            }
        }
    }
}

}  // namespace nyx
