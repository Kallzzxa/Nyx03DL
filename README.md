# Nyx03DL — custom skins & kendaraan ala 0.3DL untuk SA-MP 0.3.7

> **STATUS v1.3.0: SKIN TERBUKTI IN-GAME (R1) — KENDARAAN BUILD-ONLY.** Skin custom sudah jalan end-to-end di SA-MP 0.3.7 **R1** (download, cache, swap, `/setskin` sekali tampil). Replace kendaraan + handling v1.3.0 baru sampai tahap compile + harness unit — belum pernah dites in-game. Lihat bagian 7 di bawah.

Repo source (GPL-3.0). **Paket siap pakai (plugin + .asi + contoh) ada di [Releases](../../releases) — `nyx03dl-v1.3.0.zip`.**

## Build dari source (zig, tanpa MSVC)

```sh
./server/build.sh   # -> server/out/nyx03dl.dll
./client/build.sh   # -> client/out/nyx03dl.asi
```

Zig yang dipakai: `~/workspace/build-deps/zig-x86_64-linux-0.16.0/zig` (0.16.0).
Detail build ada di `server/README.md` dan `client/README.md`.
Desain lengkap: `ARCHITECTURE.md`.

---

# Nyx03DL v1.3.0 — custom skins & kendaraan ala 0.3DL untuk SA-MP 0.3.7

> **STATUS: SKIN TERBUKTI IN-GAME (R1) — KENDARAAN BUILD-ONLY.**
> Skin custom: terverifikasi end-to-end di PC (SA-MP 0.3.7 R1) —
> probe, download + CRC, cache, swap template, `/setskin 20000`
> sekali langsung tampil (v1.2.4).
> Replace kendaraan + handling (baru di v1.3.0): baru terverifikasi
> sampai tahap compile (plugin, `.asi`, contoh Pawn compile bersih)
> dan harness unit (plugin 55/55, `.asi` 133/133). Apakah model
> custom benar-benar tampil di game dan handling-nya terasa benar
> baru ketahuan saat Anda tes di PC. Jangan anggap bagian kendaraan
> pasti jalan sebelum tes itu lolos.

## 1. Apa ini

Nyx03DL adalah plugin server + client `.asi` yang membawa custom skin
ala 0.3DL ke SA-MP 0.3.7 R1 dan R5-1. File model `.dff`/`.txd` ditaruh di
`scriptfiles/nyx03dl/` di server, didaftarkan lewat native Pawn, lalu
client `.asi` men-download-nya saat connect dan men-swap template model
di game. Command contoh: `/setskin 20000` (skin custom memakai ID
**20000–30000** — jangan pakai 1999, itu masih model game).

**v1.3.0 — replace kendaraan:** model kendaraan GTA (400–611) bisa
diganti dengan `.dff`/`.txd` custom lewat `NyxReplaceVeh(vehid, dff, txd)`.
Modelnya TETAP pakai ID yang sama — spawn seperti biasa dengan
`CreateVehicle(vehid, ...)` dan client yang pakai `.asi` melihat model
custom. Handling (kecepatan/akselerasi/rem) diatur terpisah lewat
`NyxSetVehHandling(vehid, maxspeed, accel, brake)`. Command contoh:
`/veh 400`. Lihat `pawn/nyx03dl_example_vehicle.pwn`.

Isi paket ini:

| File | Keterangan |
| --- | --- |
| `server/nyx03dl.dll` | Plugin server SA-MP 0.3.7 R2 (Windows) |
| `server/nyx03dl.cfg` | Config plugin (port HTTP + base pool) |
| `client/nyx03dl.asi` | Client `.asi` (SA-MP 0.3.7 **R1 dan R5-1**) |
| `pawn/nyx03dl.inc` | Include Pawn (6 native) |
| `pawn/nyx03dl_example.pwn` | Filterscript contoh `/setskin` + `/skins` (source saja) |
| `pawn/nyx03dl_example_nopawncmd.pwn` | Contoh `/setskin` + `/skins` TANPA pawncmd/sscanf2 |
| `pawn/nyx03dl_example_vehicle.pwn` | Filterscript contoh `/veh` + `/vehs` (replace kendaraan, tanpa pawncmd) |
| `pawn/server.cfg.example` | Contoh baris `plugins` / `filterscripts` |
| `pawn/scriptfiles/nyx03dl/` | Folder tempat file `.dff`/`.txd` ASLI Anda (kosong — tidak ada model di paket ini) |
| `LICENSE` | GPL-3.0 (untuk `.asi`, turunan ricardoofnl/customskin-0.3.7-R5) |

**TIDAK ADA file `.amx` di paket ini — dan itu disengaja.** File
`.amx` contoh hasil compile di VM dibuat memakai **stub pawncmd**
minimal, jadi command-nya tidak akan jalan di server sungguhan.
Anda WAJIB compile ulang `nyx03dl_example.pwn` dengan
**`pawncmd.inc` ASLI** (lihat langkah 3 di bawah).

## 2. Syarat

- **Server:** `samp-server` 0.3.7 R2 (Windows).
- **Client:** GTA SA 1.0 US + SA-MP 0.3.7 **R1 atau R5-1** + ASI loader.
- `.asi` memeriksa fingerprint `samp.dll` dan **menolak build SA-MP
  lain** (R2–R4, R5-2, dsb. tidak didukung — alamat hook beda per versi).

## 3. Install server

1. Copy `server/nyx03dl.dll` ke folder `plugins/` server.
2. Copy `server/nyx03dl.cfg` ke folder server, di sebelah
   `samp-server.exe`. Default: HTTP port **8777**, base pool ped
   **264–293** (30 slot). Kalau file ini tidak ada, plugin pakai
   default itu. Ubah port/pool di file ini bila perlu — pastikan
   port HTTP tidak bentrok dengan port game (7777) dan dibuka di
   firewall.
3. Di `server.cfg`, **tambahkan** `nyx03dl` ke baris `plugins` yang
   sudah ada — jangan timpa plugin lain. Contoh:
   `plugins streamer sscanf nyx03dl`
   (di Windows nama plugin ditulis tanpa `.dll`).
4. Taruh file `.dff`/`.txd` **ASLI** Anda di
   `scriptfiles/nyx03dl/` di server. Satu skin = 1 pasang
   `.dff` + `.txd`, nama file harus persis sama dengan yang
   didaftarkan di Pawn (case-sensitive di Linux).
5. Copy `pawn/nyx03dl.inc` ke `pawno/include/` server.
6. **Compile `pawn/nyx03dl_example.pwn` dengan `pawncmd.inc` ASLI:**
   `pawncc nyx03dl_example.pwn -i. -i<pawno/include> -o nyx03dl_example.amx`
   (butuh `a_samp.inc`, `pawncmd.inc`, `sscanf2.inc`, `nyx03dl.inc`).
   **JANGAN pakai `.amx` bawaan paket — tidak ada, dan `.amx` hasil
   compile dengan stub pawncmd TIDAK BOLEH dipakai di server.**
   Kalau Anda tidak punya `pawncmd.inc`: pakai `pawncmd.inc` dari
   gamemode Anda sendiri (gamemode Society RP sudah memakainya),
   atau integrasikan isi contoh langsung ke gamemode (pindahkan
   pendaftaran skin ke `OnGameModeInit` dan `CMD:setskin` /
   `CMD:skins` apa adanya) lalu compile gamemode seperti biasa.
7. Copy `nyx03dl_example.amx` hasil compile Anda ke
   `filterscripts/`, lalu tambahkan `nyx03dl_example` ke baris
   `filterscripts` di `server.cfg`
   (lihat `pawn/server.cfg.example` — itu contoh saja, jangan timpa
   `server.cfg` asli Anda).
8. Daftarkan skin di `OnGameModeInit` / `OnFilterScriptInit`
   **sebelum pemain connect**, misal:
   `SetCostumNyx03DL(20000, "skin1.dff", "skin1.txd");`
   (ejaan `Costum` dipertahankan persis — jangan "diperbaiki").
   Skin yang didaftarkan belakangan tidak akan di-download pemain
   yang sudah connect (client hanya mem-probe manifest saat connect).
9. Restart server. Cek server log: plugin ter-load tanpa error dan
   ada baris `[nyx03dl] Skin 20000 terdaftar (base ped ...)`.

Natives plugin (lihat `pawn/nyx03dl.inc`):

- `SetCostumNyx03DL(skinid, dff[], txd[])` → baseid yang di-assign,
  atau -1 kalau gagal (file tidak ada / skinid di luar 20000–30000 /
  pool base habis).
- `Nyx_SetPlayerSkin(playerid, skinid)` → 1 sukses, 0 gagal.
  Pakai native ini, BUKAN `SetPlayerSkin` langsung — plugin yang
  mengirim baseid valid lewat wire.
- `Nyx_GetBaseSkin(skinid)` → baseid, atau -1 kalau belum terdaftar.
- `Nyx_HasClient(playerid)` → 1 kalau pemain itu pakai `.asi` dan
  download-nya sudah READY, 0 kalau tidak.
- `NyxReplaceVeh(vehid, dff[], txd[])` → 1 sukses, 0 gagal (v1.3.0).
  Ganti satu model kendaraan (400–611) dengan `.dff`/`.txd` custom.
- `NyxSetVehHandling(vehid, Float:maxspeed, Float:accel, Float:brake)`
  → 1 sukses, 0 gagal (v1.3.0). Atur handling kendaraan yang sudah
  di-replace; tiap nilai 0.0 = biarkan bawaan. Harus dipanggil
  SETELAH `NyxReplaceVeh` untuk vehid yang sama.

10. **Kendaraan (v1.3.0, opsional):** di `OnGameModeInit` /
    `OnFilterScriptInit`, daftarkan kendaraan SEBELUM pemain connect:
    `NyxReplaceVeh(400, "veh400.dff", "veh400.txd");` lalu
    `NyxSetVehHandling(400, 220.0, 2.0, 3.0);` (opsional).
    File `.dff`/`.txd` kendaraan juga di `scriptfiles/nyx03dl/`.
    Contoh lengkap (command `/veh` + `/vehs`, tanpa pawncmd):
    compile `pawn/nyx03dl_example_vehicle.pwn` seperti langkah 6
    (hanya butuh `a_samp.inc` + `nyx03dl.inc`).

## 4. Install client (.asi) — Anda pasang sendiri

1. Pastikan GTA SA **1.0 US** + SA-MP 0.3.7 **R1 atau R5-1** sudah terpasang.
2. Pastikan ada **ASI loader**. Yang umum: Silent's ASI Loader —
   tandanya ada file `vorbisFile.dll` di folder GTA SA.
   Cara cek: kalau file `.asi` lain bisa jalan di game Anda, loader
   sudah ada. Kalau belum ada, install dulu ASI loader (misal
   Silent's ASI Loader) sebelum lanjut.
3. Copy `client/nyx03dl.asi` ke folder GTA SA, di sebelah
   `gta_sa.exe`.
4. Main SA-MP seperti biasa. Log client ada di `nyx03dl.log` di
   folder yang sama (di sebelah `gta_sa.exe`).

**Pemain lain yang mau melihat skin custom HARUS pasang `.asi` yang
sama.** Pemain yang tidak pasang `.asi` melihat skin base (fallback),
bukan crash.

## 5. Cara tes

1. Jalankan server (plugin ter-load, skin terdaftar — lihat log
   server).
2. Connect dengan client R1/R5-1 + `.asi` terpasang.
3. Buka `nyx03dl.log` di folder GTA SA — harus ada baris manifest /
   probe sukses dan download selesai (bukan probe gagal / tidur).
4. Di chat: `/skins` — harus menampilkan skin 20000 dan 20001 +
   base ped-nya.
5. `/setskin 20000` — skin Anda harus berubah ke skin custom.
6. Connect pemain kedua (**dengan** `.asi`) — ia harus melihat skin
   custom pemain pertama, dan sebaliknya (minta ia `/setskin 20001`).
7. Connect pemain ketiga **tanpa** `.asi` — ia harus melihat skin
   base (fallback), bukan crash.
8. **Kendaraan (v1.3.0):** setelah chat menampilkan
   `Custom vehicles ready`, ketik `/veh 400` — kendaraan model
   custom harus muncul dan bisa dikendarai. Cek `nyx03dl.log`:
   harus ada baris `render: base 400 template clump -> custom 400`
   dan `handling: vehicle 400 maxspeed ...`. Kalau kendaraan muncul
   sebagai model bawaan, kemungkinan Anda `/veh` sebelum download
   selesai — tunggu baris `Custom vehicles ready`, lalu `/veh` lagi.
   Catatan: kendaraan yang sudah terlanjur muncul sebagai model
   bawaan baru berubah setelah re-stream (jauh-dekat) atau di-spawn
   ulang.

Cara cek cache: file `.dff`/`.txd` yang ter-download ada di
`Documents\GTA San Andreas User Files\SAMP\cache\nyx03dl\`
dengan nama **CRC hex 8 digit huruf besar**, misal
`CBF43926.dff`. Kalau file di sana tidak ada atau CRC-nya tidak
cocok, `.asi` men-download ulang otomatis saat connect berikutnya.

## 6. Troubleshooting singkat

| Gejala | Kemungkinan penyebab / yang dicek |
| --- | --- |
| Probe gagal / `.asi` tidur terus (log: probe gagal, retry 30 dtk) | Server bukan server Nyx, atau port HTTP salah. Cek `nyx03dl.cfg` (default 8777 — ingat `.asi` mencoba `gameport+1000` dulu, lalu 8777), firewall port 8777, dan apakah plugin ter-load di server. |
| CRC mismatch | File di-download ulang otomatis. Kalau terus gagal, cek file `.dff`/`.txd` di server (rusak / berubah setelah didaftarkan?). |
| Fingerprint mismatch | `samp.dll` bukan R1 atau R5-1. `.asi` menolak dengan sengaja — pakai SA-MP 0.3.7 R1 atau R5-1. |
| Kendaraan tidak berubah tapi log OK | Kemungkinan Anda `/veh` sebelum download kendaraan selesai, atau kendaraan itu sudah muncul sebagai model bawaan sebelum template di-swap — tunggu `Custom vehicles ready`, lalu `/veh` lagi (atau re-stream). |
| Skin tidak berubah tapi log OK | Cek baseid ter-assign di log server, dan apakah skin didaftarkan **sebelum** pemain connect. Skin yang didaftarkan belakangan tidak di-download pemain yang sudah connect — reconnect. |
| Command `/setskin` tidak jalan sama sekali | Kemungkinan `.amx` di-compile dengan stub pawncmd. Compile ulang dengan `pawncmd.inc` ASLI (lihat bagian 3 langkah 6). |
| Crash saat swap | Kirim `nyx03dl.log` + versi `samp.dll` Anda. Lihat juga "Batasan jujur" di bawah. |

## 7. Batasan jujur — baca sebelum mengklaim apa pun

- **Skin: TERBUKTI in-game (R1).** End-to-end di PC: probe, download
  + CRC, cache, swap template, `/setskin 20000` sekali langsung tampil
  (v1.2.4, rebuild ped via `CPed::SetModelIndex`).
- **Kendaraan v1.3.0: BUILD-ONLY, belum pernah dites in-game.**
  Yang terverifikasi: compile bersih, DLL/ASI PE32 valid, harness
  unit (plugin 55/55, `.asi` 133/133), dan contoh Pawn compile bersih.
  Yang BELUM terverifikasi: apakah template kendaraan benar-benar
  ter-swap di game, apakah handling terasa sesuai angka yang
  dimasukkan (konversi kecepatan ×0.5 adalah konversi standar dari
  plugin-sdk, tapi feel di game belum dikonfirmasi), dan apakah
  kendaraan yang sudah ada di layar ikut berubah atau hanya yang
  baru di-spawn.
- **`.asi` M3 menulis memori game** — field clump template model,
  patch JMP 5-byte di alamat hook `samp.dll`, dan patch data handling
  di `cHandlingDataMgr`. Kalau ada alamat yang meleset, gejala yang
  diharapkan: model tidak muncul (aman) **atau crash saat swap/hook
  (risiko nyata)**.
- **Handling hanya untuk mobil.** Motor/perahu/pesawat: modelnya
  tetap bisa di-replace, tapi handling-nya TIDAK di-patch (datanya
  ada di array terpisah) — `.asi` menulis peringatan di log dan
  melewatinya.
- **`.so` Android BELUM ada** — itu fase terpisah, setelah versi PC lolos tes di tangan user.
- **Custom objects BELUM didukung** (AddSimpleModel) — skins + kendaraan saja.
- **Hanya SA-MP 0.3.7 R1 dan R5-1** (client) dan samp-server 0.3.7 R2
  (server). Build lain ditolak `.asi` dengan sengaja.
- Identitas tiap ped di base pool default (264–293) belum
  diverifikasi in-game — override lewat `nyx_base_pool` di
  `nyx03dl.cfg` kalau ada yang ternyata dipakai server Anda.
- Semua pemain men-download seluruh manifest saat connect (bukan
  cuma skin/kendaraan yang mereka pakai).

## Lisensi

`.asi` turunan kode ricardoofnl/customskin-0.3.7-R5 (GPL-3.0), jadi
`.asi` ini juga GPL-3.0 — lihat `LICENSE`.
