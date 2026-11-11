// rpcprobe.cpp — diagnostic .asi: finds the TRUE ScrSetPlayerSkin address
// for SA-MP 0.3.7 R1 by logging, instead of guessing it from static analysis.
//
// Background: static analysis (objdump + Ghidra) could NOT identify the
// SetPlayerSkin RPC handler in R1's samp.dll with hookable confidence.
// The R1 binary's RPC registration lives in obfuscated `.poop` fragments
// (control-flow flattening, decoy writes), so the closest static candidate
// (RVA 0x15860) is only ~40-50% certain — hooking it would crash the game
// on every /setskin if wrong. What static analysis DID verify:
//   * RPC id for SetPlayerSkin = 153 (open.mp Shared/NetCode/core.hpp AND
//     the R1 binary's own RPC id table at VA 0x100D3F98, entry 0x100D4024).
//   * RegisterAsRemoteProcedureCall's real body is at RVA 0x34B00: it reads
//     the low byte of *uniqueID and calls internal 0x1004DBC0. It is reached
//     via the game-facing RakClient vtable (object+0xDDE), slot 22, through
//     a `sub $0xdde,%ecx; jmp 0x10034B00` thunk.
// This probe hooks that ONE function (RVA 0x34B00) and logs every
// (id, handler) pair as CNetGame registers RPCs at connect. The log then
// names the true handler RVA for id 153 directly — no guessing.
//
// SAFETY: the hook is installed ONLY when the samp.dll fingerprint matches
// 0.3.7 R1 exactly (SizeOfImage 0x330000, TimeDateStamp 0x5542F47A), and
// only when the first 6 bytes at 0x34B00 match the disassembly of the
// user's own binary (8B 44 24 04 85 C0). Any other build: the probe exits
// quietly and touches nothing. The probe writes ONLY rpcprobe.log and the
// 5-byte hook patch; it never reads or writes game state.
//
// Usage: copy rpcprobe.asi next to gta_sa.exe (same ASI loader as
// nyx03dl.asi), connect to the server once, quit, and send rpcprobe.log.
// The line `rpc register: id=153 handler=... (RVA 0x.....)` is the answer.
//
// GPL-3.0 (Nyx03DL project).
#include <windows.h>

#include <urmem.hpp>

#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <mutex>

// ---- tiny file logger (rpcprobe.log next to gta_sa.exe) -------------------
namespace {

std::mutex g_logMutex;
char g_logPath[MAX_PATH] = {0};
bool g_logReady = false;

void LogInit() {
    char exe[MAX_PATH] = {0};
    GetModuleFileNameA(nullptr, exe, MAX_PATH);
    char* slash = strrchr(exe, '\\');
    size_t dirLen = slash ? static_cast<size_t>(slash - exe + 1) : 0;
    snprintf(g_logPath, sizeof(g_logPath), "%.*s%s", static_cast<int>(dirLen),
             exe, "rpcprobe.log");
    FILE* f = fopen(g_logPath, "w");
    if (f) {
        fprintf(f, "==== rpcprobe log opened ====\n");
        fclose(f);
        g_logReady = true;
    }
}

void LogLine(const char* fmt, ...) {
    if (!g_logReady) return;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    std::lock_guard<std::mutex> lock(g_logMutex);
    FILE* f = fopen(g_logPath, "a");
    if (!f) return;
    SYSTEMTIME st;
    GetLocalTime(&st);
    fprintf(f, "[%02d:%02d:%02d.%03d] %s\n", st.wHour, st.wMinute, st.wSecond,
            st.wMilliseconds, buf);
    fclose(f);
}

}  // namespace

// ---- R1 fingerprint -------------------------------------------------------
namespace {

constexpr uint32_t kR1ImageSize = 0x330000;
constexpr uint32_t kR1TimeDateStamp = 0x5542F47A;
// RegisterAsRemoteProcedureCall body, RVA (verified in the user's binary).
constexpr uintptr_t kRegisterRpcRva = 0x34B00;
// First 6 bytes at 0x34B00 in the user's binary:
//   8B 44 24 04  mov eax,[esp+4]   (uniqueID)
//   85 C0       test eax,eax
constexpr uint8_t kRegisterRpcHead[6] = {0x8B, 0x44, 0x24, 0x04, 0x85, 0xC0};

uintptr_t g_sampBase = 0;

bool CheckR1Fingerprint(HMODULE samp) {
    auto* base = reinterpret_cast<uint8_t*>(samp);
    if (!base) return false;
    uint32_t peOff = *reinterpret_cast<uint32_t*>(base + 0x3C);
    if (peOff > 0x400 || memcmp(base + peOff, "PE\0\0", 4) != 0) return false;
    uint32_t timeDateStamp =
        *reinterpret_cast<uint32_t*>(base + peOff + 8);
    uint16_t optMagic =
        *reinterpret_cast<uint16_t*>(base + peOff + 24);
    if (optMagic != 0x10B) return false;  // PE32
    uint32_t sizeOfImage =
        *reinterpret_cast<uint32_t*>(base + peOff + 24 + 56);
    LogLine("rpcprobe: samp.dll base=0x%08X SizeOfImage=0x%X TimeDateStamp=0x%X",
            static_cast<unsigned>(reinterpret_cast<uintptr_t>(samp)),
            static_cast<unsigned>(sizeOfImage),
            static_cast<unsigned>(timeDateStamp));
    return sizeOfImage == kR1ImageSize && timeDateStamp == kR1TimeDateStamp;
}

}  // namespace

// ---- the hook -------------------------------------------------------------
// Original at 0x34B00 is effectively __thiscall: ecx = RakClient (already
// adjusted back by the `sub $0xdde,%ecx` thunk), args (uniqueID, handler)
// on the stack, callee cleans 8 bytes. Mirrors render.cpp's Hooked_Process:
// __fastcall takes `self` in ecx (plus an unused edx slot) and the two args
// on the stack, then forwards to the original with urmem's thiscall invoker.
namespace {

urmem::hook g_regHook;
bool g_seen[256] = {false};

void __fastcall Hooked_RegisterRpc(void* self, void* /*edx*/, int* uniqueID,
                                   void (*handler)(void*)) {
    int id = uniqueID ? (*uniqueID & 0xFF) : -1;
    uintptr_t handlerAddr = reinterpret_cast<uintptr_t>(handler);
    uintptr_t rva = handlerAddr - g_sampBase;
    if (id >= 0 && id < 256 && !g_seen[id]) {
        g_seen[id] = true;
        LogLine("rpc register: id=%d handler=0x%08X (RVA 0x%05X)",
                id, static_cast<unsigned>(handlerAddr),
                static_cast<unsigned>(rva));
        if (id == 153) {
            LogLine("rpcprobe: *** id 153 (SetPlayerSkin) handler RVA = 0x%05X ***",
                    static_cast<unsigned>(rva));
        }
    }
    g_regHook.call<urmem::calling_convention::thiscall, void>(self, uniqueID,
                                                              handler);
}

HMODULE WaitForModule(const char* name, DWORD timeoutMs) {
    DWORD waited = 0;
    for (;;) {
        if (HMODULE h = GetModuleHandleA(name)) return h;
        if (waited >= timeoutMs) return nullptr;
        Sleep(100);
        waited += 100;
    }
}

DWORD WINAPI ProbeThread(LPVOID) {
    LogInit();
    LogLine("rpcprobe loading (diagnostic: log RPC registrations, find ScrSetPlayerSkin R1)");

    HMODULE samp = WaitForModule("samp.dll", 60000);
    if (!samp) {
        LogLine("rpcprobe: samp.dll not present after 60s; exiting");
        return 0;
    }
    Sleep(500);  // let samp.dll finish its own startup
    g_sampBase = reinterpret_cast<uintptr_t>(samp);

    if (!CheckR1Fingerprint(samp)) {
        LogLine("rpcprobe: samp.dll is NOT 0.3.7-R1; refusing to hook, exiting");
        return 0;
    }
    LogLine("rpcprobe: 0.3.7-R1 fingerprint OK");

    uintptr_t target = g_sampBase + kRegisterRpcRva;
    if (memcmp(reinterpret_cast<void*>(target), kRegisterRpcHead,
               sizeof(kRegisterRpcHead)) != 0) {
        LogLine("rpcprobe: bytes at 0x34B00 do not match the expected "
                "RegisterAsRemoteProcedureCall head; refusing to hook, exiting");
        return 0;
    }
    g_regHook.install(target, urmem::get_func_addr(&Hooked_RegisterRpc),
                      urmem::hook::type::jmp);
    LogLine("rpcprobe: hooked RegisterAsRemoteProcedureCall @ 0x%08X "
            "(RVA 0x34B00); connect to the server once, then send rpcprobe.log",
            static_cast<unsigned>(target));
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HMODULE hModule, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hModule);
        CreateThread(nullptr, 0, ProbeThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
