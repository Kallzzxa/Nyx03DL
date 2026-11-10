#!/bin/sh
# Build nyx03dl.dll with zig (https://ziglang.org/download/).
# Produces a 32-bit Windows DLL, no Visual Studio or MinGW needed.
# Follows the proven pattern of samp-td-mcp's build-plugin-zig.bat.
# Run from this directory (server/).
set -e
cd "$(dirname "$0")"

ZIG="${ZIG:-$HOME/workspace/build-deps/zig-x86_64-linux-0.16.0/zig}"
if [ ! -x "$ZIG" ]; then
  echo "[nyx03dl] zig not found at $ZIG" >&2
  exit 1
fi

"$ZIG" cc -target x86-windows-gnu -O2 -Wno-incompatible-function-pointer-types -I. -Isdk -Isdk/amx -Izig-shim -c sampgdk.c -o sampgdk.o
# amxplugin.c provides amx_Register/amx_GetAddr, which forward through
# the server's AMX function table (pAMXFunctions, set in Load()).
# tdbridge does not need it (no custom natives); nyx03dl does.
"$ZIG" cc -target x86-windows-gnu -O2 -Wno-incompatible-function-pointer-types -I. -Isdk -Isdk/amx -Izig-shim -c sdk/amx/amxplugin.c -o amxplugin.o
"$ZIG" c++ -target x86-windows-gnu -O2 -I. -Isdk -Isdk/amx -Izig-shim -c nyx03dl_core.cpp -o nyx03dl_core.o
"$ZIG" c++ -target x86-windows-gnu -O2 -I. -Isdk -Isdk/amx -Izig-shim -c nyx03dl.cpp -o nyx03dl.o
"$ZIG" c++ -target x86-windows-gnu -shared sampgdk.o amxplugin.o nyx03dl_core.o nyx03dl.o nyx03dl.def -lws2_32 -o out/nyx03dl.dll

rm -f sampgdk.o amxplugin.o nyx03dl_core.o nyx03dl.o
echo "[nyx03dl] OK: out/nyx03dl.dll built."
echo "Copy out/nyx03dl.dll to your server's plugins folder and add this"
echo "line to server.cfg:  plugins nyx03dl"
