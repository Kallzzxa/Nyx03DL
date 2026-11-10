# nyx03dl — server plugin (M1)

SA-MP 0.3.7 R2 server plugin: custom skins ala 0.3DL. Gamemode
mendaftarkan skin lewat native, plugin menghitung CRC32 + ukuran file,
meng-assign base ped otomatis, dan menyajikan manifest + file lewat
HTTP server kecil untuk client .asi (M2) yang akan polling/download.

**Status: build-only.** Belum ada tes in-game dan belum ada tes load di
samp-server Windows (tidak ada Windows/GTA di VM ini). Yang terverifikasi
di VM: compile bersih dengan zig, DLL PE32 + exports, dan seluruh logika
portable (CRC32, registry, manifest/players JSON) lewat harness Linux
(`test-crc32.cpp`, 35/35 PASS).

## Build (zig, tanpa MSVC)

```sh
./build.sh
```

Menghasilkan `out/nyx03dl.dll` (PE32, 32-bit). Pola build mengikuti
`samp-td-mcp/plugin/build-plugin-zig.bat` yang sudah terbukti, ditambah
satu unit ekstra: `sdk/amx/amxplugin.c` — file ini menyediakan
`amx_Register`/`amx_GetAddr` (forward ke AMX function table server
lewat global `pAMXFunctions`, yang di-set di `Load()`). tdbridge tidak
membutuhkannya karena tidak punya native sendiri; nyx03dl punya 3
native, jadi file ini wajib ikut di-link. Tanpa itu link gagal dengan
`undefined symbol: _amx_Register` / `_amx_GetAddr`.

Zig yang dipakai: `~/workspace/build-deps/zig-x86_64-linux-0.16.0/zig`
(0.16.0). Perintah persis ada di `build.sh`.

Warning yang muncul saat build (jinak, dijelaskan):
- `#pragma message "Warning: Not using a VC++ compiler."` — dari
  `sdk/plugincommon.h`, muncul juga di build tdbridge; bukan error.
- `lld-link: warning: ... locally defined symbol imported: _sampgdk_*`
  (LNK4217) — artefak amalgamasi sampgdk (simbol didefinisikan di
  `sampgdk.o` sekaligus diimpor); DLL tetap ter-link dan semua export
  terverifikasi ada (lihat bawah).

## Install di server

1. Copy `out/nyx03dl.dll` ke folder `plugins/` server.
2. Copy `nyx03dl.cfg` ke folder server (sebelah `samp-server.exe`).
   Kalau tidak ada, plugin pakai default (port 8777, pool 264–293)
   dan mencatat itu di log.
3. Taruh file model di `scriptfiles/nyx03dl/` (misal `skin1.dff`,
   `skin1.txd`).
4. `server.cfg`: tambahkan `nyx03dl` ke baris `plugins`.
5. Gamemode (lihat M4 untuk `nyx03dl.inc` + contoh `/setskin`):
   `SetCostumNyx03DL(20000, "skin1.dff", "skin1.txd")` di
   `OnGameModeInit`, lalu `Nyx_SetPlayerSkin(playerid, 20000)`.

## Natives (ejaan dipertahankan persis)

- `SetCostumNyx03DL(skinid, dff[], txd[])` → baseid yang di-assign,
  atau -1 kalau gagal (file tidak ada di `scriptfiles/nyx03dl/`,
  skinid di luar 20000–30000, pool base habis, nama file tidak valid
  atau ekstensi bukan `.dff`/`.txd`). Idempotent: daftar ulang skinid
  yang sama dengan file yang sama → baseid yang sama. Daftar ulang
  dengan file berbeda → entry diperbarui, baseid dipertahankan.
- `Nyx_SetPlayerSkin(playerid, skinid)` → 1 sukses, 0 gagal (skin
  belum terdaftar / playerid tidak valid / player tidak connected).
  Saat sukses, plugin memanggil `SetPlayerSkin` ASLI dengan **baseid**,
  jadi yang lewat wire selalu skin 0.3.7 yang valid.
- `Nyx_GetBaseSkin(skinid)` → baseid, atau -1 kalau belum terdaftar.

## Config (`nyx03dl.cfg`, format key=value)

- `nyx_http_port` — port HTTP server (default **8777**).
- `nyx_base_pool` — daftar ped id (0–311) yang boleh dipakai sebagai
  base, comma-separated. Id 0 (CJ) tidak pernah dipakai walau dicantumkan.
  Default: ped 264–293 (30 slot).

## HTTP server (Winsock, thread sendiri, HTTP/1.0, `Connection: close`)

Hanya method GET; selain itu 405. Log hanya untuk error HTTP (tidak ada
log per-request).

- `GET /nyx03dl/manifest` → `application/json`, array objek:
  `{"skinid","baseid","dff","txd","dff_crc32","txd_crc32","dff_size","txd_size"}`.
  **CRC32 ditulis sebagai angka desimal unsigned, BUKAN hex** — ini
  keputusan kontraktual untuk M2 (client .asi harus parse desimal).
- `GET /nyx03dl/file/<nama>` → `application/octet-stream` + `Content-Length`,
  isi file. Hanya file yang terdaftar di registry (cocok basename persis).
  Nama mengandung `/`, `\`, atau `..` → 403. File tidak terdaftar → 404.
- `GET /nyx03dl/players` → `application/json`, objek
  `{"<playerid>": <skinid>, ...}` hanya untuk player yang sedang connected.
- Path lain → 404.

## Cara kerja / threading

- Registry + base pool + mapping player→skin ada di `nyx03dl_core.h/.cpp`
  (portable, tanpa Windows/sampgdk — bisa di-compile native Linux untuk
  harness). Satu `std::mutex` melindungi registry dari thread AMX
  (natives) dan thread HTTP.
- Native sampgdk (`SetPlayerSkin`, `IsPlayerConnected`) HANYA dipanggil
  di thread utama server: dari native AMX itu sendiri dan dari
  `ProcessTick()` (yang me-refresh flag connected tiap ~500 ms untuk
  endpoint `/players`). Thread HTTP tidak pernah memanggil native —
  ia hanya membaca snapshot registry.
- `OnPlayerDisconnect` menghapus mapping langsung; polling
  `ProcessTick` adalah jaring pengaman kalau callback itu tidak jalan.
- CRC32: IEEE, poly 0xEDB88320 reflected, init 0xFFFFFFFF, final xor
  0xFFFFFFFF — sama seperti yang dipakai open.mp artwork dan .asi
  referensi. Test vector `CRC32("123456789") == 0xCBF43926` (terbukti di
  harness).
- Base pool default: ped 264–293 (30 slot). Dipilih dari ujung atas range
  ped yang jarang dipakai sebagai skin player di RP; identitas tiap ped
  BELUM diverifikasi in-game (tanpa GTA di VM) — override lewat
  `nyx_base_pool` kalau ada yang ternyata dipakai server.

## Verifikasi RakNet via sampgdk (LANGKAH 0)

**Jawaban: TIDAK — plugin server TIDAK bisa mengirim RPC RakNet custom
ke client lewat sampgdk.**

Bukti (dari `~/workspace/samp-td-mcp/build-deps/sampgdk/`):
- `grep -ri "raknet|rpc|bitstream|SendRPC"` di `include/`, `lib/`,
  `doc/`, `plugins/` tidak menemukan SATU PUN API pengiriman paket —
  satu-satunya yang mengandung "packet" adalah
  `NetStats_PacketLossPercent` (statistik read-only) di `a_samp.idl`.
- Yang diekspos sampgdk hanyalah native Pawn (`a_samp.idl`,
  `a_players.idl`, `a_objects.idl`, `a_vehicles.idl`, `a_actor.idl`,
  `a_http.idl`) + `logprintf`/timer/callback di `core.h` + tabel fungsi
  AMX (`PLUGIN_DATA_AMX_EXPORTS`). Tidak ada handle koneksi, tidak ada
  BitStream, tidak ada `SendRPC`.
- Satu-satunya jalur plugin↔server adalah tabel AMX + `Supports`/
  `Load`/`Unload`/`ProcessTick`/`AmxLoad` (lihat `sdk/plugincommon.h`).

Artinya Opsi B (manifest HTTP + polling, arsitektur yang dipilih di
`../ARCHITECTURE.md`) TETAP berlaku apa pun hasilnya — dan hasilnya
mengonfirmasi pilihan itu. RPC custom hanya mungkin dengan hooking
internal `samp-server.exe` (tidak didokumentasikan, di luar M1).

## File di folder ini

- `nyx03dl.cpp` — plugin utama (sampgdk + HTTP server + natives).
- `nyx03dl_core.h/.cpp` — logika portable (CRC32, registry, JSON).
- `sampgdk.c/.h`, `sdk/`, `zig-shim/` — copy dari `samp-td-mcp/plugin/`.
- `nyx03dl.def` — export: Supports, Load, Unload, ProcessTick,
  AmxLoad, AmxUnload, OnPlayerDisconnect.
- `build.sh` — build zig → `out/nyx03dl.dll`.
- `nyx03dl.cfg` — contoh config.
- `test-crc32.cpp` — harness Linux (CRC32, registry, manifest, players,
  traversal). Jalankan: `g++ -std=c++11 -O2 -Wall -Wextra -I. test-crc32.cpp nyx03dl_core.cpp -o /tmp/test-crc32 && /tmp/test-crc32`.
- `test-fixtures/` — file uji berlabel jelas (BUKAN model .dff/.txd
  asli — hanya byte pola/known untuk menguji CRC + registry).

## Batasan yang belum terverifikasi

- Tidak ada tes load di samp-server Windows (tanpa Windows di VM):
  `Load()`/`AmxLoad()`/native belum pernah dipanggil server sungguhan.
  Yang terverifikasi: DLL PE32 valid, 7 export ada, 3 nama native ada
  sebagai string (parser PE export table, lihat laporan M1).
- Handler HTTP (`handle_http_client`) adalah kode Winsock dan tidak bisa
  dijalankan di Linux; yang terverifikasi lewat harness adalah
  input-nya (lookup registry, JSON manifest/players, validasi nama file).
  Perilaku HTTP end-to-end (curl ke port 8777) baru bisa dites saat
  plugin di-load di samp-server Windows.
- `OnPlayerDisconnect` sebagai export plugin mengandalkan forwarding
  callback sampgdk (pola standar sampgdk); kalau tidak fire,
  `ProcessTick` tetap memangkas `/players` lewat polling
  `IsPlayerConnected` tiap ~500 ms.
- Identitas tiap ped di base pool default belum diverifikasi in-game.
