#!/usr/bin/env bash
# Build the whole pawn-x stack: compiler (pawncc/pawnruns) + the dynhook plugin.
# Usage: tools/build.sh [--plugins-out DIR]   (default: repo-root/dist)
set -euo pipefail

root="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$root/dist"
[ "${1:-}" = "--plugins-out" ] && { out="$2"; shift 2; }
mkdir -p "$out"
cd "$root"

echo "==> 1/2  compiler (pawncc, pawnruns)"
cmake -S compiler/source/compiler -B build -DCMAKE_BUILD_TYPE=Release -DCMAKE_C_FLAGS="-m32" >/dev/null
cmake --build build -j"$(nproc)"

echo "==> 2/2  dynhook plugin (runtime hooks)"
if [ ! -f deps/subhook/subhook.c ]; then
  echo "    fetching subhook (upstream Zeex/subhook is gone; using Dasharo mirror)"
  git clone --depth 1 https://github.com/Dasharo/subhook.git deps/subhook
fi
gcc -m32 -fPIC -DSUBHOOK_STATIC -c deps/subhook/subhook.c -o "$out/subhook.o"
g++ -m32 -shared -fPIC -DLINUX -DSUBHOOK_STATIC -Icompiler/source/amx \
  -Icompiler/source/linux -Ideps/subhook \
  experiments/006-companion-plugin/dynhook.cpp "$out/subhook.o" -o "$out/dynhook.so"
rm -f "$out/subhook.o"

echo
echo "Done. Artifacts in $out :"
echo "  build/pawncc            compiler"
echo "  $out/dynhook.so         runtime-hook plugin"
echo "The set* operations are pure-Pawn stocks in <foreach>; iteration needs no plugin."
echo "Copy dynhook.so to the server's plugins/ and list it in config.json"
echo "(pawn.legacy_plugins for open.mp, or plugins for SA-MP)."

