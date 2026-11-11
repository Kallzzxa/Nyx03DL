// raknet.h — minimal CNetGame access for Nyx03DL (polling only, no hooks).
//
// Written for Nyx03DL (M2), from reference/customskin-0.3.7-R5
// vendor/raknet/rakclient.h + src/net/raknet.cpp (GPL-3.0).
//
// v1.2.2: this file NO LONGER CALLS RakClient AT ALL. Two vtable calls
// died on 0.3.7-R1, one after the other:
//   - GetServerID (slot 39) crashed the game right after connect
//     (v1.2.0, fault in ntdll, log stopped before "connected to");
//   - IsConnected (slot 18) never returned true on R1 (v1.2.1 test:
//     game connected and playing, but the log never got past
//     "monitor: started" — the monitor loop logs nothing while it
//     believes it is disconnected).
// Both are replaced by plain reads of CNetGame's own fields
// (m_szHostAddress + m_nPort for the address, m_nGameState for the
// connection state), which cannot dispatch to a wrong function. The
// field offsets are per-build (samp.h) and verified against SAMP-API's
// packed CNetGame layouts + SAMP-UDF's tables. NOTE: GAME_MODE values
// differ per build (R1: CONNECTED=14, WAITJOIN=15; R5-1: 5 and 6).
#pragma once

#include <cstdint>
#include <string>

namespace nyx {
namespace rak {

// Server address as seen by the client.
struct ServerAddress {
    std::string ip;        // dotted quad, e.g. "127.0.0.1"
    uint16_t gamePort = 0; // SA-MP game port, e.g. 7777
    bool valid = false;
    // "ip:gameport" — identity of the current server (used by NyxState).
    std::string Key() const { return ip + ":" + std::to_string(gamePort); }
};

// The live CNetGame* (from the per-build refNetGame pointer), or nullptr
// when no CNetGame exists yet (not connected / menu). Plain read only.
void* NetGame();

// Local playerid (CPlayerPool::m_localInfo.m_nId), or -1 when it cannot
// be determined (no CNetGame, no pools, or this build has no verified
// playerPoolsOff — v1.2.4 rebuild stays off there). Plain reads only;
// the id is sanity-checked against MAX_PLAYERS so a wrong offset can
// never produce a wild id.
int LocalPlayerId();

// CNetGame::m_nGameState, or -1 when there is no CNetGame yet.
int GameState(void* netGame);

// True when the game state is CONNECTED or WAITJOIN for this build.
bool IsConnected(void* netGame);

// Server address of the current connection, read from the CNetGame
// fields (m_szHostAddress + m_nPort, resolved via getaddrinfo so a
// hostname works too). Returns false if it cannot be determined.
bool GetServerAddress(void* netGame, ServerAddress& out);

}  // namespace rak
}  // namespace nyx
