#!/bin/zsh
# Compiles the device UI + synth to sim/remi.wasm with wasi-sdk (https://github.com/WebAssembly/wasi-sdk).
#   WASI_SDK=/path/to/wasi-sdk sim/build_wasm.sh
set -euo pipefail
cd "${0:A:h}/.."
: "${WASI_SDK:?set WASI_SDK to an extracted wasi-sdk release}"
FW=firmware/Remi_Device
TMP=$(mktemp -d)
"$WASI_SDK/bin/clang" --target=wasm32-wasip1 --sysroot="$WASI_SDK/share/wasi-sysroot" -O2 -flto -c "$FW/qrcode.c" -o "$TMP/qrcode.o"
"$WASI_SDK/bin/clang++" --target=wasm32-wasip1 --sysroot="$WASI_SDK/share/wasi-sysroot" \
  -std=c++17 -O2 -flto -fno-exceptions -fno-rtti -mexec-model=reactor -I"$FW" \
  sim/wasm_main.cpp "$FW/gfx.cpp" "$FW/ui.cpp" "$FW/synth.cpp" "$TMP/qrcode.o" \
  -Wl,--strip-all -Wl,--initial-memory=8388608 -o sim/remi.wasm
rm -rf "$TMP"
ls -l sim/remi.wasm
