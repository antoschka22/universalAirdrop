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

TEST(CryptoTest, DeriveKeyProducesConsistentResults) {
    // Same passphrase + same salt should produce the same key
    std::vector<uint8_t> salt(16, 0xAB);
    auto key1 = derive_key("mypassword", salt);
    auto key2 = derive_key("mypassword", salt);
    EXPECT_EQ(key1, key2);
    EXPECT_EQ(key1.size(), AES_KEY_LEN);
}

TEST(CryptoTest, DeriveKeyDiffersWithDifferentPassphrases) {
    std::vector<uint8_t> salt(16, 0xAB);
    auto key1 = derive_key("password1", salt);
    auto key2 = derive_key("password2", salt);
    EXPECT_NE(key1, key2);
}

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

TEST(CryptoTest, EncryptDecryptRoundTrip) {
    std::string plaintext = "Hello, Universal Airdrop!";
    std::vector<uint8_t> data(plaintext.begin(), plaintext.end());

    auto blob = encrypt(data, "testpass");
    auto decrypted = decrypt(blob, "testpass");

    ASSERT_EQ(decrypted.size(), data.size());
    EXPECT_EQ(std::vector<uint8_t>(decrypted.begin(), decrypted.end()), data);
}

TEST(CryptoTest, EncryptDecryptEmptyData) {
    std::vector<uint8_t> data;
    auto blob = encrypt(data, "pass");
    auto decrypted = decrypt(blob, "pass");
    EXPECT_TRUE(decrypted.empty());
}

TEST(CryptoTest, EncryptDecryptLargeData) {
    std::vector<uint8_t> data(100000, 0x42);  // 100KB of 'B'
    auto blob = encrypt(data, "largepass");
    auto decrypted = decrypt(blob, "largepass");

    ASSERT_EQ(decrypted.size(), data.size());
    EXPECT_EQ(decrypted, data);
}

TEST(CryptoTest, EncryptDecryptBinaryData) {
    // All byte values 0-255
    std::vector<uint8_t> data(256);
    for (int i = 0; i < 256; i++) data[i] = static_cast<uint8_t>(i);

    auto blob = encrypt(data, "binpass");
    auto decrypted = decrypt(blob, "binpass");

    EXPECT_EQ(decrypted, data);
}

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

TEST(CryptoTest, WrongPassphraseFailsDecryption) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto blob = encrypt(data, "correctpass");

    auto result = decrypt(blob, "wrongpass");
    EXPECT_TRUE(result.empty());
}

TEST(CryptoTest, EmptyPassphraseEncryptDecrypt) {
    // Empty passphrase should still work (just a zero-length key derivation input)
    std::vector<uint8_t> data = {0xAA, 0xBB, 0xCC};
    auto blob = encrypt(data, "");
    auto decrypted = decrypt(blob, "");
    EXPECT_EQ(decrypted, data);
}

TEST(CryptoTest, DifferentPassphrasesBothFail) {
    std::vector<uint8_t> data = {1, 2, 3};
    auto blob = encrypt(data, "pass1");

    EXPECT_TRUE(decrypt(blob, "pass2").empty());
    EXPECT_TRUE(decrypt(blob, "pass3").empty());
}

// ============================================================
// Serialization tests
// ============================================================

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

TEST(CryptoTest, DeserializeRejectsTooShortData) {
    // Data shorter than salt(16) + iv(12) = 28 bytes should fail
    std::vector<uint8_t> short_data(10, 0xFF);
    EncryptedBlob out;
    EXPECT_FALSE(deserialize_encrypted(short_data, out));
}

TEST(CryptoTest, DeserializeAcceptsMinimalValidData) {
    // salt(16) + iv(12) = 28 bytes minimum (with empty ciphertext)
    std::vector<uint8_t> minimal_data(28, 0x00);
    EncryptedBlob out;
    EXPECT_TRUE(deserialize_encrypted(minimal_data, out));
    EXPECT_EQ(out.salt.size(), SALT_LEN);
    EXPECT_EQ(out.iv.size(), AES_IV_LEN);
    EXPECT_TRUE(out.cipher.empty());
}

TEST(CryptoTest, EncryptedBlobStructureSizes) {
    std::vector<uint8_t> data = {1, 2, 3};
    auto blob = encrypt(data, "test");

    EXPECT_EQ(blob.salt.size(), SALT_LEN);     // 16 bytes
    EXPECT_EQ(blob.iv.size(), AES_IV_LEN);       // 12 bytes
    // ciphertext = 3 bytes data + 16 bytes auth tag
    EXPECT_EQ(blob.cipher.size(), data.size() + AES_TAG_LEN);
}

// ============================================================
// Tampering detection tests
// ============================================================

TEST(CryptoTest, TamperedCiphertextFailsDecryption) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto blob = encrypt(data, "pass");

    // Flip a bit in the ciphertext
    blob.cipher[0] ^= 0xFF;

    auto result = decrypt(blob, "pass");
    EXPECT_TRUE(result.empty());
}

TEST(CryptoTest, TamperedIVFailsDecryption) {
    std::vector<uint8_t> data = {1, 2, 3, 4, 5};
    auto blob = encrypt(data, "pass");

    // Flip a bit in the IV
    blob.iv[0] ^= 0xFF;

    auto result = decrypt(blob, "pass");
    EXPECT_TRUE(result.empty());
}