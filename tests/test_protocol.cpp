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

TEST(ProtocolTest, DiscoveryRoundTrip) {
    DeviceInfo original{"TestDevice", "macOS", 9090, ""};
    std::string serialized = serialize_discovery(original);

    DeviceInfo parsed{};
    ASSERT_TRUE(parse_discovery(serialized, parsed));
    EXPECT_EQ(parsed.name, "TestDevice");
    EXPECT_EQ(parsed.os, "macOS");
    EXPECT_EQ(parsed.tcp_port, 9090);
}

TEST(ProtocolTest, DiscoveryPreservesAllFields) {
    DeviceInfo original{"John's Laptop", "Linux", 12345, ""};
    std::string serialized = serialize_discovery(original);

    DeviceInfo parsed{};
    ASSERT_TRUE(parse_discovery(serialized, parsed));
    EXPECT_EQ(parsed.name, "John's Laptop");
    EXPECT_EQ(parsed.os, "Linux");
    EXPECT_EQ(parsed.tcp_port, 12345);
}

TEST(ProtocolTest, DiscoveryRejectsInvalidMagic) {
    std::string bad = "NOTAIRDROP:1:Device:macOS:9090";
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery(bad, parsed));
}

TEST(ProtocolTest, DiscoveryRejectsWrongType) {
    // Type 2 is FILE_HEADER, not DISCOVERY
    std::string wrong = "UAIRDROP:2:Device:macOS:9090";
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery(wrong, parsed));
}

TEST(ProtocolTest, DiscoveryRejectsTruncatedMessage) {
    // Missing fields
    std::string truncated = "UAIRDROP:1:Device";
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery(truncated, parsed));
}

TEST(ProtocolTest, DiscoveryRejectsEmptyString) {
    DeviceInfo parsed{};
    EXPECT_FALSE(parse_discovery("", parsed));
}

TEST(ProtocolTest, DiscoveryFormatMatchesExpected) {
    DeviceInfo info{"MyPC", "Windows", 8080, ""};
    std::string serialized = serialize_discovery(info);
    // Format: UAIRDROP:1:MyPC:Windows:8080
    EXPECT_EQ(serialized, "UAIRDROP:1:MyPC:Windows:8080");
}

// ============================================================
// File header tests
// ============================================================

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

TEST(ProtocolTest, FileHeaderRejectsInvalidMagic) {
    std::string bad = "NOTAIRDROP:2:file.txt:100\n";
    FileHeader parsed{};
    EXPECT_FALSE(parse_file_header(bad, parsed));
}

TEST(ProtocolTest, FileHeaderRejectsWrongType) {
    // Type 1 is DISCOVERY, not a file header
    std::string wrong = "UAIRDROP:1:file.txt:100";
    FileHeader parsed{};
    EXPECT_FALSE(parse_file_header(wrong, parsed));
}

TEST(ProtocolTest, FileHeaderRejectsTruncated) {
    std::string truncated = "UAIRDROP:2:file.txt";
    FileHeader parsed{};
    EXPECT_FALSE(parse_file_header(truncated, parsed));
}

TEST(ProtocolTest, FileHeaderEncryptedUsesType6) {
    FileHeader encrypted{"file.bin", 500, true};
    std::string serialized = serialize_file_header(encrypted);
    // Should contain ":6:" (FILE_HEADER_ENCRYPTED)
    EXPECT_NE(serialized.find(":6:"), std::string::npos);
}

TEST(ProtocolTest, FileHeaderUnencryptedUsesType2) {
    FileHeader plain{"file.bin", 500, false};
    std::string serialized = serialize_file_header(plain);
    // Should contain ":2:" (FILE_HEADER)
    EXPECT_NE(serialized.find(":2:"), std::string::npos);
}

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
// Constants tests
// ============================================================

TEST(ProtocolTest, DefaultPorts) {
    EXPECT_EQ(DEFAULT_TCP_PORT, 9090);
    EXPECT_EQ(DEFAULT_UDP_PORT, 9091);
}

TEST(ProtocolTest, MagicStringStartsWithUAIRDROP) {
    EXPECT_STREQ(MAGIC, "UAIRDROP");
}

TEST(ProtocolTest, HeaderDelimiterIsNewline) {
    EXPECT_STREQ(HEADER_DELIM, "\n");
}

TEST(ProtocolTest, ChunkSizeIsReasonable) {
    EXPECT_GT(CHUNK_SIZE, 0u);
    EXPECT_LE(CHUNK_SIZE, 65536u);  // Should not exceed typical network buffer
}