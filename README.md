# Nyx03DL — custom skins ala 0.3DL untuk SA-MP 0.3.7

> **STATUS: BUILD-ONLY — BELUM PERNAH DITES IN-GAME.** Lihat bagian 7 di bawah.

Repo source (GPL-3.0). **Paket siap pakai (plugin + .asi + contoh) ada di [Releases](../../releases) — `nyx03dl-v1.0.0.zip`.**

## Build dari source (zig, tanpa MSVC)

```sh
./server/build.sh   # -> server/out/nyx03dl.dll
./client/build.sh   # -> client/out/nyx03dl.asi
```

Zig yang dipakai: `~/workspace/build-deps/zig-x86_64-linux-0.16.0/zig` (0.16.0).
Detail build ada di `server/README.md` dan `client/README.md`.
Desain lengkap: `ARCHITECTURE.md`.

---

# Nyx03DL v1.0.0 — custom skins ala 0.3DL untuk SA-MP 0.3.7

> **STATUS: BUILD-ONLY — BELUM PERNAH DITES IN-GAME.**
> Paket ini baru terverifikasi sampai tahap compile (plugin, `.asi`, dan
> contoh Pawn compile bersih di VM tanpa Windows/GTA). Apakah skin custom
> benar-benar tampil di game baru ketahuan saat Anda tes di PC.
> Jangan anggap paket ini pasti jalan sebelum tes itu lolos.

## 1. Apa ini

Nyx03DL adalah plugin server + client `.asi` yang membawa custom skin
ala 0.3DL ke SA-MP 0.3.7 R5. File model `.dff`/`.txd` ditaruh di
`scriptfiles/nyx03dl/` di server, didaftarkan lewat native Pawn, lalu
client `.asi` men-download-nya saat connect dan men-swap template skin
di game. Command contoh: `/setskin 20000` (skin custom memakai ID
**20000–30000** — jangan pakai 1999, itu masih model game).

Isi paket ini:

| File | Keterangan |
| --- | --- |
| `server/nyx03dl.dll` | Plugin server SA-MP 0.3.7 R2 (Windows) |
| `server/nyx03dl.cfg` | Config plugin (port HTTP + base pool) |
| `client/nyx03dl.asi` | Client `.asi` (SA-MP 0.3.7 **R5** saja) |
| `pawn/nyx03dl.inc` | Include Pawn (3 native) |
| `pawn/nyx03dl_example.pwn` | Filterscript contoh `/setskin` + `/skins` (source saja) |
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
- **Client:** GTA SA 1.0 US + SA-MP 0.3.7 **R5** + ASI loader.
- **Hanya R5.** `.asi` memeriksa fingerprint `samp.dll` dan
  **menolak build SA-MP lain** (R1–R4 tidak didukung — alamat hook
  beda per versi).

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

## 4. Install client (.asi) — Anda pasang sendiri

1. Pastikan GTA SA **1.0 US** + SA-MP 0.3.7 **R5** sudah terpasang.
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
2. Connect dengan client R5 + `.asi` terpasang.
3. Buka `nyx03dl.log` di folder GTA SA — harus ada baris manifest /
   probe sukses dan download selesai (bukan probe gagal / tidur).
4. Di chat: `/skins` — harus menampilkan skin 20000 dan 20001 +
   base ped-nya.
5. `/setskin 20000` — skin Anda harus berubah ke skin custom.
6. Connect pemain kedua (**dengan** `.asi`) — ia harus melihat skin
   custom pemain pertama, dan sebaliknya (minta ia `/setskin 20001`).
7. Connect pemain ketiga **tanpa** `.asi` — ia harus melihat skin
   base (fallback), bukan crash.

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
| Fingerprint mismatch | `samp.dll` bukan R5-1. `.asi` menolak dengan sengaja — pakai SA-MP 0.3.7 R5. |
| Skin tidak berubah tapi log OK | Cek baseid ter-assign di log server, dan apakah skin didaftarkan **sebelum** pemain connect. Skin yang didaftarkan belakangan tidak di-download pemain yang sudah connect — reconnect. |
| Command `/setskin` tidak jalan sama sekali | Kemungkinan `.amx` di-compile dengan stub pawncmd. Compile ulang dengan `pawncmd.inc` ASLI (lihat bagian 3 langkah 6). |
| Crash saat swap | Kirim `nyx03dl.log` + versi `samp.dll` Anda. Lihat juga "Batasan jujur" di bawah. |

## 7. Batasan jujur — baca sebelum mengklaim apa pun

- **Build-only, BELUM PERNAH dites in-game.** Tidak ada Windows/GTA
  di VM tempat paket ini dibuat, jadi tidak ada tes load plugin di
  samp-server Windows dan tidak ada tes load `.asi` di `samp.dll`
  sungguhan. Yang terverifikasi: compile bersih, DLL/ASI PE32 valid,
  dan logika portable lewat harness unit (plugin 35/35, `.asi` 41/41
  + 73/73).
- **`.asi` M3 menulis memori game** — field clump template base ped
  dan patch JMP 5-byte di dua alamat hook `samp.dll`. Kalau ada
  alamat yang meleset (0xB5B0 / 0x19190 / 0xA9B0C8 / 0x4087E0, slot
  vtable RakClient, asumsi byte order PlayerID), gejala yang
  diharapkan: skin tidak muncul (aman) **atau crash saat swap/hook
  (risiko nyata)**.
- **`.so` Android BELUM ada** — itu fase terpisah, setelah versi PC lolos tes di tangan user.
- **Custom objects BELUM didukung** (AddSimpleModel) — skins saja.
- **Hanya SA-MP 0.3.7 R5** (client) dan samp-server 0.3.7 R2
  (server). Build lain ditolak `.asi` dengan sengaja.
- Identitas tiap ped di base pool default (264–293) belum
  diverifikasi in-game — override lewat `nyx_base_pool` di
  `nyx03dl.cfg` kalau ada yang ternyata dipakai server Anda.
- Semua pemain men-download seluruh manifest saat connect (bukan
  cuma skin yang mereka pakai).

## Lisensi

`.asi` turunan kode ricardoofnl/customskin-0.3.7-R5 (GPL-3.0), jadi
`.asi` ini juga GPL-3.0 — lihat `LICENSE`.
