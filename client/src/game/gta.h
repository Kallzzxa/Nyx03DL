// gta.h — GTA SA 1.0 US absolute addresses + RenderWare constants.
//
// Adapted from ricardoofnl/customskin-0.3.7-R5 (src/game/gta.h), GPL-3.0.
// Addresses are absolute VAs in gta_sa.exe (ImageBase 0x400000); the
// runtime delta guards a rebased exe (normally 0). Sources cited by the
// reference: DK22Pac/plugin-sdk (addresses) and IDA verification for
// CStreaming::RequestModel.
//
// Only the pieces M3 needs are kept: RwStream / clump / txd functions,
// the model-info array + clump field offset, RequestModel, FindPlayerPed,
// and CPed::SetModelIndex (v1.2.4 rebuild — see render.cpp).
#pragma once

#include <cstdint>

namespace gta {

// RenderWare stream / model / txd ---
constexpr uintptr_t RwStreamOpen = 0x7ECEF0;              // (RwStreamType, RwStreamAccessType, const void*) -> RwStream*
constexpr uintptr_t RwStreamClose = 0x7ECE20;             // (RwStream*, void*)
constexpr uintptr_t RwStreamFindChunk = 0x7ED2D0;         // (RwStream*, RwUInt32 type, RwUInt32* lenOut, RwUInt32* verOut) -> RwBool
constexpr uintptr_t RpClumpStreamRead = 0x74B420;         // (RwStream*) -> RpClump*
constexpr uintptr_t RpClumpDestroy = 0x74A310;            // (RpClump*)
constexpr uintptr_t RwTexDictionaryStreamRead = 0x804C30; // (RwStream*) -> RwTexDictionary*
constexpr uintptr_t RwTexDictionaryGetCurrent = 0x7F3A90; // () -> RwTexDictionary*
constexpr uintptr_t RwTexDictionarySetCurrent = 0x7F3A70; // (RwTexDictionary*)
constexpr uintptr_t RwTexDictionaryDestroy = 0x7F36A0;    // (RwTexDictionary*)

// model system / entity ---
constexpr uintptr_t ms_modelInfoPtrs = 0xA9B0C8; // CBaseModelInfo*[20000]
constexpr uintptr_t FindPlayerPed = 0x56E210;    // __cdecl (int id, -1 = local) -> CPed*
constexpr int kModelInfo_RwObject = 0x1C;        // CBaseModelInfo::m_pRwClump offset

// CStreaming::RequestModel(int modelId, int flags), __cdecl. keeps a custom skin's base ped
// resident so the clump swap lands before the ped rebuild
constexpr uintptr_t CStreaming_RequestModel = 0x4087E0;
constexpr int kStreamPriorityLoad = 0x10; // priority-request flag bit
// CEntity vtable indices (thiscall via *(void***)ped) — SetModelIndex
// is index 5 (plugin-sdk CEntity.cpp calls (*(void***)this)[5]).
constexpr int kVt_SetModelIndex = 5;
constexpr int kVt_CreateRwObject = 7;
constexpr int kVt_DeleteRwObject = 8;

// CPed::SetModelIndex, __thiscall (CPed*, unsigned int) — direct GTA
// 1.0 US address (plugin-sdk CPed.cpp). This is the exact function
// SA-MP's own ScrSetPlayerSkin handler calls on a ped, so calling it
// from the game thread rebuilds the ped from its (possibly swapped)
// template. v1.2.4 uses it once per learned skin change (see
// render.cpp); the old "never call SetModelIndex on a live ped" rule
// is retired by that version — a rebuild is precisely what was missing.
constexpr uintptr_t CPed_SetModelIndex = 0x5E4880;

// rw enums / chunk ids
constexpr int kStreamFileName = 2;        // rwSTREAMFILENAME
constexpr int kStreamRead = 1;            // rwSTREAMREAD
constexpr uint32_t kChunkClump = 0x10;    // rwID_CLUMP
constexpr uint32_t kChunkTexDict = 0x16;  // rwID_TEXDICTIONARY

}  // namespace gta
