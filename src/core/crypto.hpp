// ============================================================================
// SIXERC — Cryptographic Operations
// DPAPI, AES-256-GCM, RSA-2048, Base64
// ============================================================================

#pragma once

#include "types.hpp"
#include <windows.h>
#include <wincrypt.h>
#include <openssl/evp.h>
#include <openssl/rsa.h>
#include <openssl/pem.h>
#include <string>

namespace sixerc::crypto {

// DPAPI wrapper
bytes dpapi_decrypt(const bytes& encrypted);
bytes dpapi_encrypt(const bytes& plaintext);

// AES-256-GCM
bytes aes_gcm_encrypt(const bytes& plaintext, const bytes& key, const bytes& iv, bytes& tag);
bytes aes_gcm_decrypt(const bytes& ciphertext, const bytes& key, const bytes& iv, const bytes& tag);
bytes generate_random_bytes(size_t len);

// Chrome cookie decryption (v10/v11 format)
bytes decrypt_chrome_cookie(const bytes& encrypted_value, const bytes& master_key);

// RSA-2048
bytes rsa_encrypt(const bytes& plaintext, const std::string& public_key_pem);
std::string rsa_generate_keypair();

// Base64
std::string base64_encode(const bytes& data);
bytes base64_decode(const std::string& encoded);

// Secure memory wipe
void secure_zero(void* ptr, size_t len);

} // namespace sixerc::crypto
