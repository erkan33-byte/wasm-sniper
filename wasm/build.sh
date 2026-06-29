#!/bin/bash
set -e

# Installeer Emscripten
git clone https://github.com/emscripten-core/emsdk.git
cd emsdk
./emsdk install latest
./emsdk activate latest
source ./emsdk_env.sh

# Compileer WASM bestanden
emcc secp256k1.cpp -O3 -s WASM=1 -s EXPORTED_FUNCTIONS='["_mul_G","_add_G","_randomInRange","_getPublicKey"]' -o secp256k1.wasm
emcc crypto.cpp -O3 -s WASM=1 -s EXPORTED_FUNCTIONS='["_hash160","_doubleSha256"]' -o crypto.wasm
emcc base58.cpp -O3 -s WASM=1 -s EXPORTED_FUNCTIONS='["_encode","_encodeWIF"]' -o base58.wasm

# Maak output map
mkdir -p ../docs/wasm
cp *.wasm ../docs/wasm/
