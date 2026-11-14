#!/bin/sh
# Golf Dynamics — build the WebAssembly module with wasi-sdk.
# SPDX-License-Identifier: MIT
# Usage: WASI_SDK=/path/to/wasi-sdk web/build_wasi.sh [output-dir]   (default: web/dist)
# Tested with wasi-sdk 25. No exceptions or RTTI; -ffp-contract=off as the
# native builds, so a shot computes the same doubles natively and in the
# browser (multiplayer replays depend on it).
set -eu
HERE=$(cd "$(dirname "$0")" && pwd)
REPO=$(cd "$HERE/.." && pwd)
OUT=${1:-"$HERE/dist"}
: "${WASI_SDK:?set WASI_SDK to the wasi-sdk directory}"
mkdir -p "$OUT"
GEN="$OUT/golf_embedded.cpp"
node "$HERE/embed.mjs" "$REPO/data" "$GEN"
EXPORTS=""
for f in gd_abi_version gd_alloc gd_free gd_result gd_result_length gd_course_load gd_height_ptr gd_surface_ptr \
         gd_trees_ptr gd_trees_count gd_height gd_surface_at gd_in_bounds gd_set_stimp gd_match_new gd_match_load \
         gd_match_state gd_match_save gd_match_play gd_match_plan gd_match_preview gd_match_pickup gd_path_ptr \
         gd_path_count gd_clubs gd_launch_preview; do
  EXPORTS="$EXPORTS -Wl,--export=$f"
done
"$WASI_SDK/bin/clang++" --target=wasm32-wasip1 --sysroot="$WASI_SDK/share/wasi-sysroot" \
  -std=c++20 -O2 -fno-exceptions -fno-rtti -ffp-contract=off -Wall -Wextra -mexec-model=reactor \
  -I"$REPO/include" "$HERE/golf_web.cpp" "$REPO"/src/*.cpp "$GEN" \
  $EXPORTS -Wl,--initial-memory=67108864 -Wl,--max-memory=1073741824 -Wl,--strip-all \
  -o "$OUT/golf.wasm"
rm -f "$GEN"
cp "$HERE/golf_loader.mjs" "$OUT/golf.mjs"
cp "$REPO/data/courses/varzea.json" "$OUT/varzea.json"
ls -l "$OUT"
