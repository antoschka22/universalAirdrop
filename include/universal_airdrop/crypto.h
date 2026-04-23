/**
 * crypto.h - AES-256-GCM encryption for file transfers
 *
 * This module provides end-to-end encryption so that files sent over
 * the local network cannot be intercepted by other devices on the same
 * Wi-Fi. It uses AES-256-GCM, the same encryption algorithm used by
 * HTTPS, SSH, and most modern secure systems.
 *
 * Encryption flow (sender side):
 *   1. Generate a random 16-byte salt and 12-byte IV
 *   2. Derive a 256-bit key from the passphrase + salt using PBKDF2-SHA256
 *      (100,000 iterations — deliberately slow to resist brute-force)
 *   3. Encrypt the file data with AES-256-GCM using the derived key and IV
 *   4. The output is: salt(16) + iv(12) + ciphertext + auth_tag(16)
 *
 * Decryption flow (receiver side):
 *   1. Split the received data into salt, IV, and ciphertext+tag
 *   2. Re-derive the key from passphrase + salt
 *   3. Attempt AES-256-GCM decryption — the auth tag verification will
 *      FAIL if the passphrase is wrong, returning an empty vector
 *   4. On success, the original plaintext bytes are returned
 *
 * Why AES-256-GCM instead of other modes?
 *   - GCM provides both encryption AND authentication in one step
 *   - Tampering with the ciphertext is detected (auth tag verification fails)
 *   - Wrong passphrase is detected immediately (no corrupted file written)
 *   - It's fast on modern hardware (AES-NI instructions)
 *
 * Why PBKDF2 with 100,000 iterations?
 *   - Makes brute-force passphrase guessing extremely expensive
 *   - Each guess requires 100,000 SHA-256 operations
 *   - Salt prevents pre-computed dictionary attacks
 */
#pragma once

#include <vector>
#include <string>
#include <cstdint>

namespace uair {

constexpr size_t AES_KEY_LEN = 32;      /**< AES-256 key length (256 bits = 32 bytes) */
constexpr size_t AES_IV_LEN = 12;       /**< GCM standard IV length (12 bytes/96 bits) */
constexpr size_t AES_TAG_LEN = 16;      /**< GCM authentication tag length (16 bytes/128 bits) */
constexpr size_t PBKDF2_ITERATIONS = 100000; /**< PBKDF2 iterations (deliberately slow) */
constexpr size_t SALT_LEN = 16;          /**< Random salt length (16 bytes/128 bits) */

/**
 * EncryptedBlob - The result of encryption, holding all components
 *
 * After encryption, these three pieces are needed for decryption:
 *   - salt:  random bytes mixed with the passphrase during key derivation
 *   - iv:    random initialization vector for the AES cipher
 *   - cipher: the encrypted data with the 16-byte GCM auth tag appended
 *
 * The salt and IV are different for every transfer, so even if you
 * send the same file with the same passphrase twice, the encrypted
 * output will be completely different.
 */
struct EncryptedBlob {
    std::vector<uint8_t> salt;    /**< 16-byte random salt for key derivation */
    std::vector<uint8_t> iv;      /**< 12-byte random IV for AES-GCM */
    std::vector<uint8_t> cipher;  /**< Ciphertext with 16-byte GCM auth tag appended */
};

/**
 * derive_key() - Derive a 256-bit encryption key from a passphrase
 *
 * Uses PBKDF2 (Password-Based Key Derivation Function 2) with SHA-256.
 * The salt ensures the same passphrase produces different keys each time,
 * preventing pre-computed dictionary (rainbow table) attacks.
 *
 * @param passphrase  The user-provided password string
 * @param salt        A random 16-byte value unique to this transfer
 * @return            A 32-byte (256-bit) key suitable for AES-256
 */
std::vector<uint8_t> derive_key(const std::string& passphrase,
                                const std::vector<uint8_t>& salt);

/**
 * encrypt() - Encrypt plaintext bytes with AES-256-GCM
 *
 * Generates a random salt and IV, derives the key, and encrypts.
 * The GCM auth tag is appended to the ciphertext in the returned blob.
 *
 * @param plaintext   The raw file bytes to encrypt
 * @param passphrase  The user-provided password
 * @return            An EncryptedBlob containing salt, iv, and cipher+tag
 */
EncryptedBlob encrypt(const std::vector<uint8_t>& plaintext,
                      const std::string& passphrase);

/**
 * decrypt() - Decrypt an EncryptedBlob with the given passphrase
 *
 * If the passphrase is wrong, GCM authentication will fail and an
 * empty vector is returned. No corrupted data is written to disk.
 *
 * @param blob        The encrypted data (salt + iv + cipher+tag)
 * @param passphrase  The user-provided password (must match sender's)
 * @return            Decrypted bytes on success, empty vector on failure
 */
std::vector<uint8_t> decrypt(const EncryptedBlob& blob,
                             const std::string& passphrase);

/**
 * serialize_encrypted() - Convert EncryptedBlob to a flat byte array
 *
 * Wire format: [salt(16)][iv(12)][cipher+tag]
 * This flat format can be sent directly over the network socket.
 * The receiver knows exactly where each component starts because
 * salt and IV have fixed, known lengths.
 */
std::vector<uint8_t> serialize_encrypted(const EncryptedBlob& blob);

/**
 * deserialize_encrypted() - Parse a flat byte array back into EncryptedBlob
 *
 * Splits the wire format at fixed offsets: bytes 0-15 are salt,
 * bytes 16-27 are IV, bytes 28+ are ciphertext with tag.
 *
 * @param data  The raw bytes received from the network
 * @param out   [out] The parsed EncryptedBlob
 * @return      true if data is long enough to contain at least salt+iv
 */
bool deserialize_encrypted(const std::vector<uint8_t>& data,
                           EncryptedBlob& out);

} // namespace uair