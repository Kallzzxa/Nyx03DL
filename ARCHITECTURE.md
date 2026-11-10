# Nyx03DL — custom skins ala 0.3DL untuk SA-MP 0.3.7 (server plugin + client .asi)

## Ide user (2026-11-10)

- File `.dff` + `.txd` di `scriptfiles/nyx03dl/`.
- Native Pawn `SetCostumNyx03DL(skinid, "nama.dff", "nama.txd")` (ejaan user: Costum).
- Command `/setskin <id>` — skinid yang user sebut 1999.
- Client .asi: pas connect, download dulu file-nya, simpan di cache, baru skin-nya bisa dipakai.
- User pasang .asi sendiri (Muse JANGAN pasangin, cukup kasih tahu caranya).
- Target juga client lain (mereka pasang .asi yang sama) + .so Android nanti (fase terpisah).

## Koreksi yang sudah disepakati

- **Skin ID 1999 JANGAN dipakai.** Range aman ala 0.3DL: **20000–30000**.
  1999 ada di dalam range model game (0–19999). Setiap skin custom butuh satu
  **base ped** (ID 0–311) yang template-nya di-swap — base ini di-assign otomatis
  dari pool ped yang jarang dipakai, dan ikut dikirim ke client lewat manifest.
- Server user = samp-server 0.3.7 R2 (Society RP), BUKAN open.mp. Jadi server
  tidak punya protokol artwork — plugin yang harus mengadakannya.
- Semua pemain harus download model yang sama (bukan cuma yang /setskin).
  Yang tidak pasang .asi melihat skin base (fallback), bukan crash.

## Referensi (sudah di-clone lokal)

`reference/customskin-0.3.7-R5/` — ricardoofnl/customskin-0.3.7-R5, GPL-3.0.
.asi buat 0.3.7-R5 yang meminjam sistem artwork 0.3DL open.mp. Temuan kunci:

- R5 samp.dll sudah berisi **client 0.3DL dormant** — plugin membangunkan-nya
  dengan mendaftarkan RPC handler-nya (objects dirender native).
- Skins TIDAK bisa dirender client dormant itu → plugin mirror entry skin
  dan **clump swapping**: template RenderWare clump di model info base
  (+0x1C) ditukar dengan clump custom. Game clone template itu tiap spawn.
- Jangan sentuh ped yang sudah hidup (DeleteRwObject + CreateRwObject live
  = crash). Swap template saja; game clone secara natural.
- Streamer bisa unload base model dan menghancurkan clump kita → cek per-frame
  apakah template masih pegang clump kita; kalau tidak, drop pointer tanpa
  free ulang + reload dari cache. `RestoreAll` saat reconnect/GMX.
- Race "skin baru muncul di percobaan kedua": swap harus terjadi DI DALAM hook
  skin-change, SEBELUM game rebuild ped; + pin base model resident via
  `CStreaming::RequestModel` (0x4087E0, terverifikasi di IDA).
- Download di background thread (jangan di game thread) — antrean + worker,
  hasil dikumpulkan per-frame. `User-Agent: SAMP/0.3` (open.mp tolak yang lain).
- Protokol artwork: lihat `reference/customskin-0.3.7-R5/docs/protocol.md`
  (RPC 179/181/182/183/184/185, CRC32 IEEE, cache di
  `Documents/GTA San Andreas User Files/SAMP/cache/<ip_port>/`).
- Build repo itu: MSVC, DLL 32-bit, static CRT. Lisensi **GPL-3.0** — .asi
  turunan dari kode ini WAJIB GPL-3.0 juga.

## Arsitektur yang dipilih — Opsi B: manifest HTTP + polling

Alasan: plugin samp-server TIDAK punya akses RakNet untuk mengirim RPC custom
(verifikasi di awal M1 via docs sampgdk — kalau ternyata bisa, Opsi A boleh
dipertimbangkan ulang, tapi B jalan apapun hasilnya). Opsi B tidak butuh
masquerade versi dan tidak butuh packet rewriting.

### Alur

1. Gamemode (OnGameModeInit) panggil `SetCostumNyx03DL(skinid, dff, txd)`
   per skin. Plugin: baca file dari `scriptfiles/nyx03dl/`, hitung CRC32 +
   ukuran, assign base ped otomatis dari pool, simpan di registry.
2. Plugin menjalankan **HTTP server kecil** (port konfigurasi, default
   game-port + 1000, misal 8777 — jangan tabrakan dengan 7777):
   - `GET /nyx03dl/manifest` → JSON: daftar model
     `{skinid, baseid, dff, txd, dff_crc32, txd_crc32, dff_size, txd_size}`.
   - `GET /nyx03dl/file/<nama>` → isi file .dff/.txd (hanya file yang
     terdaftar; tolak path traversal).
   - `GET /nyx03dl/players` → JSON: `{playerid: skinid}` saat ini
     (di-update tiap Nyx_SetPlayerSkin / disconnect).
3. `.asi` (target utama: SA-MP 0.3.7 **R5**, GTA SA 1.0 US):
   - Saat connect, probe `http://<ip server>:<port>/nyx03dl/manifest`.
     Tidak menjawab → tidur (server bukan Nyx, game jalan normal).
   - Download file yang CRC-nya belum cocok di cache (background thread),
     verifikasi CRC32, simpan di cache
     (`Documents/GTA San Andreas User Files/SAMP/cache/<ip_port>/nyx03dl/`
     atau folder cache sendiri — putuskan di M2, konsisten).
   - Clump swapping per model (ikut pola ricardoofnl: `game/render.cpp`,
     `game/rwmodel.cpp` sebagai referensi, adaptasi di bawah GPL-3.0).
   - Terapkan skin: hook saat skin player berubah (atau polling
     `/nyx03dl/players` tiap ~2 detik + saat spawn) → swap template base
     yang bersangkutan SEBELUM game rebuild ped + pin base resident.
   - Pemain lain: sama — mapping playerid→skinid dari endpoint players.
   - Reconnect/GMX: restore template asli (RestoreAll).
4. Gamemode menerapkan skin via native plugin
   `Nyx_SetPlayerSkin(playerid, skinid)` (BARU, bukan SetPlayerSkin langsung):
   plugin simpan mapping, lalu panggil SetPlayerSkin ASLI dengan **baseid**
   supaya yang lewat wire selalu skin valid 0.3.7. (SetPlayerSkin(20000)
   langsung di wire = model tidak valid di client stock.)

### Native plugin (sampgdk)

- `SetCostumNyx03DL(skinid, dff[], txd[])` → return baseid yang di-assign
  (atau -1 kalau gagal: file tidak ada / skinid di luar 20000–30000 /
  pool base habis). Ejaan dipertahankan sesuai permintaan user.
- `Nyx_SetPlayerSkin(playerid, skinid)` → return 1 sukses, 0 gagal.
- `Nyx_GetBaseSkin(skinid)` → baseid (buat gamemode yang butuh).
- Config: `nyx03dl.cfg` atau server.cfg: `nyx_http_port`, `nyx_base_pool`
  (daftar ped ID yang boleh dipakai sebagai base).

### Yang TIDAK dikerjakan di run ini

- .so Android (fase terpisah, setelah PC terbukti jalan di tangan user).
- Custom objects (AddSimpleModel) — skins dulu, sesuai permintaan user.
- Client 0.3.7 R1–R4 (target R5 dulu; alamat hook beda per versi).
- Server open.mp (server user = samp-server).

## Build (VM, tanpa MSVC)

- `zig cc -target x86-windows-gnu` — TERBUKTI bisa hasilkan DLL PE32
  (test 2026-11-10: 797.696 B). Plugin .dll + .asi sama-sama 32-bit.
- Kalau kode turunan customskin pakai MSVC-ism yang tidak bisa di-zig-kan,
  fallback = GitHub Actions MSVC lewat repo (BUTUH persetujuan user dulu —
  jangan buat/push repo tanpa bilang).
- sampgdk untuk plugin; urmem/MinHook/vendor dari reference repo untuk .asi.
- TIDAK ADA tes in-game di VM (tanpa Windows/GTA). Semua milestone =
  build-only sampai user tes di PC-nya. Katakan itu di setiap laporan.

## Milestone

- **M1 — plugin server**: skeleton sampgdk, native di atas, baca
  scriptfiles/nyx03dl, CRC32, HTTP server (manifest + file + players),
  logging, config. Uji: plugin load di samp-server Windows? (tidak bisa di
  VM — verifikasi = compile bersih + review + test HTTP server di Linux
  kalau memungkinkan).
- **M2 — .asi download/cache**: deteksi connect + probe manifest, download
  background thread, CRC32, cache, tidur kalau bukan server Nyx.
- **M3 — model loading/apply**: clump swapping, per-frame restore, pin base,
  hook skin-change, remote players via polling players-endpoint, restore
  saat reconnect/GMX.
- **M4 — Pawn include + contoh**: `nyx03dl.inc`, contoh `/setskin` (pawncmd,
  sesuai gamemode user), `server.cfg` (plugins + config), contoh
  `scriptfiles/nyx03dl/` (placeholder — file .dff/.txd ASLI dari user).
- **M5 — paket + panduan**: zip (plugin .dll + .asi + .inc + contoh + README
  install), panduan pasang .asi (folder GTA SA + ASI loader), cara tes
  (connect, /setskin 20000, cek pemain lain).

## Token

Saldo additional 2026-11-10 ~13:22: **556M**. Cek saldo SEBELUM tiap
milestone; STOP kalau mendekati 400M (floor user) dan laporkan. Kerja
se-efisien mungkin; jangan ulang build yang sudah hijau.
