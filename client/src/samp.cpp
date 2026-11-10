// samp.cpp — samp.dll access layer for SA-MP 0.3.7-R5-1.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/core/samp.cpp), GPL-3.0.
// Changes: no urmem, no DebugChat. DumpFingerprint only reads memory (it
// never writes), and is only ever called after the R5 check in the monitor.
#include "samp.h"

#include "log.h"

#ifdef _WIN32
#include <windows.h>
#endif

#include <cstdio>

namespace nyx {
namespace samp {
namespace {

uintptr_t g_base = 0;
uint32_t g_imageSize = 0;
uint32_t g_timeDateStamp = 0;

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
    return true;
#else
    return false;  // no samp.dll on Linux (unit harness)
#endif
}

uintptr_t Base() { return g_base; }
uint32_t ImageSize() { return g_imageSize; }
uint32_t TimeDateStamp() { return g_timeDateStamp; }

bool IsR5Build() {
    return g_base && g_imageSize == r5::kImageSize &&
           g_timeDateStamp == r5::kTimeDateStamp;
}

void DumpFingerprint() {
    if (!g_base) {
        NYX_LOGW("DumpFingerprint: samp.dll not resolved");
        return;
    }
    NYX_LOGI("samp.dll base=0x%08X SizeOfImage=0x%X (%u bytes) TimeDateStamp=0x%08X",
             static_cast<unsigned>(g_base), g_imageSize, g_imageSize, g_timeDateStamp);
    if (IsR5Build()) {
        NYX_LOGI("fingerprint matches known 0.3.7-R5-1 build");
#ifdef _WIN32
        const auto* p =
            reinterpret_cast<const uint8_t*>(Addr(r5::kCNetGame_GetRakClient));
        NYX_LOGI("  @CNetGame::GetRakClient        rva 0x%06X : %02X %02X %02X %02X %02X "
                 "%02X %02X %02X",
                 static_cast<unsigned>(r5::kCNetGame_GetRakClient), p[0], p[1], p[2],
                 p[3], p[4], p[5], p[6], p[7]);
        const auto* pp =
            reinterpret_cast<const uint8_t*>(Addr(r5::kCNetGame_Process));
        NYX_LOGI("  @CNetGame::Process             rva 0x%06X : %02X %02X %02X %02X %02X "
                 "%02X %02X %02X",
                 static_cast<unsigned>(r5::kCNetGame_Process), pp[0], pp[1], pp[2],
                 pp[3], pp[4], pp[5], pp[6], pp[7]);
        const auto* ps =
            reinterpret_cast<const uint8_t*>(Addr(r5::kScrSetPlayerSkin));
        NYX_LOGI("  @ScrSetPlayerSkin              rva 0x%06X : %02X %02X %02X %02X %02X "
                 "%02X %02X %02X",
                 static_cast<unsigned>(r5::kScrSetPlayerSkin), ps[0], ps[1], ps[2],
                 ps[3], ps[4], ps[5], ps[6], ps[7]);
        NYX_LOGI("  RefNetGame=0x%08X",
                 *reinterpret_cast<uint32_t*>(Addr(r5::kRefNetGame)));
#endif
    } else {
        NYX_LOGW("fingerprint MISMATCH: expected SizeOfImage=0x%X TimeDateStamp=0x%08X; "
                 "RVA table is unsafe on this build, monitor will exit",
                 r5::kImageSize, r5::kTimeDateStamp);
    }
}

}  // namespace samp
}  // namespace nyx
