#include "universal_airdrop/crypto.h"
#include <openssl/evp.h>
#include <openssl/rand.h>
#include <openssl/kdf.h>
#include <openssl/err.h>
#include <cstring>
#include <stdexcept>

namespace uair {

static void throw_openssl_error(const char* msg) {
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    throw std::runtime_error(std::string(msg) + ": " + buf);
}

std::vector<uint8_t> derive_key(const std::string& passphrase,
                                const std::vector<uint8_t>& salt) {
    std::vector<uint8_t> key(AES_KEY_LEN);

    if (PKCS5_PBKDF2_HMAC(passphrase.c_str(), passphrase.size(),
                           salt.data(), salt.size(),
                           PBKDF2_ITERATIONS, EVP_sha256(),
                           key.size(), key.data()) != 1) {
        throw_openssl_error("PBKDF2 key derivation failed");
    }

    return key;
}

EncryptedBlob encrypt(const std::vector<uint8_t>& plaintext,
                      const std::string& passphrase) {
    // Generate random salt and IV
    std::vector<uint8_t> salt(SALT_LEN);
    std::vector<uint8_t> iv(AES_IV_LEN);
    if (RAND_bytes(salt.data(), salt.size()) != 1)
        throw_openssl_error("RAND_bytes for salt");
    if (RAND_bytes(iv.data(), iv.size()) != 1)
        throw_openssl_error("RAND_bytes for iv");

    auto key = derive_key(passphrase, salt);

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw_openssl_error("EVP_CIPHER_CTX_new");

    EncryptedBlob blob;
    blob.salt = salt;
    blob.iv = iv;

    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           key.data(), iv.data()) != 1)
        throw_openssl_error("EVP_EncryptInit_ex");

    // Allocate output buffer: plaintext_len + 16 (block overhead)
    blob.cipher.resize(plaintext.size() + AES_TAG_LEN);
    int out_len = 0;
    int final_len = 0;

    if (EVP_EncryptUpdate(ctx, blob.cipher.data(), &out_len,
                          plaintext.data(), plaintext.size()) != 1)
        throw_openssl_error("EVP_EncryptUpdate");

    if (EVP_EncryptFinal_ex(ctx, blob.cipher.data() + out_len, &final_len) != 1)
        throw_openssl_error("EVP_EncryptFinal_ex");

    out_len += final_len;

    // Append GCM tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_LEN,
                            blob.cipher.data() + out_len) != 1)
        throw_openssl_error("EVP_CTRL_GCM_GET_TAG");

    blob.cipher.resize(out_len + AES_TAG_LEN);

    EVP_CIPHER_CTX_free(ctx);
    return blob;
}

std::vector<uint8_t> decrypt(const EncryptedBlob& blob,
                             const std::string& passphrase) {
    auto key = derive_key(passphrase, blob.salt);

    if (blob.cipher.size() < AES_TAG_LEN) return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           key.data(), blob.iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Split ciphertext and tag
    size_t cipher_len = blob.cipher.size() - AES_TAG_LEN;
    std::vector<uint8_t> plaintext(cipher_len);
    int out_len = 0;
    int final_len = 0;

    if (EVP_DecryptUpdate(ctx, plaintext.data(), &out_len,
                          blob.cipher.data(), cipher_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Set expected tag
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_TAG_LEN,
                            const_cast<uint8_t*>(blob.cipher.data() + cipher_len)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) != 1) {
        // Authentication failed — wrong passphrase or corrupted data
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    out_len += final_len;
    plaintext.resize(out_len);

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

std::vector<uint8_t> serialize_encrypted(const EncryptedBlob& blob) {
    // Wire format: salt(16) + iv(12) + cipher(with tag appended)
    std::vector<uint8_t> out;
    out.reserve(SALT_LEN + AES_IV_LEN + blob.cipher.size());
    out.insert(out.end(), blob.salt.begin(), blob.salt.end());
    out.insert(out.end(), blob.iv.begin(), blob.iv.end());
    out.insert(out.end(), blob.cipher.begin(), blob.cipher.end());
    return out;
}

bool deserialize_encrypted(const std::vector<uint8_t>& data,
                           EncryptedBlob& out) {
    size_t header_len = SALT_LEN + AES_IV_LEN;
    if (data.size() < header_len) return false;

    out.salt.assign(data.begin(), data.begin() + SALT_LEN);
    out.iv.assign(data.begin() + SALT_LEN, data.begin() + header_len);
    out.cipher.assign(data.begin() + header_len, data.end());

    return true;
}

} // namespace uair