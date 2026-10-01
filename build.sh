#!/bin/sh
# Vereist: clang met wasm32-doel en node. Optioneel: wasm-opt (binaryen).
set -e
clang --target=wasm32 -O3 -msimd128 -mbulk-memory -ffreestanding -nostdlib -fno-builtin \
  -Wl,--no-entry -Wl,--export=memory -Wl,--export=init_sniper -Wl,--export=scan_batch -Wl,--export=batch_keys \
  -Wl,--initial-memory=2162688 -Wl,--max-memory=2162688 -Wl,-z,stack-size=262144 -Wl,--global-base=4096 \
  -o native_jacobian.wasm sniper.c
command -v wasm-opt >/dev/null && wasm-opt -O3 --enable-simd --enable-bulk-memory native_jacobian.wasm -o native_jacobian.wasm || true
node make_single.js
