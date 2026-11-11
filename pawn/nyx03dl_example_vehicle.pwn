// nyx03dl_example_vehicle.pwn — filterscript contoh Nyx03DL v1.3.0
// (REPLACE KENDARAAN, tanpa pawncmd dan tanpa sscanf2).
//
// Bedanya dengan contoh skin (nyx03dl_example_nopawncmd.pwn):
//   - Model kendaraan TIDAK punya ID custom 20000+ seperti skin.
//     NyxReplaceVeh(vehid, dff, txd) mengganti template model GTA
//     itu sendiri (400-611), jadi kendaraan di-spawn seperti biasa
//     dengan CreateVehicle(vehid, ...).
//   - Handling (kecepatan/akselerasi/rem) diatur terpisah lewat
//     NyxSetVehHandling — lihat OnFilterScriptInit di bawah.
//
// CARA PAKAI:
//   1. Taruh file .dff/.txd kendaraan di scriptfiles/nyx03dl/
//      (nama file di bawah ini HANYA contoh — ganti dengan file Anda).
//   2. #include <nyx03dl>, daftarkan kendaraan di OnGameModeInit /
//      OnFilterScriptInit SEBELUM pemain connect.
//   3. /veh [vehid] untuk spawn kendaraan. Spawn SETELAH download
//      .asi selesai (cek Nyx_HasClient) supaya model custom langsung
//      tampil — kendaraan yang sudah muncul sebagai model bawaan baru
//      berubah setelah re-stream / di-spawn ulang.
//
// INTEGRASI KE GAMEMODE (bukan filterscript):
//   - Pindahkan isi OnFilterScriptInit ke OnGameModeInit.
//   - Pindahkan blok /veh dan /vehs di OnPlayerCommandText ke gamemode
//     Anda (gabungkan kalau sudah ada OnPlayerCommandText, JANGAN
//     buat fungsi kedua).
//   - #include <nyx03dl> tetap di atas file gamemode.
//
// COMPILE (dari folder ini):
//   pawncc nyx03dl_example_vehicle.pwn -i. -i<pawno/include> -o nyx03dl_example_vehicle.amx
//   Butuh: a_samp.inc, nyx03dl.inc. Tidak butuh pawncmd.inc/sscanf2.inc.

#include <a_samp>
#include <nyx03dl>

// Warna pesan (format ARGB: 0xRRGGBBAA).
#define COLOR_PUTIH  0xFFFFFFFF
#define COLOR_MERAH  0xFF6347FF
#define COLOR_HIJAU  0x00C853FF
#define COLOR_KUNING 0xFFD600FF

// ---------------------------------------------------------------------------
// Daftar kendaraan contoh.
//
// UBAH bagian ini sesuai kendaraan Anda: satu baris per kendaraan.
// vehid = ID model GTA (400-611), mis. 400 Landstalker, 411 Infernus,
// 451 Turismo, 560 Sultan, 562 Elegy. Nama file harus persis sama
// dengan file di scriptfiles/nyx03dl/ (case-sensitive di Linux).
// File "veh400.dff" / "veh400.txd" di bawah ini HANYA nama contoh.
// ---------------------------------------------------------------------------
#define NYX_VEH_CONTOH_JUMLAH 1

new g_NyxVehId[NYX_VEH_CONTOH_JUMLAH] = {400};
new g_NyxVehDff[NYX_VEH_CONTOH_JUMLAH][32] =
{
    "veh400.dff"
};
new g_NyxVehTxd[NYX_VEH_CONTOH_JUMLAH][32] =
{
    "veh400.txd"
};

// Handling contoh per kendaraan (0.0 = biarkan bawaan).
// maxspeed = km/jam satuan handling.cfg, accel/brake = satuan cfg.
new Float:g_NyxVehMaxSpeed[NYX_VEH_CONTOH_JUMLAH] = {220.0};
new Float:g_NyxVehAccel[NYX_VEH_CONTOH_JUMLAH] = {2.0};
new Float:g_NyxVehBrake[NYX_VEH_CONTOH_JUMLAH] = {3.0};

new g_NyxVehTerdaftar[NYX_VEH_CONTOH_JUMLAH] = {0};

// ---------------------------------------------------------------------------
// OnFilterScriptInit — daftarkan kendaraan + handling ke plugin.
//
// >> KALAU DIINTEGRASIKAN KE GAMEMODE: pindahkan seluruh isi fungsi
//    ini ke OnGameModeInit. Pendaftaran HARUS terjadi sebelum pemain
//    connect.
// ---------------------------------------------------------------------------
public OnFilterScriptInit()
{
    new pesan[160];

    for (new i = 0; i < NYX_VEH_CONTOH_JUMLAH; i++)
    {
        if (!NyxReplaceVeh(g_NyxVehId[i], g_NyxVehDff[i], g_NyxVehTxd[i]))
        {
            format(pesan, sizeof(pesan),
                "[nyx03dl] GAGAL replace kendaraan %d (%s + %s). Cek file di scriptfiles/nyx03dl/",
                g_NyxVehId[i], g_NyxVehDff[i], g_NyxVehTxd[i]);
            print(pesan);
            continue;
        }
        g_NyxVehTerdaftar[i] = 1;
        format(pesan, sizeof(pesan),
            "[nyx03dl] Kendaraan %d di-replace (%s + %s).",
            g_NyxVehId[i], g_NyxVehDff[i], g_NyxVehTxd[i]);
        print(pesan);

        // Handling (opsional — 0.0 semua = bawaan). Harus SETELAH
        // NyxReplaceVeh, karena handling menempel di kendaraan yang
        // sudah terdaftar.
        if (!NyxSetVehHandling(g_NyxVehId[i], g_NyxVehMaxSpeed[i],
            g_NyxVehAccel[i], g_NyxVehBrake[i]))
        {
            format(pesan, sizeof(pesan),
                "[nyx03dl] GAGAL mengatur handling kendaraan %d.",
                g_NyxVehId[i]);
            print(pesan);
        }
        else
        {
            format(pesan, sizeof(pesan),
                "[nyx03dl] Kendaraan %d handling: maxspeed %.0f, accel %.1f, brake %.1f.",
                g_NyxVehId[i], g_NyxVehMaxSpeed[i],
                g_NyxVehAccel[i], g_NyxVehBrake[i]);
            print(pesan);
        }
    }

    print("[nyx03dl] Filterscript contoh kendaraan Nyx03DL dimuat. /veh [vehid] | /vehs");
    return 1;
}

public OnFilterScriptExit()
{
    print("[nyx03dl] Filterscript contoh kendaraan Nyx03DL dimatikan.");
    return 1;
}

// ---------------------------------------------------------------------------
// Helper: cek apakah string hanya berisi digit 0-9 (dan tidak kosong).
// Dipakai untuk parsing /veh tanpa sscanf2.
// ---------------------------------------------------------------------------
stock Nyx_IsAngka(const teks[])
{
    new len = strlen(teks);
    if (len == 0) return 0;
    for (new i = 0; i < len; i++)
    {
        if (teks[i] < '0' || teks[i] > '9') return 0;
    }
    return 1;
}

// ---------------------------------------------------------------------------
// OnPlayerCommandText — /veh dan /vehs tanpa pawncmd.
// ---------------------------------------------------------------------------
public OnPlayerCommandText(playerid, cmdtext[])
{
    new pesan[160];

    // --- /vehs (tanpa parameter) ---
    if (strcmp(cmdtext, "/vehs", true) == 0)
    {
        SendClientMessage(playerid, COLOR_KUNING, "Kendaraan custom yang terdaftar di server ini:");
        for (new i = 0; i < NYX_VEH_CONTOH_JUMLAH; i++)
        {
            if (g_NyxVehTerdaftar[i])
            {
                format(pesan, sizeof(pesan), "  - Kendaraan %d — /veh %d",
                    g_NyxVehId[i], g_NyxVehId[i]);
            }
            else
            {
                format(pesan, sizeof(pesan), "  - Kendaraan %d (GAGAL didaftarkan — cek server log)",
                    g_NyxVehId[i]);
            }
            SendClientMessage(playerid, COLOR_PUTIH, pesan);
        }
        return 1;
    }

    // --- /veh [vehid] ---
    if (strcmp(cmdtext, "/veh", true, 4) == 0
        && (cmdtext[4] == '\0' || cmdtext[4] == ' '))
    {
        new vehid;
        new arg[16];
        new j = 0;

        // Lewati "/veh", lalu lewati spasi.
        new i = 4;
        while (cmdtext[i] == ' ') i++;

        // Ambil argumen pertama (sampai spasi / akhir string).
        while (cmdtext[i] != '\0' && cmdtext[i] != ' ' && j < sizeof(arg) - 1)
        {
            arg[j++] = cmdtext[i++];
        }
        arg[j] = '\0';

        // Kosong / bukan angka -> tampilkan pemakaian.
        if (!Nyx_IsAngka(arg))
        {
            SendClientMessage(playerid, COLOR_KUNING, "Pemakaian: /veh [vehid 400-611]");
            SendClientMessage(playerid, COLOR_KUNING, "Lihat daftar kendaraan: /vehs");
            return 1;
        }

        vehid = strval(arg);

        if (vehid < NYX_MIN_VEHID || vehid > NYX_MAX_VEHID)
        {
            SendClientMessage(playerid, COLOR_MERAH, "Vehicle ID harus 400-611. Contoh: /veh 400");
            return 1;
        }

        // Peringatan (bukan penolakan): tanpa .asi, kendaraan muncul
        // sebagai model bawaan — tetap bisa dipakai, cuma tidak custom.
        if (!Nyx_HasClient(playerid))
        {
            SendClientMessage(playerid, COLOR_KUNING, "Kamu belum pakai nyx03dl.asi / download belum selesai — kendaraan muncul sebagai model bawaan.");
        }

        // Spawn di depan pemain, lalu masukkan pemain ke dalamnya.
        new Float:x, Float:y, Float:z, Float:angle;
        GetPlayerPos(playerid, x, y, z);
        GetPlayerFacingAngle(playerid, angle);
        // Sedikit ke depan supaya tidak menabrak pemain sendiri.
        x += 3.0 * floatsin(-angle, degrees);
        y += 3.0 * floatcos(-angle, degrees);

        new vehicleid = CreateVehicle(vehid, x, y, z, angle, -1, -1, -1);
        if (vehicleid == INVALID_VEHICLE_ID)
        {
            SendClientMessage(playerid, COLOR_MERAH, "Gagal membuat kendaraan. Coba lagi.");
            return 1;
        }
        PutPlayerInVehicle(playerid, vehicleid, 0);

        format(pesan, sizeof(pesan), "Kendaraan %d muncul.", vehid);
        SendClientMessage(playerid, COLOR_HIJAU, pesan);
        return 1;
    }

    return 0; // Bukan command Nyx03DL — biarkan gamemode/filterscript lain yang menangani.
}
