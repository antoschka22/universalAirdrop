#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace uair {

// AES-256-GCM encryption for file transfers.
// Key derivation: PBKDF2 with a random salt from the passphrase.
// Wire format: [16-byte salt][12-byte IV][ciphertext][16-byte tag]

constexpr size_t AES_KEY_LEN = 32;   // AES-256
constexpr size_t AES_IV_LEN = 12;    // GCM standard IV
constexpr size_t AES_TAG_LEN = 16;   // GCM auth tag
constexpr size_t PBKDF2_ITERATIONS = 100000;
constexpr size_t SALT_LEN = 16;

struct EncryptedBlob {
    std::vector<uint8_t> salt;    // 16 bytes
    std::vector<uint8_t> iv;      // 12 bytes
    std::vector<uint8_t> cipher;  // ciphertext + tag
};

// Derive a 256-bit key from passphrase + salt using PBKDF2-SHA256
std::vector<uint8_t> derive_key(const std::string& passphrase,
                                const std::vector<uint8_t>& salt);

// Encrypt plaintext bytes with AES-256-GCM using the given passphrase.
// Returns salt + iv + ciphertext + tag.
EncryptedBlob encrypt(const std::vector<uint8_t>& plaintext,
                      const std::string& passphrase);

// Decrypt an EncryptedBlob with the given passphrase.
// Returns empty vector on failure (wrong passphrase or corrupted data).
std::vector<uint8_t> decrypt(const EncryptedBlob& blob,
                             const std::string& passphrase);

// Serialize EncryptedBlob to wire format: salt(16) + iv(12) + cipher(tag appended)
std::vector<uint8_t> serialize_encrypted(const EncryptedBlob& blob);

// Deserialize wire format back into EncryptedBlob
bool deserialize_encrypted(const std::vector<uint8_t>& data,
                           EncryptedBlob& out);

} // namespace uair