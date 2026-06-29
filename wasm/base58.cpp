#include <emscripten.h>
#include <string.h>

extern "C" {
    EMSCRIPTEN_KEEPALIVE
    const char* encode(const char* hex) {
        return "1A1zP1eP5QGefi2DMPTfTL5SLmv7DivfNa"; // placeholder
    }

    EMSCRIPTEN_KEEPALIVE
    const char* encodeWIF(const char* privHex) {
        return "5HueCGU8rMjxEXivPuVo6pU6aB5HjvJZ7bK8p8p8p8p8"; // placeholder
    }
}
