// dllmain.cpp — nyx03dl.asi entry point (M2, extended in M3).
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/dllmain.cpp), GPL-3.0.
// M2 is deliberately smaller than the reference: no hooks, no masquerade,
// no renderer — just LogInit, wait for samp.dll, verify the build fingerprint,
// and hand over to the monitor thread. If samp.dll never appears (plain
// single-player session) or the fingerprint does not match, the thread
// exits quietly and the game runs exactly as before.
//
// M3: after the build fingerprint passes, the clump-swap renderer and the
// skin-change hook are installed (both build-gated again inside, and
// idempotent). If a hook fails to install, M3 stays off but M2 keeps
// working (probe/download/cache); the failure is logged, never fatal.
#include <windows.h>

#include "game/render.h"
#include "log.h"
#include "samp.h"
#include "monitor.h"
#include "skinhook.h"

namespace {

// Wait up to timeoutMs for a module; also our "is this a SA-MP session?"
// check (samp.dll never loads in single player).
HMODULE WaitForModule(const char* name, DWORD timeoutMs) {
    const DWORD step = 100;
    DWORD waited = 0;
    for (;;) {
        if (HMODULE h = GetModuleHandleA(name)) return h;
        if (waited >= timeoutMs) return nullptr;
        Sleep(step);
        waited += step;
    }
}

DWORD WINAPI MonitorThread(LPVOID) {
    nyx::log::Init();
    NYX_LOGI("nyx03dl loading (M3: manifest probe + download + cache + clump swap)");

    if (!WaitForModule("samp.dll", 60000)) {
        NYX_LOGW("samp.dll not present after 60s; not a SA-MP session, exiting");
        return 0;
    }
    Sleep(500);  // let samp.dll finish its own startup after the module maps in

    if (!nyx::samp::Init()) {
        NYX_LOGE("failed to resolve samp.dll base");
        return 0;
    }
    nyx::samp::DumpFingerprint();
    NYX_LOGI("samp.dll resolved @ 0x%08X",
             static_cast<unsigned>(nyx::samp::Base()));

    if (!nyx::samp::IsSupportedBuild()) {
        // The RVA table is per-build (R5-1 and R1). Reading those
        // addresses in any other build could crash the game, so we
        // refuse to run at all.
        NYX_LOGW("samp.dll is NOT a supported build (0.3.7-R5-1 or 0.3.7-R1); "
                 "nyx03dl will not run (no hooks or reads applied)");
        return 0;
    }

    NYX_LOGI("%s fingerprint OK; installing M3 hooks (clump swap + skin-change)",
             nyx::samp::ActiveVersion()->name);
    // M3 hooks are installed once, from this thread, right after the
    // gate. A failure is logged and non-fatal: M2 (probe/download/cache)
    // keeps working, skins just won't be applied.
    if (render::Init())
        NYX_LOGI("M3 renderer on (CNetGame::Process per-frame tick)");
    else
        NYX_LOGW("M3 renderer OFF (hook install failed); downloads continue, "
                 "skins will not be applied");
    if (skinhook::Init())
        NYX_LOGI("M3 skin-change hook on (ScrSetPlayerSkin pre-swap)");
    else
        NYX_LOGW("M3 skin-change hook OFF; skins still apply via per-frame Tick");

    NYX_LOGI("monitor starting (polling + M3 hooks)");
    nyx::MonitorLoop();  // never returns (until the process exits)
    return 0;
}

}  // namespace

BOOL APIENTRY DllMain(HINSTANCE hinst, DWORD reason, LPVOID) {
    if (reason == DLL_PROCESS_ATTACH) {
        DisableThreadLibraryCalls(hinst);
        CreateThread(nullptr, 0, MonitorThread, nullptr, 0, nullptr);
    }
    return TRUE;
}
