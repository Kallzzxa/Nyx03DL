# nyx03dl — client .asi (M3: manifest probe + download + cache + clump swap)

SA-MP 0.3.7 **R5** client plugin (`.asi` = DLL 32-bit yang diganti nama)
untuk custom skins ala 0.3DL. Saat connect ke server, `.asi` ini mem-probe
manifest HTTP server Nyx, men-download file `.dff`/`.txd` yang belum ada
di cache (background thread, verifikasi CRC32), dan — sejak M3 — me-load
model RenderWare-nya dan men-swap clump template base ped-nya, sehingga
pemain yang memakai skin custom benar-benar terlihat memakai skin itu.
Sumber kebenaran "siapa pakai skin apa" adalah polling
`GET /nyx03dl/players` tiap ~2 detik; hook `ScrSetPlayerSkin` hanya
dipakai sebagai pemicu pre-swap sebelum ped di-rebuild.

M3 MENULIS memori game — tapi hanya dua tempat: field clump template
(`CBaseModelInfo` +0x1C) per base yang di-swap, dan patch JMP 5-byte di
dua alamat hook samp.dll (`CNetGame::Process`, `ScrSetPlayerSkin`).
Tidak ada yang lain yang ditulis. Aturan kerasnya ada di bawah
("JANGAN sentuh ped hidup").

**Status: build-only.** Belum ada tes in-game dan belum ada tes load
`.asi` di samp.dll sungguhan (tidak ada Windows/GTA di VM ini).
Yang terverifikasi di VM: compile bersih dengan zig, PE32 + entry point,
dan seluruh logika portable (CRC32, parser manifest/players, cache,
mapping pemain, flag restore) lewat harness Linux (`test/unit/test_m2.cpp`,
41/41 PASS; `test/unit/test_m3.cpp`, 73/73 PASS).

## Build (zig, tanpa MSVC)

```sh
./build.sh
```

Menghasilkan `out/nyx03dl.asi` (dan `out/nyx03dl.dll` yang isinya sama —
`.asi` hanyalah DLL yang diganti nama). Zig yang dipakai:
`~/workspace/build-deps/zig-x86_64-linux-0.16.0/zig` (0.16.0), perintah
persis ada di `build.sh`: `zig c++ -target x86-windows-gnu -O2 -g0
-std=c++17 -shared -static-libstdc++ -static-libgcc`, link
`-lwininet -lshell32 -lws2_32`.

- Static runtime: tidak ada dependensi `libstdc++-6.dll` /
  `libgcc_s_*.dll` / `libwinpthread-1.dll`. Import DLL yang terverifikasi
  ada di `.asi`: `WININET.DLL`, `SHELL32.dll`, `WS2_32.dll`,
  `KERNEL32.dll`, dan `api-ms-win-crt-*` (UCRT sistem, ada di Windows 10+).
- `-g0` + penghapusan `.lib`/`.pdb` di akhir build: tidak ada path
  absolut mesin build di dalam `.asi` (dicek dengan `strings`).
- M3 memakai `vendor/urmem/urmem.hpp` (header-only, di-vendor verbatim
  dari repo referensi, sha256 `6a948993…`) untuk kedua hook + pemanggilan
  fungsi game `__cdecl`. urmem ter-compile bersih dengan zig 0.16
  (diuji dulu dengan TU kecil sebelum dipakai) — jadi TIDAK perlu
  helper hook manual, dan TIDAK ada dependensi baru (no MinHook).

Catatan build yang sempat ditemui (supaya tidak diulang):
- `-static` bersama `-shared` di zig 0.16 **menimpa** `-shared` dan
  menghasilkan arsip statis, bukan DLL. Jangan tambahkan `-static`.
- `__DATE__`/`__TIME__` ditolak zig (`-Werror=date-time`); jangan dipakai.
- `winsock2.h` harus di-include SEBELUM `windows.h`.

## Install (di PC user, manual — Muse tidak memasang)

1. Copy `out/nyx03dl.asi` ke folder GTA SA, di sebelah `gta_sa.exe`
   (butuh ASI loader, misal `vorbisFile.dll` — sama seperti `.asi` lain).
2. Main SA-MP seperti biasa. Log ada di `nyx03dl.log` di sebelah
   `gta_sa.exe`. Cache ada di
   `Documents\GTA San Andreas User Files\SAMP\cache\nyx03dl\`.

## Cara kerja / state machine

```
DllMain (DLL_PROCESS_ATTACH)
  -> DisableThreadLibraryCalls + CreateThread monitor
  -> LogInit (nyx03dl.log)
  -> tunggu samp.dll sampai 60 dtk (tidak ada = bukan sesi SA-MP, keluar diam-diam)
  -> samp::Init + DumpFingerprint
  -> kalau fingerprint BUKAN R5-1: log warning + thread keluar
     (TIDAK membaca RVA R5 — alamat itu spesifik R5-1)
  -> M3: render::Init (hook CNetGame::Process) + skinhook::Init
     (hook ScrSetPlayerSkin) — gagal pasang = M3 off, M2 tetap jalan
  -> MonitorLoop (polling + drain + /players)
```

```
IDLE --connect--> PROBING --manifest OK--> DOWNLOADING --semua file valid--> READY
                     |                           ^      |
                     | probe gagal               |      +-- disconnect --> IDLE
                     v                           |      (model list + mapping
                   SLEEP --30 dtk--> PROBING ----+       pemain dibuang,
                                                  cache file TETAP,
                                                  restore-request flag
                                                  -> Tick RestoreAll)
```

- **Polling connect** (~500 ms): resolve RakClient lewat `kRefNetGame`
  (0x26EB94) + `CNetGame::GetRakClient` (0xBBC0, thiscall) lalu panggil
  `RakClientInterface::IsConnected()` (pola
  `reference/.../src/net/raknet.cpp`, tanpa hook).
- **Alamat server**: `GetServerID()` -> `PlayerID {binaryAddress, port}`.
  `binaryAddress` (network byte order) -> IP via `inet_ntoa`;
  `port` = game port (lihat asumsi byte order di bawah).
- **Probe manifest**: `GET http://<ip>:<httpport>/nyx03dl/manifest`,
  coba `gameport+1000` dulu, fallback `8777` (default M1). Timeout
  konek/baca 3 dtk (`INTERNET_OPTION_CONNECT_TIMEOUT` /
  `RECEIVE_TIMEOUT`) supaya server non-Nyx tidak menggantung monitor.
  User-Agent `SAMP/0.3` (konsisten dengan referensi; server M1 tidak
  mengeceknya). Sukses (HTTP 200 + JSON valid + array) -> state NYX,
  simpan model list. Gagal -> state SLEEP, log sekali, ulangi tiap
  ~30 dtk selama masih connected (menutupi GMX ke server Nyx nanti).
- **Download**: untuk tiap model, cek cache; file yang belum ada atau
  CRC-nya tidak cocok di-`Enqueue` dari
  `http://<ip>:<httpport>/nyx03dl/file/<nama persis dari manifest>`.
  Satu worker thread fetch serial (pola `reference/.../src/model/download.cpp`),
  fetch ke `<dest>.part`, verifikasi CRC32, baru rename ke tempatnya
  (jadi download gagal tidak pernah meninggalkan file korup di cache).
  Hasil di-`Drain` oleh monitor thread.
- **Disconnect**: `IsConnected` berubah false -> state IDLE, model list
  + mapping pemain dibuang, cache file TETAP di disk, siap probe ulang
  saat connect berikutnya. Pindah server saat masih connected (server
  key berubah) diperlakukan sama seperti reconnect. `NyxState::Clear()`
  juga menyalakan flag restore-request; `render::Tick` di game thread
  yang mengeksekusinya (`RestoreAll()` + free model) pada frame
  berikutnya — monitor TIDAK PERNAH menulis memori game sendiri.
- **Logging**: `nyx03dl.log` di sebelah `gta_sa.exe`: fingerprint,
  connect/disconnect, probe (URL + hasil), tiap enqueue/download
  selesai/gagal, cache hit/miss, hook install, swap/restore template.
  Polling tiap 500 ms dan `/players` tiap 2 dtk TIDAK di-log per poll
  (hanya transisi sukses/gagal).

## M3: model loading + apply skin (clump swapping)

### Alur per-frame (game thread, via hook `CNetGame::Process`)

Hook `CNetGame::Process` (RVA 0xB5B0, `__fastcall`, pola
`reference/.../src/game/render.cpp`): panggil original dulu, lalu
`render::Tick()`:

1. **Restore tertunda**: kalau monitor menyalakan flag restore
   (disconnect / ganti server), `RestoreAll()` + free semua model
   yang ter-load.
2. **Lazy-load**: untuk tiap model yang file-nya ready DAN ada pemain
   yang memakainya (`IsCustomAssigned`), load `.dff`+`.txd` dari cache
   via `rwmodel::LoadModel` (sekali per skin; skin yang tidak dipakai
   siapa pun TIDAK di-load).
3. **Swap**: `SwapOne` untuk tiap model yang ter-load — lihat bawah.
4. **Restore base yang sudah tidak dipakai**: base yang skin-nya tidak
   lagi dipakai pemain mana pun di-restore ke clump aslinya (mencegah
   bleed ke ped lain yang kebetulan pakai base sama).
5. **Free**: model ter-load yang tidak lagi dipakai siapa pun di-free.

### SwapOne (satu base template)

`render.cpp`, pola `reference/.../src/game/render.cpp`:

- Template base diambil dari `ms_modelInfoPtrs[baseid]` (0xA9B0C8) +
  field clump +0x1C. Clump asli disimpan SEKALI di `g_swapped`
  (base -> {original, custom, customId}).
- Kalau template sudah pegang clump kita -> diam (masih terpasang).
- Kalau template TIDAK lagi pegang clump kita (streamer unload base
  dan menghancurkan clump kita) -> drop pointer TANPA free ulang +
  `ReloadClump` dari cache, swap ulang frame berikutnya.
- Kalau template masih kosong (`*field == null`) -> `RequestModel`
  (0x4087E0, `CStreaming::RequestModel`, flag `0x10`) dan coba lagi
  frame berikutnya. Request di-throttle: max 1x per base per ~60 frame
  (~1 dtk) supaya tidak spam streamer.
- Gate `IsCustomAssigned(skinid)`: swap HANYA kalau ada pemain yang
  memakai skin itu.
- `RestoreAll()` (disconnect/GMX): kalau template masih pegang clump
  kita -> kembalikan clump asli; kalau template sudah diganti streamer
  -> JANGAN timpa clump baru, drop pointer tanpa free
  (`OnClumpLost(reload=false)`).

### Hook ScrSetPlayerSkin (pre-swap, read-only terhadap paket)

`skinhook.cpp`, RVA 0x19190, `__cdecl`, `RPCParameters*` (layout dari
`vendor/raknet/rakclient.h` referensi). Wire Nyx adalah 0.3.7 murni:
u32 PlayerID + u32 skin (64 bit). Hook HANYA membaca `playerId` dari
`p->input`, mengecek mapping `/players`, dan kalau pemain itu memakai
skin custom, memanggil `render::EnsureSwapForCustom(customId)` SEBELUM
memanggil original — supaya ped di-rebuild dari template yang SUDAH
di-swap (mencegah bug "skin baru muncul di percobaan kedua"). Hook
TIDAK PERNAH mengubah isi paket dan TIDAK PERNAH mengubah
`numberOfBitsOfData`. Tidak seperti referensi (`dlcompat.cpp`), hook
ini TIDAK menyimpan mapping sendiri — mapping hanya dari `/players`.

### Polling /players (monitor thread, ~2 dtk)

Selama state DOWNLOADING/READY: `GET
http://<ip>:<httpport>/nyx03dl/players` (pakai `http::Get` yang sama
dengan probe manifest, timeout 3 dtk), parse dengan `players.cpp`
(hand-rolled, pola `manifest.cpp`): objek `{"<playerid>": <skinid>}`,
playerid 0–999, skinid 20000–30000, tolak JSON rusak / key duplikat /
nilai di luar range tanpa crash. Sukses -> `NyxState::SetPlayerSkins`.
Gagal -> mapping lama DIBIARKAN (tidak dihapus); kegagalan hanya
di-log saat transisi (sekali saat mulai gagal, sekali saat pulih).
Disconnect / ganti server -> mapping dibersihkan + flag restore
dinyalakan (lihat state machine di atas).

### Aturan keras — JANGAN sentuh ped hidup

- Hanya field clump template (`CBaseModelInfo` +0x1C) yang di-swap.
  JANGAN panggil `DeleteRwObject` / `CreateRwObject` / `SetModelIndex`
  pada ped yang sudah hidup (indeks vtable CEntity 5/7/8 sengaja hanya
  didokumentasikan di `src/game/gta.h`, tidak pernah dipanggil).
  Game meng-clone template secara natural saat ped (re)spawn.
- Semua tulis memori game hanya dari game thread (hook `Process` /
  hook `ScrSetPlayerSkin`). Monitor thread hanya menulis `NyxState`
  + flag restore; worker download hanya menulis file cache.
- Semua hook dipasang HANYA setelah fingerprint R5 lolos, sekali saja
  (idempotent — `Init()` kedua = no-op). Bukan R5 -> log + M3 nonaktif,
  M2 tetap jalan.

### Hooking: urmem (bukan hook manual)

Keputusan: pakai `vendor/urmem/urmem.hpp` dari repo referensi
(header-only, 364 baris, di-vendor verbatim — sha256 sama dengan
aslinya). Alasan: ter-compile bersih dengan zig 0.16 dalam tes TU
kecil SEBELUM dipakai (hook `__fastcall` + call `__thiscall` /
`__cdecl` + `get_func_addr`), jadi tidak ada alasan menulis helper
hook manual; pola `hook.install` + `hook.call` (disable sementara ->
call original -> enable lagi) sama persis dengan referensi. Tidak ada
MinHook / dependensi lain yang ditarik. Satu residu yang jujur dicatat:
pemasangan patch JMP 5-byte dilakukan dari monitor thread sementara
game thread bisa saja mengeksekusi fungsi yang sama di saat bersamaan
— referensi melakukan hal yang sama; window-nya kecil (satu write 5
byte, sekali seumur proses), tapi tetap belum terverifikasi runtime.

## Slot vtable RakClient — cara menghitungnya

samp.dll dibangun dengan MSVC; `.asi` ini dibangun dengan zig/MinGW
(Clang, Itanium C++ ABI). Class C++ hasil kompilasi Itanium ABI punya
layout vtable yang beda dari MSVC (Itanium mengeluarkan DUA slot
destructor, MSVC SATU), jadi memanggil `rak->IsConnected()` lewat
interface C++ lokal akan dispatch ke slot yang salah. Karena itu M2
membaca pointer vtable sendiri dan memanggil dua slot yang dibutuhkan
sebagai function pointer `__thiscall` mentah (lihat `src/raknet.cpp`).

Slot dihitung dari urutan deklarasi virtual di
`reference/.../vendor/raknet/rakclient.h`, dengan destructor MSVC = slot 0:

| slot | fungsi |
| --- | --- |
| 0 | `~RakClientInterface` (destructor — MSVC satu slot) |
| 1 | Connect · 2 Disconnect · 3 InitializeSecurity · 4 SetPassword · 5 HasPassword |
| 6 | Send(const char*, ...) · 7 Send(BitStream*, ...) · 8 Receive · 9 DeallocatePacket |
| 10 | PingServer() · 11 PingServer(host, ...) · 12 GetAveragePing · 13 GetLastPing |
| 14 | GetLowestPing · 15 GetPlayerPing · 16 StartOccasionalPing · 17 StopOccasionalPing |
| **18** | **IsConnected** |
| 19 | GetSynchronizedRandomInteger · 20 GenerateCompressionLayer · 21 DeleteCompressionLayer |
| 22 | RegisterAsRemoteProcedureCall · 23 RegisterClassMemberRPC · 24 UnregisterAsRemoteProcedureCall |
| 25 | RPC(const char*, ...) · 26 RPC(BitStream*, ...) · 27 RPC_ |
| 28 | SetTrackFrequencyTable · 29 GetSendFrequencyTable · 30 GetCompressionRatio · 31 GetDecompressionRatio |
| 32 | AttachPlugin · 33 DetachPlugin · 34 GetStaticServerData · 35 SetStaticServerData |
| 36 | GetStaticClientData · 37 SetStaticClientData · 38 SendStaticClientDataToServer |
| **39** | **GetServerID** |
| 40 | GetPlayerID · 41 GetInternalID · 42 PlayerIDToDottedIP · ... |

## Asumsi byte order PlayerID (penting, belum terverifikasi runtime)

- `PlayerID.binaryAddress` dalam **network byte order** (berasal dari
  `inet_addr` / wire), jadi langsung diberikan ke `inet_ntoa()`.
- `PlayerID.port` dalam **host byte order** (RakNet menyimpan port
  host-order di PlayerID; SA-MP sendiri memakai `GetServerID().port`
  langsung sebagai nomor port server).
- `PlayerID` adalah `#pragma pack(1)` di `rakclient.h`: 4 byte
  binaryAddress + 2 byte port = **6 byte**. MSVC mengembalikan struct
  <= 8 byte di EAX:EDX, jadi `GetServerID` dipanggil di sini sebagai
  fungsi yang mengembalikan `uint64_t`, lalu 48 bit bawah di-decode
  (32 bit bawah = binaryAddress, bit 32..47 = port, bit 48..63 diabaikan).
- Kalau asumsi port ini salah, probe manifest gagal secara aman
  (SLEEP + retry, tidak crash) dan log akan menunjukkan port yang salah
  — itu gejala yang mudah dikenali saat tes di PC.

## Cache

- Direktori: `Documents\GTA San Andreas User Files\SAMP\cache\nyx03dl\`
  (dibuat otomatis). SATU direktori untuk semua server, content-addressed:
  file yang sama CRC-nya tidak di-download ulang walau dari server lain.
  Ini sengaja beda dari layout open.mp (`cache/<ip_port>/`) dan dari
  referensi (`cache/customskin037` — itu nama plugin lain).
- Nama file: `<CRC hex 8 digit huruf besar>.<dff|txd>`, misal
  `CBF43926.dff` — persis pola referensi (`cache.cpp`).
- `HasValid(crc, isDff)` = file ada DAN CRC32-nya cocok. File korup /
  setengah ter-download akan di-download ulang (worker menulis ke
  `.part` dulu, lihat atas).

## Kontrak dengan server M1 (dipatuhi)

- Manifest: `GET /nyx03dl/manifest` -> array
  `{skinid, baseid, dff, txd, dff_crc32, txd_crc32, dff_size, txd_size}`.
  **CRC32 di JSON adalah angka DESIMAL unsigned, BUKAN hex** — parser
  memakai basis 10; nilai hex / string / negatif / > 4294967295 ditolak.
- File: `GET /nyx03dl/file/<nama>` — hanya nama persis dari manifest
  yang diminta (parser menolak nama berisi `/`, `\`, atau `..`).
- Pemain: `GET /nyx03dl/players` -> objek `{"<playerid>": <skinid>}`
  hanya untuk player yang sedang connected; `.asi` mem-poll endpoint
  ini tiap ~2 detik (M3) dan TIDAK PERNAH mengubah paket
  `SetPlayerSkin` — wire tetap 0.3.7 murni (baseid).
- Port HTTP default 8777 (= game-port 7777 + 1000). Skin ID 20000–30000.
- CRC32: IEEE poly 0xEDB88320 reflected, init 0xFFFFFFFF, final xor
  0xFFFFFFFF. Test vector: CRC32("123456789") == 0xCBF43926.

## Seam untuk M3 (`src/nyx_state.h`)

`NyxState& GlobalState()` — singleton thread-safe (mutex), ditulis
monitor thread, dibaca M3. M3 TIDAK boleh memegang lock sambil
menyentuh game state; `Models()` / `PlayerSkins()` mengembalikan
snapshot (copy).

```cpp
struct Model {
    int skinid, baseid;                 // skinid 20000..30000, base ped 0..311
    uint32_t dffCrc, txdCrc, dffSize, txdSize;
    std::string dffName, txdName;       // nama file dari manifest
    std::string dffPath, txdPath;       // path absolut di cache
    bool dffReady, txdReady;            // file ada DAN CRC cocok
};

class NyxState {
    void SetManifest(const std::vector<Model>&, const std::string& serverKey,
                     uint16_t httpPort);   // dipanggil monitor saat probe sukses
    void Clear();                          // disconnect / ganti server
                                           // (+ hapus mapping + RequestRestore)
    std::vector<Model> Models() const;     // snapshot
    bool IsReady() const;                  // manifest ada DAN semua file valid
    bool HasManifest() const;
    std::string ServerKey() const;         // "ip:gameport" server saat ini
    uint16_t HttpPort() const;             // port HTTP yang dipakai probe
    void MarkDownloaded(int skinid, bool isDff, bool ok);
    void UpdateFromCache();                // refresh semua flag dari disk
    // M3: mapping pemain (monitor tulis, game thread baca)
    void SetPlayerSkins(const std::map<int, int>& skins);
    std::map<int, int> PlayerSkins() const;   // snapshot
    int SkinForPlayer(int playerid) const;    // 0 kalau tidak pakai custom
    bool IsCustomAssigned(int skinid) const;  // ada pemain yang memakai?
    void ClearPlayerSkins();
    // M3: restore-request flag (monitor set, Tick consume — satu-satunya
    // sinyal lintas thread ke renderer; game memory tidak pernah ditulis
    // dari monitor thread)
    void RequestRestore();
    bool TakeRestoreRequest();             // hanya dari game thread
};
```

## JSON parser (`src/manifest.cpp`, `src/players.cpp`)

Hand-rolled, tanpa library eksternal (skema tetap dan kecil). Mendukung
escape `\" \\ \/ \b \f \n \r \t` dan `\uXXXX` (ASCII saja). Menolak:
JSON rusak / terpotong, field hilang, key tidak dikenal, angka negatif,
angka > uint32, skinid di luar 20000–30000, baseid di luar 0–311,
CRC/size nol, nama file berisi path. Penolakan tidak pernah crash —
`ParseManifest` mengembalikan false + alasan.

`ParsePlayers` (`{"<playerid>": <skinid>}`) memakai aturan yang sama:
playerid harus string angka 0–999, skinid 20000–30000, key duplikat
ditolak, objek kosong `{}` valid. Dokumen yang ditolak membuat monitor
mempertahankan mapping lama.

## File di folder ini

- `src/crc32.h/.cpp` — CRC-32/IEEE (adaptasi referensi; tabel dibangun
  dengan `std::call_once` supaya aman dipanggil monitor + worker).
- `src/manifest.h/.cpp` — parser manifest hand-rolled (baru, M2).
- `src/cache.h/.cpp` — cache dir + nama file CRC + HasValid
  (adaptasi referensi; dir tunggal `.../cache/nyx03dl/`).
- `src/log.h/.cpp` — logger file `nyx03dl.log` (adaptasi referensi).
- `src/download.h/.cpp` — Fetch WinINet + worker + queue + Drain
  (adaptasi referensi; Result membawa `skinid`; fetch ke `.part`).
- `src/http.h/.cpp` — HTTP GET dengan timeout, untuk probe manifest (baru).
- `src/samp.h/.cpp` — resolve samp.dll + fingerprint R5 (adaptasi
  referensi, tanpa urmem/DebugChat).
- `src/raknet.h/.cpp` — ResolveRakClient / IsConnected / GetServerAddress
  via vtable mentah (baru, dari `rakclient.h` + `raknet.cpp` referensi).
- `src/nyx_state.h/.cpp` — seam M3 (baru di M2, diperluas di M3:
  mapping pemain + flag restore).
- `src/monitor.h/.cpp` — state machine + probe + drain (baru di M2,
  diperluas di M3: polling `/players` tiap ~2 dtk).
- `src/players.h/.cpp` — parser JSON `/players` hand-rolled (baru, M3).
- `src/skinload.h/.cpp` — ready-model manager: lazy-load dari cache,
  repair clump yang dihancurkan streamer, free skin yang tidak dipakai
  (baru, M3 — pengganti `net/artwork.*` referensi).
- `src/game/gta.h` — alamat GTA SA 1.0 US + konstanta RenderWare
  (baru, M3 — adaptasi referensi).
- `src/game/rwmodel.h/.cpp` — LoadModel / Free / ReloadClump /
  RequestModel via urmem (baru, M3 — adaptasi referensi).
- `src/game/render.h/.cpp` — SwapOne + g_swapped + Tick + RestoreAll
  + EnsureSwapForCustom + hook `CNetGame::Process` (baru, M3 —
  adaptasi referensi).
- `src/skinhook.h/.cpp` — hook `ScrSetPlayerSkin` read-only, pemicu
  pre-swap (baru, M3 — adaptasi `dlcompat.cpp` referensi TANPA
  packet rewriting).
- `src/dllmain.cpp` — DllMain + monitor thread (adaptasi referensi;
  M3: install hook setelah fingerprint R5 lolos).
- `vendor/urmem/urmem.hpp` — hook helper header-only, di-vendor
  verbatim dari repo referensi (baru, M3).
- `test/unit/test_m2.cpp` — harness Linux (CRC32, parser, cache).
- `test/unit/test_m3.cpp` — harness Linux M3 (parser `/players`,
  mapping pemain, flag restore, transisi gate).
- `test/fixtures/` — file uji (BUKAN model asli) + `manifest.json`
  contoh dengan CRC desimal ASLI dari file fixture.
- `build.sh`, `LICENSE` (GPL-3.0), `README.md` (file ini).

## Verifikasi (di VM, 2026-11-10)

```sh
./build.sh
# [nyx03dl] OK: out/nyx03dl.asi built.
file out/nyx03dl.asi
# out/nyx03dl.asi: PE32 executable (DLL) (GUI) Intel 80386, for MS Windows, 9 sections
sha256sum out/nyx03dl.asi
# e8e4470eff3a48dc6f9839e186e20e4e151c8c5d95674c83feea1223131b5a1d
# ukuran 573952 byte
```

Import DLL terverifikasi (via `objdump -p`): `WININET.DLL`,
`SHELL32.dll`, `WS2_32.dll`, `KERNEL32.dll`, `api-ms-win-crt-*` —
tidak ada `libstdc++-6.dll` / `libgcc_s_*` / `libwinpthread-1.dll`.
`strings` pada `.asi`: 0 hit path absolut VM (`/home/`, `/tmp/`).

Harness unit (portable, tanpa Windows):

```sh
g++ -std=c++17 -O2 -Wall -Wextra -Isrc test/unit/test_m2.cpp \
    src/crc32.cpp src/manifest.cpp src/cache.cpp src/log.cpp \
    -o /tmp/test_m2 && /tmp/test_m2
# 41 checks, 0 failures
g++ -std=c++17 -O2 -Wall -Wextra -Isrc test/unit/test_m3.cpp \
    src/players.cpp src/nyx_state.cpp src/cache.cpp src/log.cpp \
    src/crc32.cpp -o /tmp/test_m3 && /tmp/test_m3
# 73 checks, 0 failures
```

## Batasan jujur (baca sebelum mengklaim apa pun)

- **TIDAK ADA tes in-game di VM** (tanpa Windows/GTA). **TIDAK ADA tes
  load `.asi` di samp.dll sungguhan.** Yang diuji di VM hanyalah:
  compile bersih, PE32 + imports + strings, dan logika portable
  lewat harness. Kode yang memanggil alamat game (`rwmodel`,
  `render`, `skinhook`, hook urmem) TIDAK BISA dites di VM —
  benar-tidaknya alamat 0xB5B0 / 0x19190 / 0xA9B0C8 / 0x4087E0 /
  fungsi RenderWare, slot vtable, dan perilaku streamer baru akan
  ketahuan saat tes di PC user. Kalau ada yang meleset, gejala yang
  diharapkan: skin tidak muncul (aman) atau crash saat swap/hook
  (risiko nyata dari penulisan memori game — M3, tidak seperti M2,
  memang menulis memori game).
- WinINet fetch, slot vtable RakClient (IsConnected=18, GetServerID=39),
  dan asumsi byte order PlayerID **belum terverifikasi runtime** — baru
  review kode + harness unit. Kalau slot vtable meleset satu, gejala
  yang diharapkan: monitor menganggap tidak pernah connect (atau
  connect terus-menerus) dan log akan menunjukkannya.
- Target HANYA SA-MP 0.3.7 R5-1 (`SizeOfImage` 0x27E000,
  `TimeDateStamp` 0x6372C39E). Build lain ditolak dengan sengaja.
- Pemasangan hook JMP 5-byte dilakukan dari monitor thread sementara
  game thread berjalan (sama seperti referensi) — window race kecil
  yang belum terverifikasi runtime.
- **Build-only sampai user tes di PC-nya.**

## Lisensi & atribusi

`.asi` ini turunan kode **ricardoofnl/customskin-0.3.7-R5** (GPL-3.0),
jadi `.asi` ini juga **GPL-3.0** — lihat `LICENSE`. File yang diadaptasi
dari repo tersebut membawa header atribusi di atasnya:

- `src/crc32.*`, `src/cache.*`, `src/log.*`, `src/download.*`,
  `src/samp.*`, `src/dllmain.cpp` — diadaptasi dari
  ricardoofnl/customskin-0.3.7-R5 (GPL-3.0).
- `src/raknet.*` — dari `vendor/raknet/rakclient.h` +
  `src/net/raknet.cpp` repo yang sama.
- `src/game/gta.h`, `src/game/rwmodel.*`, `src/game/render.*`,
  `src/skinhook.*` — diadaptasi dari `src/game/gta.h`,
  `src/game/rwmodel.*`, `src/game/render.*`, `src/net/dlcompat.cpp`
  repo yang sama (M3; `skinhook` TANPA packet rewriting referensi).
- `vendor/urmem/urmem.hpp` — di-vendor verbatim dari repo yang sama.
- `src/manifest.*`, `src/http.*`, `src/nyx_state.*`,
  `src/monitor.*` — ditulis baru untuk Nyx03DL (GPL-3.0 juga).
- `src/players.*`, `src/skinload.*` — ditulis baru untuk Nyx03DL
  (M3, GPL-3.0 juga; `skinload` menggantikan `net/artwork.*`
  referensi sebagai sumber model siap).
