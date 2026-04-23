/**
 * test_file_transfer.cpp - Unit tests for the file I/O module
 *
 * Tests reading/writing files, filename extraction, directory creation,
 * and edge cases like non-existent files and binary data.
 */
#include <gtest/gtest.h>
#include "universal_airdrop/file_transfer.h"
#include <filesystem>
#include <fstream>

using namespace uair;

class FileTransferTest : public ::testing::Test {
protected:
    std::string test_dir;

    void SetUp() override {
        test_dir = std::filesystem::temp_directory_path() / "uair_test";
        std::filesystem::create_directories(test_dir);
    }

    void TearDown() override {
        std::filesystem::remove_all(test_dir);
    }

    // Helper: create a file with specific content
    void create_file(const std::string& path, const std::string& content) {
        std::ofstream ofs(path, std::ios::binary);
        ofs.write(content.data(), content.size());
    }
};

// ============================================================
// read_file tests
// ============================================================

TEST_F(FileTransferTest, ReadExistingFile) {
    std::string path = test_dir + "/testfile.txt";
    create_file(path, "Hello, World!");

    FileData data;
    ASSERT_TRUE(read_file(path, data));
    EXPECT_EQ(data.filename, "testfile.txt");
    EXPECT_EQ(data.bytes.size(), 13u);
    EXPECT_EQ(std::string(data.bytes.begin(), data.bytes.end()), "Hello, World!");
}

TEST_F(FileTransferTest, ReadNonExistentFile) {
    FileData data;
    EXPECT_FALSE(read_file("/nonexistent/path/file.txt", data));
}

TEST_F(FileTransferTest, ReadBinaryFile) {
    std::string path = test_dir + "/binary.bin";
    std::vector<char> binary_data(256);
    for (int i = 0; i < 256; i++) binary_data[i] = static_cast<char>(i);

    std::ofstream ofs(path, std::ios::binary);
    ofs.write(binary_data.data(), binary_data.size());
    ofs.close();

    FileData data;
    ASSERT_TRUE(read_file(path, data));
    EXPECT_EQ(data.bytes.size(), 256u);
    for (int i = 0; i < 256; i++) {
        EXPECT_EQ(static_cast<unsigned char>(data.bytes[i]), i);
    }
}

TEST_F(FileTransferTest, ReadEmptyFile) {
    std::string path = test_dir + "/empty.txt";
    create_file(path, "");

    FileData data;
    ASSERT_TRUE(read_file(path, data));
    EXPECT_EQ(data.filename, "empty.txt");
    EXPECT_EQ(data.bytes.size(), 0u);
}

// ============================================================
// write_file tests
// ============================================================

TEST_F(FileTransferTest, WriteAndReadRoundTrip) {
    std::string content = "Test content for round trip!";
    std::string out_dir = test_dir + "/output";

    ASSERT_TRUE(write_file(out_dir, "roundtrip.txt", content.data(), content.size()));

    FileData data;
    ASSERT_TRUE(read_file(out_dir + "/roundtrip.txt", data));
    EXPECT_EQ(std::string(data.bytes.begin(), data.bytes.end()), content);
}

TEST_F(FileTransferTest, WriteCreatesDirectory) {
    std::string nested_dir = test_dir + "/nested/deep/dir";
    std::string content = "nested file";

    // The directory shouldn't exist yet, write_file should create it
    EXPECT_FALSE(std::filesystem::exists(nested_dir));
    ASSERT_TRUE(write_file(nested_dir, "file.txt", content.data(), content.size()));
    EXPECT_TRUE(std::filesystem::exists(nested_dir + "/file.txt"));
}

TEST_F(FileTransferTest, WriteBinaryData) {
    std::vector<char> binary_data(1024);
    for (int i = 0; i < 1024; i++) binary_data[i] = static_cast<char>(i % 256);

    std::string out_dir = test_dir + "/bin_output";
    ASSERT_TRUE(write_file(out_dir, "binary.bin", binary_data.data(), binary_data.size()));

    FileData data;
    ASSERT_TRUE(read_file(out_dir + "/binary.bin", data));
    EXPECT_EQ(data.bytes.size(), 1024u);
    for (int i = 0; i < 1024; i++) {
        EXPECT_EQ(data.bytes[i], static_cast<char>(i % 256));
    }
}

TEST_F(FileTransferTest, WriteToExistingDirectory) {
    std::string out_dir = test_dir;  // Already exists from SetUp
    std::string content = "existing dir";

    ASSERT_TRUE(write_file(out_dir, "file.txt", content.data(), content.size()));
    EXPECT_TRUE(std::filesystem::exists(out_dir + "/file.txt"));
}

// ============================================================
// get_filename tests
// ============================================================

TEST_F(FileTransferTest, GetFilenameFromFullPath) {
    EXPECT_EQ(get_filename("/home/user/docs/photo.jpg"), "photo.jpg");
}

TEST_F(FileTransferTest, GetFilenameFromRelativePath) {
    EXPECT_EQ(get_filename("./photos/vacation.png"), "vacation.png");
}

TEST_F(FileTransferTest, GetFilenameFromJustFilename) {
    EXPECT_EQ(get_filename("file.txt"), "file.txt");
}

TEST_F(FileTransferTest, GetFilenameFromDeepPath) {
    EXPECT_EQ(get_filename("/a/b/c/d/e/file.dat"), "file.dat");
}

TEST_F(FileTransferTest, GetFilenameFromWindowsPath) {
    // std::filesystem should handle backslashes on Windows,
    // but on Unix they're just part of the filename
    EXPECT_EQ(get_filename("C:\\Users\\file.txt"), "C:\\Users\\file.txt");
}