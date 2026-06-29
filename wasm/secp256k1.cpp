#include <emscripten.h>
#include <secp256k1.h>
#include <string>
#include <vector>

// Helper om hex string naar bytes om te zetten
std::vector<unsigned char> hexToBytes(const std::string& hex) {
    std::vector<unsigned char> bytes;
    for (unsigned int i = 0; i < hex.length(); i += 2) {
        std::string byteString = hex.substr(i, 2);
        unsigned char byte = (unsigned char) strtol(byteString.c_str(), NULL, 16);
        bytes.push_back(byte);
    }
    return bytes;
}

extern "C" {
    // Genereer public key van private key (hex string)
    EMSCRIPTEN_KEEPALIVE
    const char* getPublicKey(const char* privHex) {
        secp256k1_context* ctx = secp256k1_context_create(SECP256K1_CONTEXT_SIGN);
        unsigned char privKey[32];
        std::vector<unsigned char> privBytes = hexToBytes(std::string(privHex));
        std::copy(privBytes.begin(), privBytes.end(), privKey);
        
        secp256k1_pubkey pubKey;
        if (!secp256k1_ec_pubkey_create(ctx, &pubKey, privKey)) {
            secp256k1_context_destroy(ctx);
            return "error";
        }
        
        // Serialize compressed public key
        unsigned char pubKeyBytes[33];
        size_t len = 33;
        secp256k1_ec_pubkey_serialize(ctx, pubKeyBytes, &len, &pubKey, SECP256K1_EC_COMPRESSED);
        secp256k1_context_destroy(ctx);
        
        // Return as hex string (static buffer voor JS)
        static char result[67];
        for (int i = 0; i < 33; i++) {
            sprintf(result + i*2, "%02x", pubKeyBytes[i]);
        }
        return result;
    }
}
