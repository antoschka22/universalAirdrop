/**
 * test_crypto.cpp - Unit tests for the encryption module
 *
 * Tests AES-256-GCM encryption/decryption, PBKDF2 key derivation,
 * serialization/deserialization, wrong passphrase rejection,
 * corrupted data handling, and edge cases.
 */
#include <gtest/gtest.h>
#include "universal_airdrop/crypto.h"

using namespace uair;

// ============================================================
// Key derivation tests
// ============================================================

// PBKDF2 is deterministic — same passphrase + same salt must always
// yield the same 32-byte AES key. If this fails, encryption and
// decryption on separate machines would produce different keys.
TEST(CryptoTest, DeriveKeyProducesConsistentResults) {
    // Same passphrase + same salt should produce the same key
    std::vector<uint8_t> salt(16, 0xAB);
    auto key1 = derive_key("mypassword", salt);
    auto key2 = derive_key("mypassword", salt);
    EXPECT_EQ(key1, key2);
    EXPECT_EQ(key1.size(), AES_KEY_LEN);
}

// Different passphrases must produce different keys — otherwise two
// users with different passwords could decrypt each other's files.
TEST(CryptoTest, DeriveKeyDiffersWithDifferentPassphrases) {
    std::vector<uint8_t> salt(16, 0xAB);
    auto key1 = derive_key("password1", salt);
    auto key2 = derive_key("password2", salt);
    EXPECT_NE(key1, key2);
}

// Different salts must also produce different keys. The salt is
// randomly generated per transfer, so even the same passphrase
// yields a different key each time — this is what makes the
// ciphertext differ between transfers of the same file.
TEST(CryptoTest, DeriveKeyDiffersWithDifferentSalts) {
    std::vector<uint8_t> salt1(16, 0xAA);
    std::vector<uint8_t> salt2(16, 0xBB);
    auto key1 = derive_key("samepassword", salt1);
    auto key2 = derive_key("samepassword", salt2);
    EXPECT_NE(key1, key2);
}

// ============================================================
// Encrypt/Decrypt round-trip tests
// ============================================================

// The fundamental contract: encrypt then decrypt must return the
// original plaintext. If this fails, the entire encrypted transfer
// pipeline is broken.
TEST(CryptoTest, EncryptDecryptRoundTrip) {
    std::string plaintext = "Hello, Universal Airdrop!";
    std::vector<uint8_t> data(plaintext.begin(), plaintext.end());

    auto blob = encrypt(data, "testpass");
    auto decrypted = decrypt(blob, "testpass");

    ASSERT_EQ(decrypted.size(), data.size());
    EXPECT_EQ(std::vector<uint8_t>(decrypted.begin(), decrypted.end()), data);
}

// Edge case: encrypting empty data should not crash or fail.
// The result should decrypt back to an empty vector.
TEST(CryptoTest, EncryptDecryptEmptyData) {
    std::vector<uint8_t> data;
    auto blob = encrypt(data, "pass");
    auto decrypted = decrypt(blob, "pass");
    EXPECT_TRUE(decrypted.empty());
}

// Stress-test with 100 KB of data. Ensures the encryption/decryption
// pipeline handles multi-chunk data without truncation or overflow.
TEST(CryptoTest, EncryptDecryptLargeData) {
    std::vector<uint8_t> data(100000, 0x42);  // 100KB of 'B'
    auto blob = encrypt(data, "largepass");
    auto decrypted = decrypt(blob, "largepass");

    ASSERT_EQ(decrypted.size(), data.size());
    EXPECT_EQ(decrypted, data);
}

// All 256 byte values (0x00–0xFF) must round-trip correctly.
// This catches issues like null-byte truncation or sign-extension
// bugs that only appear with certain byte values.
TEST(CryptoTest, EncryptDecryptBinaryData) {
    // All byte values 0-255
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; i++) data[i] = static_cast<uint8_t>(i);

    auto blob = encrypt(data, "binpass");
    auto decrypted = decrypt(blob, "binpass");

    EXPECT_EQ(decrypted, data);
}

// AES-GCM uses a random IV each time, so encrypting the same data
// twice with the same passphrase must produce different ciphertext.
// If the ciphertext were identical, it would leak information about
// repeated transfers of the same file.
TEST(CryptoTest, EncryptionProducesDifferentCiphertextEachTime) {
    // Even with the same plaintext and passphrase, different random
    // salts and IVs mean different ciphertext each time
    std::vector<uint8_t> data(100, 0x55);

    auto blob1 = encrypt(data, "samepass");
    auto blob2 = encrypt(data, "samepass");

    // The ciphertext should differ (different salt/IV)
    EXPECT_NE(blob1.cipher, blob2.cipher);
    // But decryption with the same passphrase should both work
    EXPECT_EQ(decrypt(blob1, "samepass"), data);
    EXPECT_EQ(decrypt(blob2, "samepass"), data);
}

// ============================================================
// Wrong passphrase tests
// ============================================================

// Decryption with the wrong passphrase must fail and return an empty
// vector (our error signal). GCM authentication detects this before
// any plaintext is revealed.
TEST(CryptoTest, WrongPassphraseFailsDecryption) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto blob = encrypt(data, "correctpass");

    auto result = decrypt(blob, "wrongpass");
    EXPECT_TRUE(result.empty());
}

// Edge case: an empty passphrase should still produce a valid key
// (PBKDF2 accepts zero-length input). This ensures the code path
// doesn't crash when no passphrase is provided.
TEST(CryptoTest, EmptyPassphraseEncryptDecrypt) {
    // Empty passphrase should still work (just a zero-length key derivation input)
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
    auto blob = encrypt(data, "");
    auto decrypted = decrypt(blob, "");
    EXPECT_EQ(decrypted, data);
}

// Two different wrong passphrases should both fail — verifies that
// the decrypt function doesn't accidentally accept any non-matching
// key as valid (which would indicate a GCM tag-check bypass).
TEST(CryptoTest, DifferentPassphrasesBothFail) {
    std::vector<uint8_t> data = {1, 2, 3};
    auto blob = encrypt(data, "pass1");

    EXPECT_TRUE(decrypt(blob, "pass2").empty());
    EXPECT_TRUE(decrypt(blob, "pass3").empty());
}

// ============================================================
// Serialization tests — the wire format for encrypted blobs must
// survive serialize → deserialize without losing any bytes.
// ============================================================

// Full round-trip: encrypt, serialize to flat bytes, deserialize back
// into an EncryptedBlob, and verify all fields match the original.
TEST(CryptoTest, SerializeDeserializeRoundTrip) {
    std::vector<uint8_t> data = {0x01, 0x02, 0x03, 0x04};
    auto blob = encrypt(data, "test");

    auto serialized = serialize_encrypted(blob);
    EncryptedBlob deserialized;
    ASSERT_TRUE(deserialize_encrypted(serialized, deserialized));

    EXPECT_EQ(deserialized.salt, blob.salt);
    EXPECT_EQ(deserialized.iv, blob.iv);
    EXPECT_EQ(deserialized.cipher, blob.cipher);
}

// Deserialization must reject data shorter than the minimum header
// size (salt 16 + IV 12 = 28 bytes). Without this check, reading
// past the buffer would cause undefined behavior.
TEST(CryptoTest, DeserializeRejectsTooShortData) {
    // Data shorter than salt(16) + iv(12) = 28 bytes should fail
    std::vector<uint8_t> short_data(10, 0xFF);
    EncryptedBlob out;
    EXPECT_FALSE(deserialize_encrypted(short_data, out));
}

// The minimum valid serialized blob is exactly 28 bytes (salt + IV,
// no ciphertext). This test verifies the deserializer handles the
// boundary case correctly.
TEST(CryptoTest, DeserializeAcceptsMinimalValidData) {
    // salt(16) + iv(12) = 28 bytes minimum (with empty ciphertext)
    std::vector<uint8_t> minimal_data(28, 0x00);
    EncryptedBlob out;
    EXPECT_TRUE(deserialize_encrypted(minimal_data, out));
    EXPECT_EQ(out.salt.size(), SALT_LEN);
    EXPECT_EQ(out.iv.size(), AES_IV_LEN);
    EXPECT_TRUE(out.cipher.empty());
}

// Verify the EncryptedBlob internal sizes match the protocol
// constants. If these drift (e.g., salt length changes), the wire
// format breaks silently.
TEST(CryptoTest, EncryptedBlobStructureSizes) {
    std::vector<uint8_t> data = {1, 2, 3};
    auto blob = encrypt(data, "test");

    EXPECT_EQ(blob.salt.size(), SALT_LEN);     // 16 bytes
    EXPECT_EQ(blob.iv.size(), AES_IV_LEN);       // 12 bytes
    // ciphertext = 3 bytes data + 16 bytes auth tag
    EXPECT_EQ(blob.cipher.size(), data.size() + AES_TAG_LEN);
}

// ============================================================
// Tampering detection tests — GCM's authentication tag must
// detect any modification to the ciphertext or IV.
// ============================================================

// Flipping a single bit in the ciphertext must cause decryption to
// fail. This is GCM's core security guarantee: any tampering is
// detected before plaintext is released.
TEST(CryptoTest, TamperedCiphertextFailsDecryption) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto blob = encrypt(data, "pass");

    // Flip a bit in the ciphertext
    blob.cipher[0] ^= 0xFF;

    auto result = decrypt(blob, "pass");
    EXPECT_TRUE(result.empty());
}

// Flipping a bit in the IV (nonce) must also cause decryption to
// fail. An IV tampering attack could otherwise produce a predictable
// keystream shift.
TEST(CryptoTest, TamperedIVFailsDecryption) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto blob = encrypt(data, "pass");

    // Flip a bit in the IV
    blob.iv[0] ^= 0xFF;

    auto result = decrypt(blob, "pass");
    EXPECT_TRUE(result.empty());
}