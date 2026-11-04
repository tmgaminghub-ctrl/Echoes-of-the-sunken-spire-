#!/bin/bash
# ---------------------------------------------------------------------------
#  build_exe.sh - one self-contained, portable Windows .exe
#
#    ./build_exe.sh          -> ../builds/Echoes-of-the-Sunken-Spire.exe
#
#  The whole web/public/assets tree is baked into the executable, so there is no
#  folder to copy, no installer and no config: drop the .exe anywhere and
#  double-click it. Requires a mingw-w64 cross compiler on Linux:
#
#    sudo apt install g++-mingw-w64-x86-64 binutils-mingw-w64-x86-64
#
#  It also emits a Linux binary with the same embedded assets (same code path)
#  so the asset lookup can be tested without Windows.
# ---------------------------------------------------------------------------
set -euo pipefail
cd "$(dirname "$0")"

WEB="$(cd ../web && pwd)"
OUT="$(cd .. && pwd)/builds"
mkdir -p "$OUT"

# win32 thread model: std::thread maps onto Win32 threads, so there is no
# statically linked libwinpthread blob. That matters beyond size - a large
# static pthread inside a mingw binary is a known antivirus false-positive
# trigger and makes the exe look suspicious to SmartScreen/Defender.
CXX_WIN=${CXX_WIN:-x86_64-w64-mingw32-g++-win32}
LD_WIN=${LD_WIN:-x86_64-w64-mingw32-ld}
RC_WIN=${RC_WIN:-x86_64-w64-mingw32-windres}

FLAGS="-std=c++17 -O2 -w -D_CRT_SECURE_NO_WARNINGS -DKAYKIT_EMBEDDED_ASSETS"
WLIBS="-luser32 -lgdi32 -lxinput -static -static-libgcc -static-libstdc++"
# -mwindows: GUI subsystem, so double-clicking does not open a console window
WFLAGS="-mwindows"

GAME=spire
NAME=Echoes-of-the-Sunken-Spire
B="/tmp/kaykit-exe-$GAME"
rm -rf "$B"; mkdir -p "$B"

echo "=== $GAME: packing $WEB/public/assets"
python3 tools/embed_assets.py --assets "$WEB/public/assets" --game "$GAME" --out "$B"

# the blob has to be linked from a relative path, otherwise ld names the
# symbols after the absolute path and nothing resolves
( cd "$B" && $LD_WIN -r -b binary -o assets_blob.o assets_blob.bin )

echo "=== $GAME: compiling resources (icon + version info)"
$RC_WIN -I win -i "win/$GAME.rc" -O coff -o "$B/app.res"

echo "=== $GAME: compiling Windows x64 -> $OUT/$NAME.exe"
$CXX_WIN $FLAGS $WFLAGS -I src -o "$OUT/$NAME.exe" \
    src/*.cpp "$B/assets_index.cpp" "$B/assets_blob.o" "$B/app.res" $WLIBS

echo "=== $GAME: linux self-test binary (same embedded assets)"
g++ $FLAGS -I src -o "$B/kaykit-linux" src/*.cpp "$B/assets_index.cpp" \
    "$B/assets_blob.o" -lX11 -pthread

ls -la "$OUT/$NAME.exe"
echo "OK"
