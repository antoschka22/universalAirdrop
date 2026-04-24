/**
 * test_protocol.cpp - Unit tests for the protocol module
 *
 * Tests serialization/parsing of discovery messages and file headers,
 * including edge cases like invalid magic strings, wrong message types,
 * encrypted vs unencrypted headers, and round-trip consistency.
 */
#include <gtest/gtest.h>
#include "universal_airdrop/protocol.h"

using namespace uair;

// ============================================================
// Discovery message tests
// ============================================================

// Verify that a discovery message survives serialize → parse unchanged.
// This is the most basic round-trip test: if this fails, all device
// discovery is broken.
TEST(ProtocolTest, DiscoveryRoundTrip) {
    DeviceInfo original{"TestDevice", "macOS", 9090, ""};
    std::string serialized = serialize_discovery(original);

    DeviceInfo parsed{};
    ASSERT_TRUE(parse_discovery(serialized, parsed));
    EXPECT_EQ(parsed.name, "TestDevice");
    EXPECT_EQ(parsed.os, "macOS");
    EXPECT_EQ(parsed.tcp_port, 9090);
}

// Like DiscoveryRoundTrip, but with non-trivial values (spaces in name,
// high port number) to ensure serialization doesn't silently drop or
// mangle any field.
TEST(ProtocolTest, DiscoveryPreservesAllFields) {
    DeviceInfo original{"John's Laptop", "Linux", 12345, ""};
    std::string serialized = serialize_discovery(original);

    DeviceInfo parsed{};
    ASSERT_TRUE(parse_discovery(serialized, parsed));
    EXPECT_EQ(parsed.name, "John's Laptop");
    EXPECT_EQ(parsed.os, "Linux");
    EXPECT_EQ(parsed.tcp_port, 12345);
}

// The parser must reject messages that don't start with the "UAIRDROP"
// magic string — otherwise any random UDP packet could be misinterpreted
// as a discovery message.
TEST(ProtocolTest, DiscoveryRejectsInvalidMagic) {
    std::string bad = "NOTAIRDROP:1:Device:macOS:9090";
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery(bad, parsed));
}

// A discovery parser must only accept type 1 (DISCOVERY) messages.
// Type 2 is a file header and should be rejected to avoid confusion
// between the two message kinds.
TEST(ProtocolTest, DiscoveryRejectsWrongType) {
    std::string wrong = "UAIRDROP:2:Device:macOS:9090";
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery(wrong, parsed));
}

// Incomplete messages (missing colon-separated fields) must be rejected
// rather than parsing garbage or crashing with out-of-bounds access.
TEST(ProtocolTest, DiscoveryRejectsTruncatedMessage) {
    std::string truncated = "UAIRDROP:1:Device";
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery(truncated, parsed));
}

// Empty input must not crash the parser — a defensive edge case.
TEST(ProtocolTest, DiscoveryRejectsEmptyString) {
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery("", parsed));
}

// Pin the exact wire format — any change to the serialization format
// will be caught here. This prevents accidental protocol breakage.
TEST(ProtocolTest, DiscoveryFormatMatchesExpected) {
    DeviceInfo info{"MyPC", "Windows", 8080, ""};
    std::string serialized = serialize_discovery(info);
    // Format: UAIRDROP:1:MyPC:Windows:8080
    EXPECT_EQ(serialized, "UAIRDROP:1:MyPC:Windows:8080");
}

// ============================================================
// File header tests
// ============================================================

// Round-trip an unencrypted file header through serialize → parse
// and verify all fields (filename, size, encrypted flag) are preserved.
// Also checks that the newline delimiter is present in the wire format.
TEST(ProtocolTest, FileHeaderUnencryptedRoundTrip) {
    FileHeader original{"photo.jpg", 1024000, false};
    std::string serialized = serialize_file_header(original);

    // Should end with newline delimiter
    EXPECT_NE(serialized.find('\n'), std::string::npos);

    // Strip the newline for parsing (simulating what the server does)
    std::string header_only = serialized.substr(0, serialized.find('\n'));

    FileHeader parsed{};
    ASSERT_TRUE(parse_file_header(header_only, parsed));
    EXPECT_EQ(parsed.filename, "photo.jpg");
    EXPECT_EQ(parsed.size, 1024000u);
    EXPECT_FALSE(parsed.encrypted);
}

// Same round-trip test but with encrypted=true. This verifies the
// parser correctly sets the encrypted flag, which determines whether
// the receiver attempts decryption or treats the data as plaintext.
TEST(ProtocolTest, FileHeaderEncryptedRoundTrip) {
    FileHeader original{"secret.doc", 2048, true};
    std::string serialized = serialize_file_header(original);

    std::string header_only = serialized.substr(0, serialized.find('\n'));

    FileHeader parsed{};
    ASSERT_TRUE(parse_file_header(header_only, parsed));
    EXPECT_EQ(parsed.filename, "secret.doc");
    EXPECT_EQ(parsed.size, 2048u);
    EXPECT_TRUE(parsed.encrypted);
}

// File headers must require the "UAIRDROP" magic prefix — same
// defensive check as discovery, but for the file-header parser.
TEST(ProtocolTest, FileHeaderRejectsInvalidMagic) {
    std::string bad = "NOTAIRDROP:2:file.txt:100\n";
    FileHeader parsed{};
    EXPECT_FALSE(parse_file_header(bad, parsed));
}

// File-header parser must only accept type 2 or 6 — a type-1
// (discovery) message should never be interpreted as a file header.
TEST(ProtocolTest, FileHeaderRejectsWrongType) {
    std::string wrong = "UAIRDROP:1:file.txt:100";
    FileHeader parsed{};
    EXPECT_FALSE(parse_file_header(wrong, parsed));
}

// Truncated file header (missing the size field) must be rejected
// to prevent undefined behavior when the receiver tries to read
// a number of bytes that was never specified.
TEST(ProtocolTest, FileHeaderRejectsTruncated) {
    std::string truncated = "UAIRDROP:2:file.txt";
    FileHeader parsed{};
    EXPECT_FALSE(parse_file_header(truncated, parsed));
}

// Encrypted file headers must serialize with type 6
// (FILE_HEADER_ENCRYPTED) so the receiver knows to decrypt the payload.
TEST(ProtocolTest, FileHeaderEncryptedUsesType6) {
    FileHeader encrypted{"file.bin", 500, true};
    std::string serialized = serialize_file_header(encrypted);
    // Should contain ":6:" (FILE_HEADER_ENCRYPTED)
    EXPECT_NE(serialized.find(":6:"), std::string::npos);
}

// Unencrypted file headers must serialize with type 2 (FILE_HEADER)
// so the receiver treats the payload as raw bytes.
TEST(ProtocolTest, FileHeaderUnencryptedUsesType2) {
    FileHeader plain{"file.bin", 500, false};
    std::string serialized = serialize_file_header(plain);
    // Should contain ":2:" (FILE_HEADER)
    EXPECT_NE(serialized.find(":2:"), std::string::npos);
}

// Ensure 64-bit file sizes (4 GB+) survive the string conversion
// round-trip. A 32-bit truncation here would silently corrupt large
// file transfers.
TEST(ProtocolTest, FileHeaderLargeFileSize) {
    uint64_t large_size = 4ULL * 1024 * 1024 * 1024; // 4 GB
    FileHeader original{"bigfile.iso", large_size, false};
    std::string serialized = serialize_file_header(original);

    std::string header_only = serialized.substr(0, serialized.find('\n'));
    FileHeader parsed{};
    ASSERT_TRUE(parse_file_header(header_only, parsed));
    EXPECT_EQ(parsed.size, large_size);
}

// ============================================================
// Constants tests — these guard against accidental changes to
// protocol constants that would break wire compatibility.
// ============================================================

// The default TCP and UDP ports must stay at 9090/9091 so that
// different versions of the application can discover and talk to
// each other.
TEST(ProtocolTest, DefaultPorts) {
    EXPECT_EQ(DEFAULT_TCP_PORT, 9090);
    EXPECT_EQ(DEFAULT_UDP_PORT, 9091);
}

// The MAGIC string is the protocol identifier on the wire — if it
// changes, old clients can't talk to new ones.
TEST(ProtocolTest, MagicStringStartsWithUAIRDROP) {
    EXPECT_STREQ(MAGIC, "UAIRDROP");
}

// The TCP stream uses a newline to separate the header from the
// payload. If this constant changes, stream framing breaks.
TEST(ProtocolTest, HeaderDelimiterIsNewline) {
    EXPECT_STREQ(HEADER_DELIM, "\n");
}

// CHUNK_SIZE must be positive and not exceed a reasonable network
// buffer size. A value of 0 would cause infinite loops; a value
// over 65 KB would fragment on most networks.
TEST(ProtocolTest, ChunkSizeIsReasonable) {
    EXPECT_GT(CHUNK_SIZE, 0u);
    EXPECT_LE(CHUNK_SIZE, 65536u);  // Should not exceed typical network buffer
}