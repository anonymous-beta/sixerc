// ============================================================================
// SIXERC — Obfuscation & Anti-Analysis
// String encryption, sleep jitter, control flow
// ============================================================================

#pragma once

#include <string>
#include <array>
#include <cstdint>

namespace sixerc::obf {

// Compile-time string encryption using XOR
// Usage: OBF("secret_string") — decrypts at runtime
template<size_t N>
struct encrypted_string {
    std::array<char, N> data;
    char key;
    
    constexpr encrypted_string(const char (&str)[N], char k) : key(k) {
        for (size_t i = 0; i < N; ++i) {
            data[i] = str[i] ^ key;
        }
    }
    
    std::string decrypt() const {
        std::string result;
        result.reserve(N);
        for (size_t i = 0; i < N - 1; ++i) {
            result.push_back(data[i] ^ key);
        }
        return result;
    }
};

// Runtime string decryption
std::string xor_decrypt(const std::string& data, uint8_t key);
std::string xor_encrypt(const std::string& data, uint8_t key);

// Jittered sleep with anti-emulation
void stealth_sleep(uint32_t base_ms);

// Junk code insertion marker (no-op that confuses static analysis)
void junk_marker();

} // namespace sixerc::obf

// Macro for easy compile-time encryption
#define OBF(str) ([]() -> std::string { \
    constexpr sixerc::obf::encrypted_string<sizeof(str)> es(str, 0x42); \
    return es.decrypt(); \
}())
