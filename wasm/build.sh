#!/bin/bash
set -e

# 1. Installeer Emscripten (de compiler)
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# 2. Compileer secp256k1 (met echte elliptic curve code)
emcc secp256k1.cpp -O3 \
    -s WASM=1 \
    -s EXPORTED_FUNCTIONS='["_mul_G","_add_G","_randomInRange","_getPublicKey"]' \
    -s EXPORTED_RUNTIME_METHODS='["cwrap"]' \
    -o secp256k1.wasm

# 3. Compileer crypto (SHA256 + RIPEMD160)
emcc crypto.cpp -O3 \
    -s WASM=1 \
    -s EXPORTED_FUNCTIONS='["_hash160","_doubleSha256"]' \
    -s EXPORTED_RUNTIME_METHODS='["cwrap"]' \
    -o crypto.wasm

# 4. Compileer base58
emcc base58.cpp -O3 \
    -s WASM=1 \
    -s EXPORTED_FUNCTIONS='["_encode","_encodeWIF"]' \
    -s EXPORTED_RUNTIME_METHODS='["cwrap"]' \
    -o base58.wasm

# 5. Verplaats de wasm bestanden naar de docs map (voor GitHub Pages)
mkdir -p ../docs/wasm
mv *.wasm ../docs/wasm/
