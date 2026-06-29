#include <emscripten.h>
#include <openssl/sha.h>
#include <openssl/ripemd.h>
#include <string>
#include <vector>

// Helper: hex string naar bytes
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
    // Hash160: SHA256 + RIPEMD160
    EMSCRIPTEN_KEEPALIVE
    const char* hash160(const char* pubHex) {
        std::vector<unsigned char> pubBytes = hexToBytes(std::string(pubHex));
        
        unsigned char sha[SHA256_DIGEST_LENGTH];
        SHA256(pubBytes.data(), pubBytes.size(), sha);
        
        unsigned char ripe[RIPEMD160_DIGEST_LENGTH];
        RIPEMD160(sha, SHA256_DIGEST_LENGTH, ripe);
        
        static char result[41];
        for (int i = 0; i < 20; i++) {
            sprintf(result + i*2, "%02x", ripe[i]);
        }
        return result;
    }
    
    // Double SHA256 (voor checksum)
    EMSCRIPTEN_KEEPALIVE
    const char* doubleSha256(const char* data) {
        std::vector<unsigned char> dataBytes = hexToBytes(std::string(data));
        
        unsigned char hash1[SHA256_DIGEST_LENGTH];
        SHA256(dataBytes.data(), dataBytes.size(), hash1);
        
        unsigned char hash2[SHA256_DIGEST_LENGTH];
        SHA256(hash1, SHA256_DIGEST_LENGTH, hash2);
        
        static char result[65];
        for (int i = 0; i < 32; i++) {
            sprintf(result + i*2, "%02x", hash2[i]);
        }
        return result;
    }
}
