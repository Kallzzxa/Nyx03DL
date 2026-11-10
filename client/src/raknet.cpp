// raknet.cpp — minimal RakClient access for Nyx03DL (polling only, no hooks).
//
// Written for Nyx03DL (M2), from reference/customskin-0.3.7-R5
// vendor/raknet/rakclient.h + src/net/raknet.cpp (GPL-3.0).
//
// VTABLE SLOT COUNT (MSVC layout, destructor = slot 0, then the virtuals
// in declaration order exactly as written in vendor/raknet/rakclient.h):
//   0  ~RakClientInterface (destructor — MSVC emits ONE destructor slot)
//   1  Connect
//   2  Disconnect
//   3  InitializeSecurity
//   4  SetPassword
//   5  HasPassword
//   6  Send(const char*, ...)
//   7  Send(BitStream*, ...)
//   8  Receive
//   9  DeallocatePacket
//   10 PingServer()
//   11 PingServer(host, ...)
//   12 GetAveragePing
//   13 GetLastPing
//   14 GetLowestPing
//   15 GetPlayerPing
//   16 StartOccasionalPing
//   17 StopOccasionalPing
//   18 IsConnected            <-- used
//   19 GetSynchronizedRandomInteger
//   20 GenerateCompressionLayer
//   21 DeleteCompressionLayer
//   22 RegisterAsRemoteProcedureCall
//   23 RegisterClassMemberRPC
//   24 UnregisterAsRemoteProcedureCall
//   25 RPC(const char*, ...)
//   26 RPC(BitStream*, ...)
//   27 RPC_
//   28 SetTrackFrequencyTable
//   29 GetSendFrequencyTable
//   30 GetCompressionRatio
//   31 GetDecompressionRatio
//   32 AttachPlugin
//   33 DetachPlugin
//   34 GetStaticServerData
//   35 SetStaticServerData
//   36 GetStaticClientData
//   37 SetStaticClientData
//   38 SendStaticClientDataToServer
//   39 GetServerID            <-- used
//   40 GetPlayerID
//   41 GetInternalID
//   42 PlayerIDToDottedIP
//   ... (later slots unused by M2)
//
// BYTE-ORDER ASSUMPTIONS (see README for the full discussion):
//  * PlayerID.binaryAddress is in NETWORK byte order (it comes from
//    inet_addr / the wire), so it can be handed to inet_ntoa() as-is.
//  * PlayerID.port is in HOST byte order (RakNet stores the port
//    host-order in PlayerID; SA-MP itself prints GetServerID().port
//    directly as the server port). If this assumption were wrong, the
//    manifest probe would simply fail and the monitor would SLEEP —
//    never a crash — and the log would show the wrong port.
//  * PlayerID is #pragma pack(1) in rakclient.h: 4-byte binaryAddress +
//    2-byte port = 6 bytes. MSVC returns a <=8-byte struct in EAX:EDX,
//    so GetServerID is called here as returning uint64_t and the low
//    48 bits are decoded; the top 16 bits are ignored.
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

// Vtable slots (counted from vendor/raknet/rakclient.h, see header comment).
constexpr size_t kVtIsConnected = 18;
constexpr size_t kVtGetServerID = 39;

#ifdef _WIN32
using GetRakClientFn = void*(__thiscall*)(void*);  // CNetGame::GetRakClient()
using IsConnectedFn = bool(__thiscall*)(void*);     // RakClient::IsConnected()
using GetServerIDFn = uint64_t(__thiscall*)(void*); // RakClient::GetServerID()

void* VTableFn(void* rakClient, size_t slot) {
    if (!rakClient) return nullptr;
    void** vtable = *reinterpret_cast<void***>(rakClient);
    if (!vtable) return nullptr;
    return vtable[slot];
}
#endif

}  // namespace

void* ResolveRakClient() {
#ifdef _WIN32
    if (!samp::Base()) return nullptr;
    const auto* ver = nyx::samp::ActiveVersion();
    if (!ver) return nullptr;
    void* netgame = *reinterpret_cast<void**>(samp::Addr(ver->refNetGame));
    if (!netgame) return nullptr;
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

bool GetServerAddress(void* rakClient, ServerAddress& out) {
    out = ServerAddress{};
#ifdef _WIN32
    auto fn = reinterpret_cast<GetServerIDFn>(VTableFn(rakClient, kVtGetServerID));
    if (!fn) return false;
    uint64_t raw = fn(rakClient);
    // Low 32 bits: binaryAddress (network byte order). Bits 32..47: port
    // (host byte order, see header comment). Bits 48..63: padding, ignored.
    uint32_t binaryAddress = static_cast<uint32_t>(raw & 0xFFFFFFFFull);
    uint16_t port = static_cast<uint16_t>((raw >> 32) & 0xFFFFu);
    if (binaryAddress == 0 || binaryAddress == 0xFFFFFFFFu || port == 0) return false;
    in_addr addr;
    addr.s_addr = binaryAddress;  // already network byte order
    const char* dotted = inet_ntoa(addr);
    if (!dotted) return false;
    out.ip = dotted;
    out.gamePort = port;
    out.valid = true;
    return true;
#else
    (void)rakClient;
    return false;
#endif
}

}  // namespace rak
}  // namespace nyx
