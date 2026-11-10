// skinload.cpp — ready-model manager for Nyx03DL (M3).
//
// Written for Nyx03DL (M3). Lazy-loads custom skins from the M2 cache
// only once a player wears them; repairs streamer-destroyed clumps
// without double-freeing; frees skins nobody wears.
//
// GAME-THREAD ONLY (see skinload.h).
#include "skinload.h"

#include "log.h"

#include <vector>

namespace skinload {
namespace {

std::vector<Entry> g_entries;

Entry* Find(int skinid) {
    for (auto& e : g_entries)
        if (e.skinid == skinid) return &e;
    return nullptr;
}

}  // namespace

bool EnsureLoaded(const nyx::Model& m) {
    Entry* e = Find(m.skinid);
    if (!e) {
        g_entries.push_back(Entry{});
        e = &g_entries.back();
        e->skinid = m.skinid;
        e->baseid = m.baseid;
        e->dffPath = m.dffPath;
        e->txdPath = m.txdPath;
    }
    if (e->loaded.ok()) return true;
    if (!m.dffReady || !m.txdReady) {
        NYX_LOGW("skinload: skin %d not cache-ready (dff %d txd %d), cannot load",
                 m.skinid, (int)m.dffReady, (int)m.txdReady);
        return false;
    }
    if (e->loaded.txd && !e->loaded.clump) {
        // Half-loaded (clump lost to the streamer, txd survived): repair
        // in place instead of re-reading the txd.
        if (rwmodel::ReloadClump(e->loaded, e->dffPath.c_str())) {
            NYX_LOGI("skinload: skin %d clump repaired from cache", m.skinid);
            return true;
        }
        NYX_LOGW("skinload: skin %d clump repair failed, will retry full load",
                 m.skinid);
        rwmodel::Free(e->loaded);
    } else if (e->loaded.clump || e->loaded.txd) {
        // Unexpected half state (clump without txd): drop it, load fresh.
        rwmodel::Free(e->loaded);
    }
    e->loaded = rwmodel::LoadModel(e->dffPath.c_str(), e->txdPath.c_str());
    if (e->loaded.ok()) {
        NYX_LOGI("skinload: skin %d RenderWare-loaded from cache (base %d)",
                 m.skinid, m.baseid);
        return true;
    }
    NYX_LOGW("skinload: skin %d RW-load incomplete (clump=%p txd=%p) - bad file?",
             m.skinid, e->loaded.clump, e->loaded.txd);
    return false;
}

bool GetReady(int skinid, int& baseidOut, void*& clumpOut) {
    Entry* e = Find(skinid);
    if (!e || !e->loaded.ok()) return false;
    baseidOut = e->baseid;
    clumpOut = e->loaded.clump;
    return true;
}

void ForEachReady(ReadyVisitor visit, void* ctx) {
    if (!visit) return;
    for (auto& e : g_entries) {
        if (!e.loaded.ok()) continue;
        visit(e.skinid, e.baseid, e.loaded.clump, ctx);
    }
}

void OnClumpLost(void* clump, bool reload) {
    for (auto& e : g_entries) {
        if (e.loaded.clump != clump) continue;
        e.loaded.clump = nullptr;  // streamer already destroyed it — don't re-free
        if (reload) {
            if (rwmodel::ReloadClump(e.loaded, e.dffPath.c_str()))
                NYX_LOGI("skinload: skin %d clump unloaded by streamer -> reloaded %p",
                         e.skinid, e.loaded.clump);
            else
                NYX_LOGW("skinload: skin %d clump lost, reload failed "
                         "(reverts to base until reconnect)",
                         e.skinid);
        } else {
            NYX_LOGI("skinload: skin %d clump abandoned (teardown)", e.skinid);
        }
        return;
    }
}

void FreeSkin(int skinid) {
    for (auto it = g_entries.begin(); it != g_entries.end(); ++it) {
        if (it->skinid != skinid) continue;
        rwmodel::Free(it->loaded);
        g_entries.erase(it);
        NYX_LOGI("skinload: skin %d freed", skinid);
        return;
    }
}

void FreeAll() {
    for (auto& e : g_entries) rwmodel::Free(e.loaded);
    if (!g_entries.empty())
        NYX_LOGI("skinload: freed %zu loaded skin(s)", g_entries.size());
    g_entries.clear();
}

void FreeUnassigned() {
    for (auto it = g_entries.begin(); it != g_entries.end();) {
        if (nyx::GlobalState().IsCustomAssigned(it->skinid)) {
            ++it;
            continue;
        }
        rwmodel::Free(it->loaded);
        NYX_LOGI("skinload: skin %d freed (no player wears it anymore)", it->skinid);
        it = g_entries.erase(it);
    }
}

}  // namespace skinload
