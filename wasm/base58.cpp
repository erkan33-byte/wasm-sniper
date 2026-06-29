#include <emscripten.h>
#include <string>
#include <vector>
#include <openssl/sha.h>

const char* BASE58_ALPHABET = "123456789ABCDEFGHJKLMNPQRSTUVWXYZabcdefghijkmnopqrstuvwxyz";

extern "C" {
    // Base58 Encode
    EMSCRIPTEN_KEEPALIVE
    const char* encode(const char* hex) {
        // Converteer hex naar bytes
        std::string hexStr(hex);
        std::vector<unsigned char> bytes;
        for (size_t i = 0; i < hexStr.length(); i += 2) {
            std::string byteStr = hexStr.substr(i, 2);
            unsigned char byte = (unsigned char) strtol(byteStr.c_str(), NULL, 16);
            bytes.push_back(byte);
        }
        
        // Base58 conversie
        std::vector<unsigned char> digits(1, 0);
        for (unsigned char b : bytes) {
            int carry = b;
            for (size_t j = 0; j < digits.size(); j++) {
                carry += digits[j] << 8;
                digits[j] = carry % 58;
                carry = carry / 58;
            }
            while (carry > 0) {
                digits.push_back(carry % 58);
                carry = carry / 58;
            }
        }
        
        // Leading zeros
        size_t zeros = 0;
        while (zeros < bytes.size() && bytes[zeros] == 0) zeros++;
        
        static char result[256];
        int pos = 0;
        for (size_t i = 0; i < zeros; i++) {
            result[pos++] = BASE58_ALPHABET[0];
        }
        for (int i = digits.size() - 1; i >= 0; i--) {
            result[pos++] = BASE58_ALPHABET[digits[i]];
        }
        result[pos] = '\0';
        return result;
    }
    
    // WIF Encode (private key + checksum)
    EMSCRIPTEN_KEEPALIVE
    const char* encodeWIF(const char* privHex) {
        // WIF = 0x80 + private key + 0x01 + checksum
        std::string payload = "80" + std::string(privHex) + "01";
        
        // Double SHA256 voor checksum
        std::vector<unsigned char> payloadBytes;
        for (size_t i = 0; i < payload.length(); i += 2) {
            std::string byteStr = payload.substr(i, 2);
            unsigned char byte = (unsigned char) strtol(byteStr.c_str(), NULL, 16);
            payloadBytes.push_back(byte);
        }
        
        unsigned char hash1[SHA256_DIGEST_LENGTH];
        SHA256(payloadBytes.data(), payloadBytes.size(), hash1);
        unsigned char hash2[SHA256_DIGEST_LENGTH];
        SHA256(hash1, SHA256_DIGEST_LENGTH, hash2);
        
        // Voeg checksum toe (eerste 4 bytes)
        std::string fullWIF = payload;
        for (int i = 0; i < 4; i++) {
            char buf[3];
            sprintf(buf, "%02x", hash2[i]);
            fullWIF += buf;
        }
        
        // Encode naar Base58
        return encode(fullWIF.c_str());
    }
}
