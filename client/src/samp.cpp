// samp.cpp — samp.dll access layer for SA-MP 0.3.7 (R5-1 and R1).
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/core/samp.cpp), GPL-3.0.
// Changes: no urmem, no DebugChat. DumpFingerprint only reads memory (it
// never writes), and is only ever called after the supported-build check.
#include "samp.h"

#include "log.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>

namespace nyx {
namespace samp {

namespace versions {

// R5-1: from the reference repo, cross-checked vs SAMP-API R5-1
// (CNetGame.cpp: Process 0xB5B0, GetRakClient 0xBBC0, RefNetGame
// 0x26EB94; RPCHandlers.cpp: ScrSetPlayerSkin 0x19190; CChat.cpp:
// AddMessage 0x68170, RefChat 0x26EB80).
const VersionAddrs kR5_1 = {
    "0.3.7-R5-1", "CNetGame::Process",
    0x0027E000, 0x6372C39E,
    0x26EB94,  // refNetGame
    0xBBC0,    // getRakClient
    0xB5B0,    // netGameProcess (CNetGame::Process)
    0x19190,   // scrSetPlayerSkin
    0x26EB80,  // refChat
    0x68170,   // chatAddMessage
};

// R1: from SAMP-API multiver, sampapi/0.3.7-R1 — CNetGame.cpp
// (RefNetGame 0x21A0F8, GetRakClient 0x1A40, UpdateNetwork 0xAD70),
// CChat.cpp (RefChat 0x21A0E4, AddMessage 0x645A0). Chat pointer
// cross-checked vs SAMP-UDF (ADDR_SAMP_CHATMSG_PTR[0] = 0x21A0E4).
// Fingerprint from a real user's samp.dll (log 2026-11-10).
// scrSetPlayerSkin == 0: no public R1 address found, NOT guessed.
const VersionAddrs kR1 = {
    "0.3.7-R1", "CNetGame::UpdateNetwork",
    0x00330000, 0x5542F47A,
    0x21A0F8,  // refNetGame
    0x1A40,    // getRakClient
    0xAD70,    // netGameProcess (CNetGame::UpdateNetwork, called per-frame)
    0,         // scrSetPlayerSkin (unknown on R1 — no hook)
    0x21A0E4,  // refChat
    0x645A0,   // chatAddMessage
};

}  // namespace versions

namespace {

uintptr_t g_base = 0;
uint32_t g_imageSize = 0;
uint32_t g_timeDateStamp = 0;
const VersionAddrs* g_active = nullptr;

#ifdef _WIN32
bool ReadPe(uintptr_t base) {
    auto dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE) return false;
    auto nt = reinterpret_cast<const IMAGE_NT_HEADERS*>(base + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE) return false;
    g_imageSize = nt->OptionalHeader.SizeOfImage;
    g_timeDateStamp = nt->FileHeader.TimeDateStamp;
    return true;
}
#endif

}  // namespace

bool Init() {
    if (g_base) return true;
#ifdef _WIN32
    HMODULE h = GetModuleHandleA("samp.dll");
    if (!h) return false;
    auto base = reinterpret_cast<uintptr_t>(h);
    if (!ReadPe(base)) return false;
    g_base = base;
    if (g_imageSize == versions::kR5_1.imageSize &&
        g_timeDateStamp == versions::kR5_1.timeDateStamp) {
        g_active = &versions::kR5_1;
    } else if (g_imageSize == versions::kR1.imageSize &&
               g_timeDateStamp == versions::kR1.timeDateStamp) {
        g_active = &versions::kR1;
    }
    return true;
#else
    return false;  // no samp.dll on Linux (unit harness)
#endif
}

uintptr_t Base() { return g_base; }
uint32_t ImageSize() { return g_imageSize; }
uint32_t TimeDateStamp() { return g_timeDateStamp; }
const VersionAddrs* ActiveVersion() { return g_active; }
bool IsSupportedBuild() { return g_base && g_active != nullptr; }
bool IsR5Build() { return g_active == &versions::kR5_1; }

void DumpFingerprint() {
    if (!g_base) {
        NYX_LOGW("DumpFingerprint: samp.dll not resolved");
        return;
    }
    NYX_LOGI("samp.dll base=0x%08X SizeOfImage=0x%X (%u bytes) TimeDateStamp=0x%08X",
             static_cast<unsigned>(g_base), g_imageSize, g_imageSize, g_timeDateStamp);
    if (g_active) {
        NYX_LOGI("fingerprint matches known %s build", g_active->name);
#ifdef _WIN32
        const auto* p =
            reinterpret_cast<const uint8_t*>(Addr(g_active->getRakClient));
        NYX_LOGI("  @CNetGame::GetRakClient        rva 0x%06X : %02X %02X %02X %02X %02X "
                 "%02X %02X %02X",
                 static_cast<unsigned>(g_active->getRakClient), p[0], p[1], p[2],
                 p[3], p[4], p[5], p[6], p[7]);
        const auto* pp =
            reinterpret_cast<const uint8_t*>(Addr(g_active->netGameProcess));
        NYX_LOGI("  @%s rva 0x%06X : %02X %02X %02X %02X %02X "
                 "%02X %02X %02X",
                 g_active->tickName,
                 static_cast<unsigned>(g_active->netGameProcess), pp[0], pp[1], pp[2],
                 pp[3], pp[4], pp[5], pp[6], pp[7]);
        if (g_active->scrSetPlayerSkin) {
            const auto* ps =
                reinterpret_cast<const uint8_t*>(Addr(g_active->scrSetPlayerSkin));
            NYX_LOGI("  @ScrSetPlayerSkin              rva 0x%06X : %02X %02X %02X %02X %02X "
                     "%02X %02X %02X",
                     static_cast<unsigned>(g_active->scrSetPlayerSkin), ps[0], ps[1], ps[2],
                     ps[3], ps[4], ps[5], ps[6], ps[7]);
        } else {
            NYX_LOGI("  @ScrSetPlayerSkin              none on this build (no hook; "
                     "repeat /setskin once after the swap)");
        }
        NYX_LOGI("  RefNetGame=0x%08X",
                 *reinterpret_cast<uint32_t*>(Addr(g_active->refNetGame)));
#endif
    } else {
        NYX_LOGW("fingerprint MISMATCH: known builds are R5-1 "
                 "(SizeOfImage=0x%X TimeDateStamp=0x%08X) and R1 "
                 "(SizeOfImage=0x%X TimeDateStamp=0x%08X); RVA table is unsafe "
                 "on this build, monitor will exit",
                 versions::kR5_1.imageSize, versions::kR5_1.timeDateStamp,
                 versions::kR1.imageSize, versions::kR1.timeDateStamp);
    }
}

}  // namespace samp
}  // namespace nyx
