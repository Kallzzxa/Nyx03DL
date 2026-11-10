// nyx03dl_example.pwn — filterscript contoh Nyx03DL.
//
// Menunjukkan pemakaian lengkap plugin server Nyx03DL:
//   - mendaftarkan skin custom di OnFilterScriptInit
//   - /setskin [skinid]  -> ganti skin pemain ke skin custom
//   - /skins             -> lihat skin custom yang terdaftar
//
// INTEGRASI KE GAMEMODE (bukan filterscript):
//   - Pindahkan isi OnFilterScriptInit di bawah ke OnGameModeInit
//     gamemode Anda (tetap SEBELUM pemain connect).
//   - Pindahkan CMD:setskin dan CMD:skins apa adanya ke gamemode.
//   - #include <nyx03dl> tetap di atas file gamemode.
//   - Jangan lupa: file .dff/.txd ASLI harus sudah ada di
//     scriptfiles/nyx03dl/ sebelum server dijalankan, dan plugin
//     nyx03dl.dll harus di-load lewat server.cfg.
//
// COMPILE (dari folder ini):
//   pawncc nyx03dl_example.pwn -i. -i<pawno/include> -o nyx03dl_example.amx
//   (lihat README.md di folder ini untuk perintah persis.)
//   Butuh: a_samp.inc, pawncmd.inc, sscanf2.inc, nyx03dl.inc.
//   pawncmd.inc + sscanf2.inc TIDAK ikut di-ship di sini — gamemode
//   Anda sudah punya keduanya; yang di-ship hanya nyx03dl.inc.
//
// CATATAN: filterscript contoh ini TIDAK memakai dialog (easydialog)
// supaya tetap sederhana. Pesan ke pemain memakai SendClientMessage.

#include <a_samp>
#include <pawncmd>
#include <sscanf2>
#include <nyx03dl>

// Warna pesan (format ARGB: 0xRRGGBBAA).
#define COLOR_PUTIH  0xFFFFFFFF
#define COLOR_MERAH  0xFF6347FF
#define COLOR_HIJAU  0x00C853FF
#define COLOR_KUNING 0xFFD600FF

// ---------------------------------------------------------------------------
// Daftar skin contoh.
//
// UBAH bagian ini sesuai skin Anda: satu baris per skin, dengan nama
// file .dff/.txd yang persis sama dengan file di scriptfiles/nyx03dl/
// (case-sensitive di Linux). File .dff/.txd ASLI dari Anda — file
// "skin1.dff" / "skin2.dff" di bawah ini HANYA nama contoh, bukan
// file yang ikut di-ship.
// ---------------------------------------------------------------------------
#define NYX_CONTOH_JUMLAH 2

new g_NyxSkinId[NYX_CONTOH_JUMLAH] = {20000, 20001};
new g_NyxDff[NYX_CONTOH_JUMLAH][32] =
{
    "skin1.dff",
    "skin2.dff"
};
new g_NyxTxd[NYX_CONTOH_JUMLAH][32] =
{
    "skin1.txd",
    "skin2.txd"
};

// Hasil pendaftaran: baseid yang di-assign plugin + flag terdaftar.
// (Diisi di OnFilterScriptInit; jangan diisi manual.)
new g_NyxBaseId[NYX_CONTOH_JUMLAH] = {NYX_INVALID_BASE, NYX_INVALID_BASE};
new g_NyxTerdaftar[NYX_CONTOH_JUMLAH] = {0, 0};

// ---------------------------------------------------------------------------
// OnFilterScriptInit — daftarkan semua skin contoh ke plugin.
//
// >> KALAU DIINTEGRASIKAN KE GAMEMODE: pindahkan seluruh isi fungsi
//    ini ke OnGameModeInit. Pendaftaran HARUS terjadi sebelum pemain
//    connect — pemain yang sudah connect tidak akan men-download skin
//    yang didaftarkan belakangan.
// ---------------------------------------------------------------------------
public OnFilterScriptInit()
{
    new baseid;
    new pesan[128];

    for (new i = 0; i < NYX_CONTOH_JUMLAH; i++)
    {
        baseid = SetCostumNyx03DL(g_NyxSkinId[i], g_NyxDff[i], g_NyxTxd[i]);

        if (baseid == NYX_INVALID_BASE)
        {
            // Gagal daftar (file tidak ada / skinid salah / pool habis).
            // Hanya warning ke server log — JANGAN crash server.
            format(pesan, sizeof(pesan),
                "[nyx03dl] GAGAL mendaftarkan skin %d (%s + %s). Cek file di scriptfiles/nyx03dl/",
                g_NyxSkinId[i], g_NyxDff[i], g_NyxTxd[i]);
            print(pesan);
        }
        else
        {
            g_NyxBaseId[i] = baseid;
            g_NyxTerdaftar[i] = 1;
            format(pesan, sizeof(pesan),
                "[nyx03dl] Skin %d terdaftar (base ped %d).",
                g_NyxSkinId[i], baseid);
            print(pesan);
        }
    }

    print("[nyx03dl] Filterscript contoh Nyx03DL dimuat. /setskin [skinid] | /skins");
    return 1;
}

public OnFilterScriptExit()
{
    print("[nyx03dl] Filterscript contoh Nyx03DL dimatikan.");
    return 1;
}

// ---------------------------------------------------------------------------
// /setskin [skinid] — ganti skin pemain ke skin custom yang terdaftar.
//
// >> KALAU DIINTEGRASIKAN KE GAMEMODE: pindahkan fungsi ini apa adanya.
//    Gamemode Society RP memakai pawncmd, jadi tetap CMD: seperti ini
//    (BUKAN OnPlayerCommandText).
// ---------------------------------------------------------------------------
CMD:setskin(playerid, params[])
{
    new skinid;
    new baseid;
    new pesan[128];

    // Parse 1 angka. Kosong / bukan angka -> tampilkan pemakaian.
    if (sscanf(params, "d", skinid))
    {
        SendClientMessage(playerid, COLOR_KUNING, "Pemakaian: /setskin [skinid 20000-30000]");
        SendClientMessage(playerid, COLOR_KUNING, "Lihat daftar skin: /skins");
        return 1;
    }

    // Skin ID harus di range custom (20000-30000).
    if (skinid < NYX_MIN_SKINID || skinid > NYX_MAX_SKINID)
    {
        SendClientMessage(playerid, COLOR_MERAH, "Skin ID harus 20000-30000. Contoh: /setskin 20000");
        return 1;
    }

    // Skin harus sudah terdaftar lewat SetCostumNyx03DL.
    baseid = Nyx_GetBaseSkin(skinid);
    if (baseid == NYX_INVALID_BASE)
    {
        format(pesan, sizeof(pesan), "Skin %d belum terdaftar di server ini.", skinid);
        SendClientMessage(playerid, COLOR_MERAH, pesan);
        return 1;
    }

    // Terapkan skin. Plugin memanggil SetPlayerSkin ASLI dengan baseid.
    if (!Nyx_SetPlayerSkin(playerid, skinid))
    {
        SendClientMessage(playerid, COLOR_MERAH, "Gagal mengganti skin. Coba lagi.");
        return 1;
    }

    format(pesan, sizeof(pesan), "Skin diganti ke %d (base ped %d).", skinid, baseid);
    SendClientMessage(playerid, COLOR_HIJAU, pesan);
    return 1;
}

// ---------------------------------------------------------------------------
// /skins — daftar skin custom yang terdaftar di filterscript ini.
//
// >> KALAU DIINTEGRASIKAN KE GAMEMODE: pindahkan fungsi ini apa adanya.
//    (Opsional — boleh dihapus kalau gamemode Anda sudah punya daftar
//    skin sendiri.)
// ---------------------------------------------------------------------------
CMD:skins(playerid, params[])
{
    #pragma unused params
    new pesan[128];

    SendClientMessage(playerid, COLOR_KUNING, "Skin custom yang terdaftar di server ini:");
    for (new i = 0; i < NYX_CONTOH_JUMLAH; i++)
    {
        if (g_NyxTerdaftar[i])
        {
            format(pesan, sizeof(pesan), "  - Skin %d (base ped %d) — /setskin %d",
                g_NyxSkinId[i], g_NyxBaseId[i], g_NyxSkinId[i]);
        }
        else
        {
            format(pesan, sizeof(pesan), "  - Skin %d (GAGAL didaftarkan — cek server log)",
                g_NyxSkinId[i]);
        }
        SendClientMessage(playerid, COLOR_PUTIH, pesan);
    }
    return 1;
}
