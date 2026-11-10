// nyx03dl.inc — include Pawn untuk plugin server Nyx03DL.
//
// Nyx03DL = custom skins ala 0.3DL untuk SA-MP 0.3.7 R2.
// Gamemode mendaftarkan skin lewat native di bawah, plugin lalu
// menghitung CRC32 + ukuran file, meng-assign satu "base ped"
// (ID 0-311) secara otomatis, dan menyajikan manifest + file lewat
// HTTP server kecil untuk client .asi (lihat ../server/README.md
// dan ../client/README.md).
//
// CARA PAKAI:
//   1. Copy file ini ke pawno/include/ server Anda.
//   2. Di gamemode / filterscript: #include <nyx03dl>
//   3. Daftarkan skin di OnGameModeInit / OnFilterScriptInit,
//      SEBELUM pemain connect (lihat nyx03dl_example.pwn).
//   4. Pastikan plugin nyx03dl.dll sudah di-load (server.cfg,
//      baris "plugins") dan file .dff/.txd ASLI sudah ada di
//      scriptfiles/nyx03dl/.
//
// CATATAN EJAAN: nama native "SetCostumNyx03DL" dipertahankan persis
// seperti permintaan awal (Costum, bukan Custom). Jangan "diperbaiki",
// karena nama ini harus cocok dengan yang di-export plugin.

#if defined _nyx03dl_included
	#endinput
#endif
#define _nyx03dl_included

// ---------------------------------------------------------------------------
// Konstanta
// ---------------------------------------------------------------------------

// Range skin ID custom yang boleh dipakai. JANGAN pakai ID di bawah
// 20000 (mis. 1999): itu masih di dalam range model game (0-19999)
// dan akan bentrok dengan model bawaan GTA SA.
#define NYX_MIN_SKINID (20000)
#define NYX_MAX_SKINID (30000)

// Nilai yang dikembalikan kalau base ped tidak valid / skin belum
// terdaftar (lihat SetCostumNyx03DL dan Nyx_GetBaseSkin).
#define NYX_INVALID_BASE (-1)

// ---------------------------------------------------------------------------
// Native plugin (di-export oleh nyx03dl.dll — JANGAN ubah tandatangannya)
// ---------------------------------------------------------------------------

// Daftarkan satu skin custom ke plugin.
//
//   skinid : ID skin custom, harus 20000-30000.
//   dff[]  : nama file .dff, mis. "skin1.dff".
//   txd[]  : nama file .txd, mis. "skin1.txd".
//            Kedua file HARUS sudah ada di scriptfiles/nyx03dl/
//            dan namanya harus persis sama (case-sensitive di Linux).
//
// Return : base ped yang di-assign plugin (ID 0-311), atau
//          NYX_INVALID_BASE (-1) kalau gagal:
//            - file tidak ada di scriptfiles/nyx03dl/
//            - skinid di luar 20000-30000
//            - pool base ped habis
//            - nama file tidak valid / ekstensi bukan .dff/.txd
//
// Idempotent: daftar ulang skinid yang sama dengan file yang sama
// -> baseid yang sama. Daftar ulang dengan file berbeda -> entry
// diperbarui, baseid dipertahankan.
//
// Contoh:
//   new baseid = SetCostumNyx03DL(20000, "skin1.dff", "skin1.txd");
//   if (baseid == NYX_INVALID_BASE)
//   {
//       print("[nyx03dl] GAGAL mendaftarkan skin 20000!");
//   }
native SetCostumNyx03DL(skinid, dff[], txd[]);

// Ganti skin seorang pemain ke skin custom yang sudah terdaftar.
//
//   playerid : ID pemain.
//   skinid   : ID skin custom (20000-30000) yang SUDAH didaftarkan
//              lewat SetCostumNyx03DL.
//
// Return : 1 kalau sukses, 0 kalau gagal (skin belum terdaftar /
//          playerid tidak valid / pemain tidak connected).
//
// PENTING: pakai native ini, BUKAN SetPlayerSkin() langsung.
// Plugin memanggil SetPlayerSkin ASLI dengan baseid, supaya yang
// lewat wire selalu skin 0.3.7 yang valid. SetPlayerSkin(20000)
// langsung = model tidak valid di client stock.
//
// Contoh:
//   if (Nyx_SetPlayerSkin(playerid, 20000))
//   {
//       SendClientMessage(playerid, -1, "Skin diganti ke 20000.");
//   }
native Nyx_SetPlayerSkin(playerid, skinid);

// Ambil base ped yang di-assign untuk sebuah skin custom.
//
//   skinid : ID skin custom (20000-30000).
//
// Return : baseid (0-311), atau NYX_INVALID_BASE (-1) kalau skin
//          itu belum terdaftar.
//
// Contoh:
//   new baseid = Nyx_GetBaseSkin(20000);
//   if (baseid != NYX_INVALID_BASE)
//   {
//       printf("Skin 20000 memakai base ped %d.", baseid);
//   }
native Nyx_GetBaseSkin(skinid);
