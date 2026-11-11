// nyx_state.cpp — shared state between the M2 monitor and the M3 renderer.
//
// Written for Nyx03DL (M2), extended in M3 (player mapping + restore flag).
#include "nyx_state.h"

#include "cache.h"

namespace nyx {

void NyxState::SetManifest(const std::vector<Model>& models,
                           const std::string& serverKey, uint16_t httpPort) {
    std::lock_guard<std::mutex> lk(mtx_);
    // v1.3.0: the manifest carries skins only; keep any vehicle entries
    // already in the list (SetVehicles owns them).
    std::vector<Model> kept;
    for (const auto& m : models_)
        if (m.isVehicle) kept.push_back(m);
    models_ = models;
    for (const auto& m : kept) models_.push_back(m);
    serverKey_ = serverKey;
    httpPort_ = httpPort;
    for (auto& m : models_) {
        m.dffPath = cache::PathFor(m.dffCrc, true);
        m.txdPath = cache::PathFor(m.txdCrc, false);
        m.dffReady = cache::HasValid(m.dffCrc, true);
        m.txdReady = cache::HasValid(m.txdCrc, false);
    }
}

void NyxState::SetVehicles(const std::vector<Model>& vehicles) {
    std::lock_guard<std::mutex> lk(mtx_);
    std::vector<Model> kept;
    for (const auto& m : models_)
        if (!m.isVehicle) kept.push_back(m);
    models_ = kept;
    for (const auto& v : vehicles) models_.push_back(v);
    for (auto& m : models_) {
        if (!m.isVehicle) continue;
        m.dffPath = cache::PathFor(m.dffCrc, true);
        m.txdPath = cache::PathFor(m.txdCrc, false);
        m.dffReady = cache::HasValid(m.dffCrc, true);
        m.txdReady = cache::HasValid(m.txdCrc, false);
    }
}

void NyxState::Clear() {
    std::lock_guard<std::mutex> lk(mtx_);
    models_.clear();
    serverKey_.clear();
    httpPort_ = 0;
    playerSkins_.clear();
    // The game thread must restore swapped templates + free loaded skins
    // on its next Tick (this flag is the only cross-thread signal into
    // the renderer — the monitor never writes game memory itself).
    restoreRequested_ = true;
}

std::vector<Model> NyxState::Models() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return models_;
}

bool NyxState::IsReady() const {
    std::lock_guard<std::mutex> lk(mtx_);
    if (models_.empty()) return false;
    for (const auto& m : models_)
        if (!m.dffReady || !m.txdReady) return false;
    return true;
}

bool NyxState::HasManifest() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return !models_.empty();
}

std::string NyxState::ServerKey() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return serverKey_;
}

uint16_t NyxState::HttpPort() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return httpPort_;
}

void NyxState::MarkDownloaded(int skinid, bool isDff, bool ok) {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& m : models_) {
        if (m.skinid != skinid) continue;
        if (isDff) {
            m.dffReady = ok;
            if (ok) m.dffPath = cache::PathFor(m.dffCrc, true);
        } else {
            m.txdReady = ok;
            if (ok) m.txdPath = cache::PathFor(m.txdCrc, false);
        }
    }
}

void NyxState::UpdateFromCache() {
    std::lock_guard<std::mutex> lk(mtx_);
    for (auto& m : models_) {
        m.dffReady = cache::HasValid(m.dffCrc, true);
        m.txdReady = cache::HasValid(m.txdCrc, false);
    }
}

void NyxState::SetPlayerSkins(const std::map<int, int>& skins) {
    std::lock_guard<std::mutex> lk(mtx_);
    playerSkins_ = skins;
}

std::map<int, int> NyxState::PlayerSkins() const {
    std::lock_guard<std::mutex> lk(mtx_);
    return playerSkins_;
}

int NyxState::SkinForPlayer(int playerid) const {
    std::lock_guard<std::mutex> lk(mtx_);
    auto it = playerSkins_.find(playerid);
    return it == playerSkins_.end() ? 0 : it->second;
}

bool NyxState::IsCustomAssigned(int skinid) const {
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& kv : playerSkins_)
        if (kv.second == skinid) return true;
    return false;
}

bool NyxState::IsVehicle(int skinid) const {
    std::lock_guard<std::mutex> lk(mtx_);
    for (const auto& m : models_)
        if (m.skinid == skinid && m.isVehicle) return true;
    return false;
}

void NyxState::ClearPlayerSkins() {
    std::lock_guard<std::mutex> lk(mtx_);
    playerSkins_.clear();
}

void NyxState::RequestRestore() {
    std::lock_guard<std::mutex> lk(mtx_);
    restoreRequested_ = true;
}

bool NyxState::TakeRestoreRequest() {
    std::lock_guard<std::mutex> lk(mtx_);
    bool pending = restoreRequested_;
    restoreRequested_ = false;
    return pending;
}

NyxState& GlobalState() {
    static NyxState state;
    return state;
}

}  // namespace nyx
