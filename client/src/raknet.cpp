// raknet.cpp — minimal CNetGame access for Nyx03DL (polling only, no hooks).
//
// Written for Nyx03DL (M2), from reference/customskin-0.3.7-R5
// vendor/raknet/rakclient.h + src/net/raknet.cpp (GPL-3.0).
//
// v1.2.2: NO RakClient vtable calls remain (see raknet.h for why).
// Everything here is a plain read of CNetGame fields. The offsets live
// in the per-build table (samp.h) and are verified in samp.cpp against
// SAMP-API's packed CNetGame layouts (R1: m_nGameState at 957, m_pRakClient
// at 969 — the latter confirmed by the actual GetRakClient bytes in a
// user's log) and SAMP-UDF's offset tables.
#include "raknet.h"

#include "samp.h"

#ifdef _WIN32
// winsock2.h MUST come before windows.h.
#include <winsock2.h>
#include <windows.h>
#include <ws2tcpip.h>
#endif

namespace nyx {
namespace rak {

void* NetGame() {
#ifdef _WIN32
    if (!samp::Base()) return nullptr;
    const auto* ver = nyx::samp::ActiveVersion();
    if (!ver) return nullptr;
    return *reinterpret_cast<void**>(samp::Addr(ver->refNetGame));
#else
    return nullptr;  // no samp.dll on Linux (unit harness)
#endif
}

int GameState(void* netGame) {
#ifdef _WIN32
    const auto* ver = nyx::samp::ActiveVersion();
    if (!ver || !netGame) return -1;
    return *reinterpret_cast<const int*>(reinterpret_cast<const char*>(netGame) +
                                         ver->gameStateOff);
#else
    (void)netGame;
    return -1;
#endif
}

bool IsConnected(void* netGame) {
#ifdef _WIN32
    const auto* ver = nyx::samp::ActiveVersion();
    if (!ver || !netGame) return false;
    int state = GameState(netGame);
    return state == ver->stateConnected || state == ver->stateWaitJoin;
#else
    (void)netGame;
    return false;
#endif
}

bool GetServerAddress(void* netGame, ServerAddress& out) {
    out = ServerAddress{};
#ifdef _WIN32
    const auto* ver = nyx::samp::ActiveVersion();
    if (!ver || !netGame) return false;
    // m_szHostAddress: the address string the user connected to (dotted
    // IP or hostname, up to 256 chars + NUL). Read defensively: the
    // pointer comes from samp.dll, so copy byte-by-byte with a cap.
    const char* hostRaw =
        reinterpret_cast<const char*>(netGame) + ver->hostAddressOff;
    char host[257];
    size_t n = 0;
    while (n < sizeof(host) - 1 && hostRaw[n] != '\0') {
        char c = hostRaw[n];
        if (c < 0x20 || c > 0x7E) return false;  // not plausible text
        host[n++] = c;
    }
    host[n] = '\0';
    if (n == 0) return false;
    // m_nPort: int at the packed offset (x86 tolerates the unaligned read).
    int port = *reinterpret_cast<const int*>(reinterpret_cast<const char*>(netGame) +
                                             ver->serverPortOff);
    if (port <= 0 || port > 65535) return false;
    // Resolve the host (works for a dotted IP and for a hostname).
    WSADATA wsa;
    static bool wsaOk = (WSAStartup(MAKEWORD(2, 2), &wsa) == 0);
    if (!wsaOk) return false;
    addrinfo hints{};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    addrinfo* res = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &res) != 0 || !res) return false;
    auto* sin = reinterpret_cast<sockaddr_in*>(res->ai_addr);
    uint32_t binaryAddress = sin->sin_addr.s_addr;  // network byte order
    freeaddrinfo(res);
    if (binaryAddress == 0 || binaryAddress == 0xFFFFFFFFu) return false;
    in_addr addr;
    addr.s_addr = binaryAddress;
    const char* dotted = inet_ntoa(addr);
    if (!dotted) return false;
    out.ip = dotted;
    out.gamePort = static_cast<uint16_t>(port);
    out.valid = true;
    return true;
#else
    (void)netGame;
    return false;
#endif
}

}  // namespace rak
}  // namespace nyx
