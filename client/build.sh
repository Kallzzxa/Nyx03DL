#!/bin/sh
# Build nyx03dl.asi with zig (https://ziglang.org/download/).
# Produces a 32-bit Windows DLL renamed to .asi (.asi IS a DLL), no Visual
# Studio or MinGW needed. Mirrors server/build.sh's way of invoking zig.
# Run from this directory (client/).
set -e
cd "$(dirname "$0")"

ZIG="${ZIG:-$HOME/workspace/build-deps/zig-x86_64-linux-0.16.0/zig}"
if [ ! -x "$ZIG" ]; then
  echo "[nyx03dl] zig not found at $ZIG" >&2
  exit 1
fi

mkdir -p out

"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/crc32.cpp -o crc32.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/manifest.cpp -o manifest.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/cache.cpp -o cache.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/log.cpp -o log.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -Ivendor/urmem -c src/chatui.cpp -o chatui.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/download.cpp -o download.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/http.cpp -o http.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/samp.cpp -o samp.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/raknet.cpp -o raknet.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/nyx_state.cpp -o nyx_state.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/monitor.cpp -o monitor.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/players.cpp -o players.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/skinload.cpp -o skinload.o
# M3 game/hook units need vendored urmem (vendor/urmem/urmem.hpp).
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -Ivendor/urmem -c src/game/rwmodel.cpp -o rwmodel.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -Ivendor/urmem -c src/game/render.cpp -o render.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -Ivendor/urmem -c src/game/handling.cpp -o handling.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -Ivendor/urmem -c src/skinhook.cpp -o skinhook.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -Isrc -c src/dllmain.cpp -o dllmain.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -shared -static-libstdc++ -static-libgcc \
  crc32.o manifest.o cache.o log.o chatui.o download.o http.o samp.o raknet.o nyx_state.o monitor.o players.o skinload.o rwmodel.o render.o handling.o skinhook.o dllmain.o \
  -lwininet -lshell32 -lws2_32 -o out/nyx03dl.dll
cp out/nyx03dl.dll out/nyx03dl.asi

rm -f crc32.o manifest.o cache.o log.o chatui.o download.o http.o samp.o raknet.o nyx_state.o monitor.o players.o skinload.o rwmodel.o render.o handling.o skinhook.o dllmain.o
# Drop the import lib / PDB zig leaves next to the DLL: the PDB embeds
# absolute build-machine paths and neither file ships with the .asi.
rm -f out/*.lib out/*.pdb
echo "[nyx03dl] OK: out/nyx03dl.asi built."
echo "Install: copy out/nyx03dl.asi next to gta_sa.exe (with an ASI loader)."
echo "Log: nyx03dl.log next to gta_sa.exe."
