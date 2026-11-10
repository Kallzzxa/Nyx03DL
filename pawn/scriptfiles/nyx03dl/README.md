# scriptfiles/nyx03dl/ — tempat file model skin custom
#
# File .dff/.txd ASLI ditaruh DI SINI.
#
# Aturan:
#   1. Satu skin = 1 pasang file: 1x .dff + 1x .txd.
#      Contoh: skin1.dff + skin1.txd = skin 20000.
#   2. Nama file harus PERSIS SAMA dengan yang didaftarkan di Pawn
#      lewat SetCostumNyx03DL(skinid, "nama.dff", "nama.txd").
#      Case-sensitive di Linux: "Skin1.dff" != "skin1.dff".
#   3. Daftarkan skin di OnGameModeInit / OnFilterScriptInit,
#      SEBELUM pemain connect. Kalau daftar belakangan, pemain yang
#      sudah connect tidak akan men-download skin itu (client .asi
#      hanya mem-probe manifest saat connect).
#   4. File di sini dibaca plugin server (CRC32 + ukuran dihitung
#      dari file ini) dan di-download client .asi lewat HTTP.
#      Jangan taruh file lain yang tidak didaftarkan — file yang
#      tidak terdaftar tidak akan disajikan ke client.
#
# PENTING: folder ini TIDAK berisi model asli apa pun. File
# "skin1.dff" / "skin2.dff" yang disebut di nyx03dl_example.pwn
# hanyalah NAMA CONTOH. Taruh file .dff/.txd asli Anda sendiri di sini.
