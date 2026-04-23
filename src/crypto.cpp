/**
 * crypto.cpp - AES-256-GCM encryption/decryption implementation
 *
 * This file implements the encryption layer using OpenSSL's EVP API.
 * The EVP (Envelope) API is OpenSSL's high-level interface for cryptographic
 * operations. It abstracts away the details of individual cipher algorithms
 * so the same code works with any cipher (AES, ChaCha20, etc.).
 *
 * Encryption steps:
 *   1. Generate random salt (16 bytes) and IV (12 bytes)
 *   2. Derive a 256-bit key from passphrase + salt using PBKDF2-SHA256
 *   3. Initialize AES-256-GCM cipher context
 *   4. Encrypt the plaintext (EVP_EncryptUpdate + EVP_EncryptFinal_ex)
 *   5. Extract the 16-byte GCM authentication tag (EVP_CTRL_GCM_GET_TAG)
 *   6. Return salt + IV + ciphertext + tag as EncryptedBlob
 *
 * Decryption steps:
 *   1. Re-derive the key from passphrase + salt
 *   2. Split ciphertext and auth tag (last 16 bytes)
 *   3. Set the expected auth tag (EVP_CTRL_GCM_SET_TAG)
 *   4. Decrypt (EVP_DecryptUpdate + EVP_DecryptFinal_ex)
 *   5. If the passphrase is wrong, DecryptFinal_ex fails → return empty vector
 */
#include "universal_airdrop/crypto.h"
#include <openssl/evp.h>    /* EVP cipher API (EncryptInit, EncryptUpdate, etc.) */
#include <openssl/rand.h>    /* RAND_bytes for generating random salt/IV */
#include <openssl/kdf.h>     /* Key derivation functions (PBKDF2) */
#include <openssl/err.h>    /* Error reporting */
#include <cstring>
#include <stdexcept>

namespace uair {

/**
 * throw_openssl_error() - Convert the most recent OpenSSL error to a C++ exception
 *
 * OpenSSL doesn't throw exceptions — it returns error codes. This helper
 * fetches the human-readable error string and throws a runtime_error,
 * making it easy to integrate OpenSSL errors into the C++ exception system.
 */
static void throw_openssl_error(const char* msg) {
    char buf[256];
    ERR_error_string_n(ERR_get_error(), buf, sizeof(buf));
    throw std::runtime_error(std::string(msg) + ": " + buf);
}

/**
 * derive_key() - Derive a 256-bit encryption key from a passphrase using PBKDF2
 *
 * PBKDF2 (Password-Based Key Derivation Function 2) converts a human-readable
 * passphrase into a cryptographic key by:
 *   1. Mixing the passphrase with a random salt
 *   2. Running HMAC-SHA256 100,000 times on the mixture
 *
 * The high iteration count makes brute-force attacks impractical: each guess
 * requires 100,000 SHA-256 operations. The salt prevents pre-computed
 * dictionary attacks (rainbow tables) because the same passphrase produces
 * a completely different key with each unique salt.
 *
 * @param passphrase  The user-provided password string
 * @param salt        A random 16-byte value unique to this transfer
 * @return            A 32-byte (256-bit) key suitable for AES-256
 */
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

/**
 * encrypt() - Encrypt plaintext bytes with AES-256-GCM using a passphrase
 *
 * This function:
 *   1. Generates a random 16-byte salt and 12-byte IV
 *   2. Derives the encryption key from passphrase + salt
 *   3. Encrypts the data with AES-256-GCM
 *   4. Appends the 16-byte GCM authentication tag to the ciphertext
 *   5. Returns an EncryptedBlob with salt, iv, and cipher (which
 *      contains the ciphertext followed by the auth tag)
 *
 * The auth tag is critical: it allows the receiver to verify that
 * the data hasn't been tampered with and that the passphrase is correct.
 * Without it, an attacker could modify the ciphertext undetected.
 */
EncryptedBlob encrypt(const std::vector<uint8_t>& plaintext,
                      const std::string& passphrase) {
    // Generate random salt and IV — these are different every time
    // to ensure the same file + same passphrase produces different output
    std::vector<uint8_t> salt(SALT_LEN);
    std::vector<uint8_t> iv(AES_IV_LEN);
    if (RAND_bytes(salt.data(), salt.size()) != 1)
        throw_openssl_error("RAND_bytes for salt");
    if (RAND_bytes(iv.data(), iv.size()) != 1)
        throw_openssl_error("RAND_bytes for iv");

    auto key = derive_key(passphrase, salt);

    // Create and initialize the cipher context
    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) throw_openssl_error("EVP_CIPHER_CTX_new");

    EncryptedBlob blob;
    blob.salt = salt;
    blob.iv = iv;

    // Initialize AES-256-GCM with the derived key and IV
    if (EVP_EncryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           key.data(), iv.data()) != 1)
        throw_openssl_error("EVP_EncryptInit_ex");

    // Allocate output buffer: plaintext_len + 16 (for the auth tag)
    // GCM doesn't expand the ciphertext beyond the tag, so this is sufficient
    blob.cipher.resize(plaintext.size() + AES_TAG_LEN);
    int out_len = 0;
    int final_len = 0;

    // Encrypt the plaintext — EVP_EncryptUpdate processes the data
    // and writes ciphertext to blob.cipher.data()
    if (EVP_EncryptUpdate(ctx, blob.cipher.data(), &out_len,
                          plaintext.data(), plaintext.size()) != 1)
        throw_openssl_error("EVP_EncryptUpdate");

    // Finalize encryption — for GCM this doesn't produce additional ciphertext
    // but is required by the API
    if (EVP_EncryptFinal_ex(ctx, blob.cipher.data() + out_len, &final_len) != 1)
        throw_openssl_error("EVP_EncryptFinal_ex");

    out_len += final_len;

    // Extract the 16-byte GCM authentication tag and append it to the ciphertext.
    // This tag will be verified during decryption to ensure data integrity
    // and passphrase correctness.
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_GET_TAG, AES_TAG_LEN,
                            blob.cipher.data() + out_len) != 1)
        throw_openssl_error("EVP_CTRL_GCM_GET_TAG");

    blob.cipher.resize(out_len + AES_TAG_LEN);

    EVP_CIPHER_CTX_free(ctx);
    return blob;
}

/**
 * decrypt() - Decrypt an EncryptedBlob with the given passphrase
 *
 * This function:
 *   1. Re-derives the key from passphrase + salt
 *   2. Splits the ciphertext and auth tag (last 16 bytes)
 *   3. Sets the expected auth tag on the cipher context
 *   4. Attempts decryption with EVP_DecryptFinal_ex
 *   5. If the passphrase is wrong or data was tampered with,
 *      DecryptFinal_ex FAILS and we return an empty vector
 *
 * The key insight: GCM's DecryptFinal_ex performs authentication verification.
 * If the computed tag doesn't match the provided tag, it returns failure.
 * This is how we detect wrong passphrases without writing corrupted data.
 *
 * @return  Decrypted bytes on success, empty vector on failure
 */
std::vector<uint8_t> decrypt(const EncryptedBlob& blob,
                             const std::string& passphrase) {
    auto key = derive_key(passphrase, blob.salt);

    // Need at least the auth tag's worth of data
    if (blob.cipher.size() < AES_TAG_LEN) return {};

    EVP_CIPHER_CTX* ctx = EVP_CIPHER_CTX_new();
    if (!ctx) return {};

    // Initialize AES-256-GCM decryption with the derived key and IV
    if (EVP_DecryptInit_ex(ctx, EVP_aes_256_gcm(), nullptr,
                           key.data(), blob.iv.data()) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // The auth tag is the last 16 bytes of the cipher field.
    // Everything before it is the actual ciphertext.
    size_t cipher_len = blob.cipher.size() - AES_TAG_LEN;
    std::vector<uint8_t> plaintext(cipher_len);
    int out_len = 0;
    int final_len = 0;

    // Decrypt the ciphertext portion
    if (EVP_DecryptUpdate(ctx, plaintext.data(), &out_len,
                          blob.cipher.data(), cipher_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // Tell OpenSSL what auth tag to expect. This MUST be done before
    // DecryptFinal_ex — that's where the actual verification happens.
    if (EVP_CIPHER_CTX_ctrl(ctx, EVP_CTRL_GCM_SET_TAG, AES_TAG_LEN,
                            const_cast<uint8_t*>(blob.cipher.data() + cipher_len)) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    // DecryptFinal_ex verifies the auth tag. If it fails, the passphrase
    // is wrong or the data was tampered with. We return an empty vector
    // so the caller knows decryption failed without getting garbage data.
    if (EVP_DecryptFinal_ex(ctx, plaintext.data() + out_len, &final_len) != 1) {
        EVP_CIPHER_CTX_free(ctx);
        return {};
    }

    out_len += final_len;
    plaintext.resize(out_len);

    EVP_CIPHER_CTX_free(ctx);
    return plaintext;
}

/**
 * serialize_encrypted() - Convert EncryptedBlob to a flat byte array for network transmission
 *
 * Wire format: [salt(16)][iv(12)][cipher+tag]
 * The receiver knows exactly where each component starts because
 * salt and IV have fixed, known lengths.
 */
std::vector<uint8_t> serialize_encrypted(const EncryptedBlob& blob) {
    std::vector<uint8_t> out;
    out.reserve(SALT_LEN + AES_IV_LEN + blob.cipher.size());
    out.insert(out.end(), blob.salt.begin(), blob.salt.end());
    out.insert(out.end(), blob.iv.begin(), blob.iv.end());
    out.insert(out.end(), blob.cipher.begin(), blob.cipher.end());
    return out;
}

/**
 * deserialize_encrypted() - Parse a received byte array back into EncryptedBlob
 *
 * Splits at fixed offsets: bytes 0-15 are salt, bytes 16-27 are IV,
 * bytes 28+ are ciphertext with auth tag appended.
 *
 * @return  true if the data is long enough to contain at least salt + IV
 */
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