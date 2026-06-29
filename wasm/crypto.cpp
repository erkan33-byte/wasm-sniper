#include <emscripten.h>
#include <string.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    const char* hash160(const char* pubHex) {
        // SHA256 + RIPEMD160 placeholder
        return "0123456789abcdef0123456789abcdef01234567";
    }

    EMSCRIPTEN_KEEPALIVE
    const char* doubleSha256(const char* data) {
        return "0123456789abcdef0123456789abcdef01234567";
    }
}
