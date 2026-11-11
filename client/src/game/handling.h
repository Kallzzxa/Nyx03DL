// handling.h — runtime handling patches for replaced vehicles (v1.3.0).
//
// GAME-THREAD ONLY. Patches the in-memory handling data (GTA 1.0 US)
// for one vehicle model: top speed, engine acceleration, brake.
// Sources (plugin-sdk, DK22Pac/plugin-sdk, master):
//   cHandlingDataMgr.cpp: gHandlingDataMgr at 0xC2B9C8.
//   cHandlingDataMgr.h: m_aVehicleHandling at +0x14, 210 entries.
//   tHandlingData.h: entry size 0xE0; m_nVehicleId +0x0,
//     m_fTractionMultiplier +0x28, m_transmissionData +0x2C,
//     m_fBrakeDeceleration +0x94.
//   cTransmission.h: m_fEngineAcceleration +0x50,
//     m_fEngineInertia +0x54, m_fMaxGearVelocity +0x58 (relative to
//     the transmission block).
//   CVehicleModelInfo.h: m_nHandlingId +0x4A (unsigned short) — the
//     game's own index of the model into m_aVehicleHandling.
//   CVehicle.h: m_pHandlingData — vehicles read their handling through
//     a pointer to this shared entry, so patching it affects every
//     vehicle of that model, live, including ones already spawned.
//
// Lookup: entry = m_aVehicleHandling[modelInfo->m_nHandlingId], then
// verified against entry->m_nVehicleId == vehid. The verification
// keeps bikes/boats/planes out: their handling lives in the separate
// bike/flying/boat arrays (same handlingId would index the wrong
// array), so a mismatch skips the patch with a log line instead of
// writing into a stranger's entry.
//
// Units: maxspeed arrives in handling.cfg units (km/h) and is
// converted to world units with the standard x0.5 factor (the same
// conversion the game's own ConvertDataToWorldUnits applies when
// loading handling.cfg). accel/brake are written as given (cfg units,
// unconverted — the exact feel is tunable; the values written are
// logged so the user can compare with what they asked for).
//
// Originals are recorded on the first patch and RestoreAll() writes
// them back — called on disconnect/server change so a handling patch
// never leaks onto a server that did not ask for it.
#pragma once

namespace handling {

// Apply handling for one replaced vehicle model. `modelInfo` is the
// CBaseModelInfo pointer for `vehid` (CVehicleModelInfo layout).
// No-op when all three values are 0 (stock). Only writes fields whose
// desired value differs from what was last applied (tracked per
// vehid). Game thread only. Logs once per applied change, and once
// when the entry cannot be found or the vehicle is not a car.
void Apply(int vehid, void* modelInfo, float maxspeed, float accel,
           float brake);

// Write back every recorded original and forget all state. Called on
// disconnect / server change (render::Tick restore branch).
void RestoreAll();

// Testable core: the handling entry for `vehid` given the manager
// block and the model's CVehicleModelInfo pointer. Returns nullptr
// when the handling id is out of range or the entry's vehicle id does
// not match (not a car). Pure pointer math — no game calls, safe to
// unit-test on Linux.
void* FindEntry(void* mgr, void* modelInfo, int vehid);

}  // namespace handling
