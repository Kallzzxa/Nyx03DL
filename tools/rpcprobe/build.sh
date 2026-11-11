#!/bin/sh
# Build rpcprobe.asi (diagnostic) with zig. Run from client/probe/.
# Produces a 32-bit Windows DLL renamed to .asi. Self-contained: only
# rpcprobe.cpp + vendored urmem, no nyx03dl sources.
set -e
cd "$(dirname "$0")"

ZIG="${ZIG:-$HOME/workspace/build-deps/zig-x86_64-linux-0.16.0/zig}"
if [ ! -x "$ZIG" ]; then
  echo "[rpcprobe] zig not found at $ZIG" >&2
  exit 1
fi

mkdir -p out

"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -I../src -I../vendor/urmem -c rpcprobe.cpp -o rpcprobe.o
"$ZIG" c++ -target x86-windows-gnu -O2 -g0 -std=c++17 -shared -static-libstdc++ -static-libgcc \
  rpcprobe.o -o out/rpcprobe.dll
cp out/rpcprobe.dll out/rpcprobe.asi

rm -f rpcprobe.o
rm -f out/*.lib out/*.pdb
echo "[rpcprobe] OK: out/rpcprobe.asi built."
echo "Install: copy out/rpcprobe.asi next to gta_sa.exe (same ASI loader as nyx03dl.asi)."
echo "Log: rpcprobe.log next to gta_sa.exe."
