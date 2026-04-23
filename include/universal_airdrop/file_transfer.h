/**
 * file_transfer.h - File reading and writing utilities
 *
 * This module provides simple file I/O functions used by both the TCP
 * client (to read files before sending) and the TCP server (to write
 * received files to disk). It also includes a helper to extract just
 * the filename from a full path.
 *
 * Uses C++17's <filesystem> library for portable path operations
 * (creating directories, extracting filenames). This works across
 * Windows, macOS, and Linux without platform-specific code.
 */
#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace uair {

/**
 * FileData - Holds a file's name and contents in memory
 *
 * This struct is used by the TCP client to pass the file's name
 * (without the directory path) and raw bytes to the sending function.
 * The entire file is read into memory at once — this is fine for
 * typical AirDrop-sized files (photos, documents), but could be
 * problematic for very large files (multiple GB).
 */
struct FileData {
    std::string filename;      /**< Just the filename, e.g. "photo.jpg" */
    std::vector<char> bytes;   /**< The complete file contents in memory */
};

/**
 * read_file() - Read an entire file from disk into memory
 *
 * Opens the file in binary mode at the end (std::ios::ate) to get
 * the file size via tellg(), then seeks back to the beginning and
 * reads the entire file into the output vector.
 *
 * @param path  The full filesystem path to the file
 * @param out   [out] FileData struct with filename and bytes populated
 * @return      true if the file was read successfully, false on error
 */
bool read_file(const std::string& path, FileData& out);

/**
 * write_file() - Write data to a file on disk
 *
 * Creates the output directory (and any parent directories) if it
 * doesn't exist, then writes the data as a binary file. Uses C++17's
 * std::filesystem::create_directories for portable directory creation.
 *
 * @param dir       The directory to save the file in (e.g., "./received")
 * @param filename  The filename to save as (e.g., "photo.jpg")
 * @param data      Pointer to the raw bytes to write
 * @param len       Number of bytes to write
 * @return          true if the file was written successfully, false on error
 */
bool write_file(const std::string& dir, const std::string& filename,
                const char* data, size_t len);

/**
 * get_filename() - Extract just the filename from a full path
 *
 * Uses std::filesystem::path to handle path separators correctly
 * across platforms (/ on Unix, \ on Windows).
 * Example: "/home/user/photos/vacation.jpg" -> "vacation.jpg"
 *
 * @param path  The full filesystem path
 * @return      Just the filename component
 */
std::string get_filename(const std::string& path);

} // namespace uair