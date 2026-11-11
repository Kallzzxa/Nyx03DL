// handling.cpp — runtime handling patches for replaced vehicles (v1.3.0).
//
// See handling.h for the layout sources and the lookup rules.
// GAME-THREAD ONLY (writes game memory).
#include "game/handling.h"

#include <cstdint>
#include <map>

#include "log.h"

#ifdef _WIN32
#include <windows.h>
#endif

namespace handling {
namespace {

// GTA 1.0 US absolute address of gHandlingDataMgr (plugin-sdk
// cHandlingDataMgr.cpp). The .asi rebases it with the exe base delta,
// exactly like the other addresses in gta.h.
constexpr uintptr_t kMgrAddr = 0xC2B9C8;
constexpr uintptr_t kExeBase = 0x400000;

// cHandlingDataMgr layout (plugin-sdk cHandlingDataMgr.h).
constexpr int kEntryCount = 210;
constexpr int kEntrySize = 0xE0;
constexpr int kArrayOff = 0x14;

// tHandlingData offsets (plugin-sdk tHandlingData.h).
constexpr int kVehicleIdOff = 0x0;
constexpr int kTransmissionOff = 0x2C;
constexpr int kBrakeOff = 0x94;
// cTransmission offsets, relative to the transmission block
// (plugin-sdk cTransmission.h).
constexpr int kAccelOff = 0x50;
constexpr int kMaxSpeedOff = 0x58;

// CVehicleModelInfo::m_nHandlingId (plugin-sdk CVehicleModelInfo.h).
constexpr int kHandlingIdOff = 0x4A;

// cfg km/h -> world units (same factor the game's own
// ConvertDataToWorldUnits applies when loading handling.cfg).
constexpr float kSpeedToWorld = 0.5f;

struct Values {
    float maxspeed = 0;
    float accel = 0;
    float brake = 0;
};

std::map<int, void*> g_entries;    // vehid -> handling entry (cached)
std::map<int, Values> g_originals; // vehid -> values before first patch
std::map<int, Values> g_applied;   // vehid -> values last written
std::map<int, bool> g_noEntry;     // vehid -> lookup failed once (log once)

uintptr_t MgrAddress() {
#ifdef _WIN32
    static uintptr_t delta =
        reinterpret_cast<uintptr_t>(GetModuleHandleA(nullptr)) - kExeBase;
    return kMgrAddr + delta;
#else
    return 0;
#endif
}

}  // namespace

void* FindEntry(void* mgr, void* modelInfo, int vehid) {
    if (!mgr || !modelInfo) return nullptr;
    unsigned short hid =
        *reinterpret_cast<unsigned short*>(static_cast<char*>(modelInfo) +
                                           kHandlingIdOff);
    if (hid >= kEntryCount) return nullptr;
    char* entry = static_cast<char*>(mgr) + kArrayOff + hid * kEntrySize;
    if (*reinterpret_cast<int*>(entry + kVehicleIdOff) != vehid)
        return nullptr;  // not a car (bike/boat/plane: different array)
    return entry;
}

void Apply(int vehid, void* modelInfo, float maxspeed, float accel,
           float brake) {
    if (vehid < 400 || vehid > 611) return;
    // All-zero with nothing ever applied: nothing to do. All-zero
    // WITH a previous patch falls through to restore the stock values
    // field by field (see the write blocks below).
    if (maxspeed == 0 && accel == 0 && brake == 0 &&
        g_applied.find(vehid) == g_applied.end())
        return;

    void* entry = nullptr;
    std::map<int, void*>::iterator it = g_entries.find(vehid);
    if (it != g_entries.end()) {
        entry = it->second;
    } else {
        void* mgr = reinterpret_cast<void*>(MgrAddress());
        entry = FindEntry(mgr, modelInfo, vehid);
        if (!entry) {
            // Log once: Tick calls Apply every frame, and a bike/boat/
            // plane (or a bad model) would spam the log otherwise.
            if (!g_noEntry[vehid]) {
                g_noEntry[vehid] = true;
                NYX_LOGW("handling: no car handling entry for vehicle %d "
                         "(bike/boat/plane or bad model?)",
                         vehid);
            }
            return;
        }
        g_entries[vehid] = entry;
        // Record the stock values before the first patch so a later
        // RestoreAll() can put them back.
        Values orig;
        char* e = static_cast<char*>(entry);
        char* tr = e + kTransmissionOff;
        orig.maxspeed = *reinterpret_cast<float*>(tr + kMaxSpeedOff) /
                        kSpeedToWorld;
        orig.accel = *reinterpret_cast<float*>(tr + kAccelOff);
        orig.brake = *reinterpret_cast<float*>(e + kBrakeOff);
        g_originals[vehid] = orig;
    }

    char* e = static_cast<char*>(entry);
    char* tr = e + kTransmissionOff;
    Values& applied = g_applied[vehid];
    Values& orig = g_originals[vehid];  // recorded on first patch
    // A field set back to 0 restores its stock value (so
    // NyxSetVehHandling(v, 0, 0, 0) is a real reset).
    if (maxspeed != applied.maxspeed) {
        float world = (maxspeed == 0 ? orig.maxspeed : maxspeed) *
                      kSpeedToWorld;
        *reinterpret_cast<float*>(tr + kMaxSpeedOff) = world;
        applied.maxspeed = maxspeed;
        if (maxspeed == 0)
            NYX_LOGI("handling: vehicle %d maxspeed restored to stock",
                     vehid);
        else
            NYX_LOGI("handling: vehicle %d maxspeed %.1f km/h -> %.1f (world)",
                     vehid, static_cast<double>(maxspeed),
                     static_cast<double>(world));
    }
    if (accel != applied.accel) {
        *reinterpret_cast<float*>(tr + kAccelOff) =
            (accel == 0 ? orig.accel : accel);
        applied.accel = accel;
        if (accel == 0)
            NYX_LOGI("handling: vehicle %d accel restored to stock", vehid);
        else
            NYX_LOGI("handling: vehicle %d accel %.2f", vehid,
                     static_cast<double>(accel));
    }
    if (brake != applied.brake) {
        *reinterpret_cast<float*>(e + kBrakeOff) =
            (brake == 0 ? orig.brake : brake);
        applied.brake = brake;
        if (brake == 0)
            NYX_LOGI("handling: vehicle %d brake restored to stock", vehid);
        else
            NYX_LOGI("handling: vehicle %d brake %.2f", vehid,
                     static_cast<double>(brake));
    }
}

void RestoreAll() {
    for (std::map<int, Values>::iterator it = g_originals.begin();
         it != g_originals.end(); ++it) {
        int vehid = it->first;
        Values orig = it->second;
        std::map<int, void*>::iterator eit = g_entries.find(vehid);
        if (eit == g_entries.end()) continue;
        char* e = static_cast<char*>(eit->second);
        char* tr = e + kTransmissionOff;
        *reinterpret_cast<float*>(tr + kMaxSpeedOff) =
            orig.maxspeed * kSpeedToWorld;
        *reinterpret_cast<float*>(tr + kAccelOff) = orig.accel;
        *reinterpret_cast<float*>(e + kBrakeOff) = orig.brake;
        NYX_LOGI("handling: vehicle %d handling restored to stock", vehid);
    }
    g_entries.clear();
    g_originals.clear();
    g_applied.clear();
    g_noEntry.clear();
}

}  // namespace handling
