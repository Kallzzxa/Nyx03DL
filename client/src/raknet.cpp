// raknet.cpp — minimal RakClient access for Nyx03DL (polling only, no hooks).
//
// Written for Nyx03DL (M2), from reference/customskin-0.3.7-R5
// vendor/raknet/rakclient.h + src/net/raknet.cpp (GPL-3.0).
//
// VTABLE SLOT (MSVC layout, destructor = slot 0, then the virtuals in
// declaration order exactly as written in vendor/raknet/rakclient.h).
// Only IsConnected is called through the vtable now (slot 18). The
// GetServerID call (former slot 39) was REMOVED in v1.2.1: on 0.3.7-R1
// it crashed the game right after connect, with the log stopping before
// "connected to" and the fault landing in ntdll. The server address
// now comes from CNetGame's own fields (see GetServerAddress below),
// which are plain memory reads — they cannot dispatch to a wrong
// function. The field offsets are per-build (samp.h) and verified
// against SAMP-API's packed CNetGame layouts + SAMP-UDF's tables.
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
namespace {

// VTABLE SLOT (counted from vendor/raknet/rakclient.h, see header comment).
// Only IsConnected is still called through the vtable (v1.2.1 removed the
// GetServerID call — see raknet.h). Slot 18 survived many menu polls on
// R1 before the v1.2.0 crash, which happened later in GetServerAddress.
constexpr size_t kVtIsConnected = 18;

#ifdef _WIN32
using GetRakClientFn = void*(__thiscall*)(void*);  // CNetGame::GetRakClient()
using IsConnectedFn = bool(__thiscall*)(void*);     // RakClient::IsConnected()

void* VTableFn(void* rakClient, size_t slot) {
    if (!rakClient) return nullptr;
    void** vtable = *reinterpret_cast<void***>(rakClient);
    if (!vtable) return nullptr;
    // Defensive: the slot must point inside samp.dll's image. A garbage
    // RakClient pointer (or a wrong vtable) must never dispatch anywhere.
    uintptr_t fn = reinterpret_cast<uintptr_t>(vtable[slot]);
    uintptr_t base = samp::Base();
    if (fn < base || fn >= base + samp::ImageSize()) return nullptr;
    return reinterpret_cast<void*>(fn);
}
#endif

}  // namespace

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

void* ResolveRakClient() {
#ifdef _WIN32
    void* netgame = NetGame();
    if (!netgame) return nullptr;
    const auto* ver = nyx::samp::ActiveVersion();
    if (!ver) return nullptr;
    auto fn = reinterpret_cast<GetRakClientFn>(samp::Addr(ver->getRakClient));
    return fn(netgame);
#else
    return nullptr;  // no samp.dll on Linux (unit harness)
#endif
}

bool IsConnected(void* rakClient) {
#ifdef _WIN32
    auto fn = reinterpret_cast<IsConnectedFn>(VTableFn(rakClient, kVtIsConnected));
    if (!fn) return false;
    return fn(rakClient);
#else
    (void)rakClient;
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
