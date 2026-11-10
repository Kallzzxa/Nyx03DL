# Nyx03DL — Pawn include + contoh /setskin (M4)

File Pawn untuk plugin server Nyx03DL (custom skins ala 0.3DL
untuk SA-MP 0.3.7 R2). Desain lengkap di `../ARCHITECTURE.md`;
kontrak native + config + HTTP di `../server/README.md`;
cara kerja client .asi di `../client/README.md`.

**Status: build-only (compile check saja).** Contoh di bawah ini
belum pernah dites in-game — belum ada tes load plugin di
samp-server Windows dan belum ada tes client .asi di PC (tidak ada
Windows/GTA di VM ini). Yang terverifikasi di VM: compile bersih
0 error 0 warning (lihat "Verifikasi compile" di bawah).

## File di folder ini

| File | Isi |
| --- | --- |
| `nyx03dl.inc` | Include Pawn: 3 native plugin + konstanta. Copy ke `pawno/include/`. |
| `nyx03dl_example.pwn` | Filterscript contoh: daftar 2 skin + `/setskin` + `/skins` (pawncmd). |
| `server.cfg.example` | CONTOH server.cfg — sesuaikan, jangan timpa server.cfg asli Anda. |
| `scriptfiles/nyx03dl/README.md` | Aturan taruh file .dff/.txd asli. |
| `scriptfiles/nyx03dl/PUT_YOUR_FILES_HERE.txt` | Penanda folder (tidak ada model asli di sini). |
| `test-stubs/` | Stub pawncmd + file tes compile — BUKAN bagian yang di-ship. |

`pawncmd.inc` dan `sscanf2.inc` TIDAK ikut di-ship — gamemode
Anda sudah punya keduanya.

## Cara pakai

### 1. Include + plugin + config

1. Copy `nyx03dl.inc` ke `pawno/include/` server Anda.
2. Copy `nyx03dl.dll` (hasil build M1, `../server/out/`) ke folder
   `plugins/` server.
3. Copy `nyx03dl.cfg` (`../server/nyx03dl.cfg`) ke folder server,
   di sebelah `samp-server.exe`. Default: HTTP port 8777, base pool
   ped 264–293. Kalau file ini tidak ada, plugin pakai default itu.
4. Di `server.cfg`: TAMBAHKAN `nyx03dl` ke baris `plugins` yang
   sudah ada — jangan timpa daftar plugin lain. Lihat
   `server.cfg.example` (contoh: `plugins streamer sscanf nyx03dl`).

### 2. Taruh file model asli

Taruh file `.dff`/`.txd` ASLI Anda di `scriptfiles/nyx03dl/`
di server. Satu skin = 1 pasang `.dff` + `.txd`, nama file harus
persis sama dengan yang didaftarkan di Pawn (case-sensitive di
Linux). Folder contoh di sini TIDAK berisi model apa pun —
"skin1.dff" / "skin2.dff" di contoh hanya nama.

### 3a. Pakai filterscript contoh (paling cepat)

1. Compile `nyx03dl_example.pwn` (lihat "Cara compile" di bawah).
2. Copy `nyx03dl_example.amx` hasil compile ke `filterscripts/`.
3. Tambahkan `nyx03dl_example` ke baris `filterscripts` di
   `server.cfg` (lihat `server.cfg.example`).
4. Restart server. Skin didaftarkan otomatis di
   `OnFilterScriptInit`.

### 3b. Integrasi langsung ke gamemode (yang disarankan)

Pindahkan dari `nyx03dl_example.pwn` ke gamemode Anda:

1. `#include <nyx03dl>` di atas file gamemode.
2. Isi `OnFilterScriptInit` contoh -> pindahkan ke `OnGameModeInit`
   (tetap SEBELUM pemain connect — skin yang didaftarkan belakangan
   tidak akan di-download pemain yang sudah connect).
3. `CMD:setskin` dan `CMD:skins` -> pindahkan apa adanya.
   Gamemode Society RP memakai pawncmd, jadi tetap `CMD:` seperti
   contoh ini (BUKAN `OnPlayerCommandText`).
4. Sesuaikan daftar skin (`g_NyxSkinId` / `g_NyxDff` / `g_NyxTxd`)
   dengan file `.dff`/`.txd` asli Anda.

## Cara compile contoh

Compiler yang dipakai di VM: pawncc 3.10.10
(`~/workspace/society-rp/pawncc-build/pawncc`).

```sh
pawncc nyx03dl_example.pwn \
  -i. \
  -i<pawno/include server Anda> \
  -o nyx03dl_example.amx
```

`-i.` = folder ini (untuk `nyx03dl.inc`); `-i<pawno/include>`
harus berisi `a_samp.inc`, `pawncmd.inc`, dan `sscanf2.inc`.
Hasil: `nyx03dl_example.amx` di sebelah file `.pwn`.

## Cara tes (di PC, setelah M1 + M2/M3 terpasang)

1. Server: plugin ter-load (cek server log: tidak ada error load
   `nyx03dl.dll`), `nyx03dl.cfg` ada di sebelah `samp-server.exe`,
   file `.dff`/`.txd` asli ada di `scriptfiles/nyx03dl/`,
   filterscript/gamemode ter-load (log: `[nyx03dl] Skin 20000
   terdaftar (base ped ...)`).
2. Client: pasang `nyx03dl.asi` di folder GTA SA, di sebelah
   `gta_sa.exe` (butuh ASI loader, mis. `vorbisFile.dll` — sama
   seperti `.asi` lain). Log client ada di `nyx03dl.log` di sebelah
   `gta_sa.exe`.
3. Connect ke server, lalu di chat:
   - `/skins` — harus menampilkan skin 20000 dan 20001 + base ped.
   - `/setskin 20000` — skin Anda harus berubah ke skin custom.
   - Minta pemain lain (yang juga pasang `.asi`) memakai
     `/setskin 20001` — Anda harus melihat skin custom-nya.
   - Pemain TANPA `.asi` melihat skin base (fallback), bukan crash.
4. Kalau gagal, cek dulu: server log (apakah skin terdaftar?),
   `nyx03dl.log` client (apakah manifest ter-probe? file
   ter-download? CRC cocok?), dan cache client di
   `Documents\GTA San Andreas User Files\SAMP\cache\nyx03dl\`.

## Batasan jujur (baca sebelum mengklaim apa pun)

- **Contoh ini belum pernah dites in-game.** Yang terverifikasi
  hanyalah compile bersih di VM (lihat bawah). Apakah
  `/setskin` benar-benar menampilkan skin custom di game baru
  ketahuan saat tes di PC (butuh plugin M1 ter-load di samp-server
  Windows + `.asi` M2/M3 + file model asli).
- **Skin ID 1999 JANGAN dipakai.** Range custom 20000–30000.
  1999 masih di dalam range model game (0–19999).
- **Daftar skin SEBELUM pemain connect.** Client `.asi` hanya
  mem-probe manifest saat connect; skin yang didaftarkan belakangan
  tidak akan di-download pemain yang sudah connect.
- **Target client `.asi`: SA-MP 0.3.7 R5, GTA SA 1.0 US.**
  Build SA-MP lain ditolak `.asi` dengan sengaja.
- **Semua pemain harus download model yang sama** (bukan cuma yang
  `/setskin`) — `.asi` men-download seluruh manifest saat connect.
- **Ejaan `SetCostumNyx03DL` dipertahankan persis** (Costum, bukan
  Custom) supaya cocok dengan export plugin. Jangan "diperbaiki".

## Verifikasi compile (di VM, 2026-11-10)

Compiler: pawncc 3.10.10. Include: `a_samp.inc` dari Society RP,
`sscanf2.inc` 2.11.2 (dari koky-mp). **pawncmd.inc asli TIDAK ada
di VM dan repo publik urShadow/pawncmd sudah tidak bisa diunduh**
(raw.githubusercontent + api.github.com sama-sama 404 pada
2026-11-10), jadi compile check memakai **stub minimal**
(`test-stubs/pawncmd.inc`, hanya macro `CMD:` secukupnya, tanpa
dispatcher command). Artinya: sintaks + native terverifikasi,
tapi compile ulang dengan pawncmd.inc ASLI di PC/server sebelum
memakai `.amx` di server sungguhan — `.amx` hasil compile dengan
stub TIDAK BOLEH dipakai (command tidak akan jalan).

Perintah persis yang dijalankan (dari folder `pawn/` ini):

```sh
~/workspace/society-rp/pawncc-build/pawncc test-stubs/test_inc.pwn \
  -i. -i~/workspace/society-rp/repo/pawno/include \
  -o/tmp/test_inc.amx
# -> 0 error, 0 warning

~/workspace/society-rp/pawncc-build/pawncc nyx03dl_example.pwn \
  -i. -itest-stubs \
  -i~/workspace/society-rp/repo/pawno/include \
  -i~/workspace/koky-mp/repo/pawno/include \
  -o/tmp/nyx03dl_example.amx
# -> 0 error, 0 warning
```

Diverifikasi juga lewat `strings` pada `.amx` hasil compile:
`SetCostumNyx03DL`, `Nyx_GetBaseSkin`, `Nyx_SetPlayerSkin`,
`cmd_setskin`, `cmd_skins`, dan `OnFilterScriptInit` semuanya ada.
