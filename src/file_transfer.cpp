/**
 * file_transfer.cpp - File reading and writing implementation
 *
 * Provides simple file I/O utilities for the file transfer system.
 * Uses C++17's <filesystem> for portable path operations and
 * directory creation. Files are read entirely into memory, which
 * is fine for typical AirDrop-sized files (photos, documents).
 */
#include "universal_airdrop/file_transfer.h"
#include <fstream>
#include <filesystem>

namespace uair {

/**
 * read_file() - Read an entire file from disk into memory
 *
 * Opens the file in binary mode at the end (std::ios::ate) so tellg()
 * immediately returns the file size. Then seeks back to the beginning
 * and reads the entire file into the output vector.
 *
 * The filename field is set to just the filename component (without
 * the directory path) using get_filename().
 *
 * @param path  Full filesystem path to the file
 * @param out   [out] FileData struct with filename and bytes populated
 * @return      true if the file was read successfully, false on error
 */
bool read_file(const std::string& path, FileData& out) {
    std::ifstream ifs(path, std::ios::binary | std::ios::ate);
    if (!ifs) return false;

    auto size = ifs.tellg();
    ifs.seekg(0, std::ios::beg);

    out.filename = get_filename(path);
    out.bytes.resize(size);
    if (!ifs.read(out.bytes.data(), size)) return false;

    return true;
}

/**
 * write_file() - Write data to a file on disk
 *
 * Creates the output directory (and any parent directories) if it
 * doesn't exist, then writes the data as a binary file.
 * std::filesystem::create_directories handles nested paths like
 * "./received/subfolder" and does nothing if the directory exists.
 *
 * @param dir       Directory path (will be created if needed)
 * @param filename  Name for the file (appended to dir with /)
 * @param data      Pointer to the raw bytes to write
 * @param len       Number of bytes to write
 * @return          true if the file was written successfully
 */
bool write_file(const std::string& dir, const std::string& filename,
                const char* data, size_t len) {
    std::filesystem::create_directories(dir);
    std::string full_path = dir + "/" + filename;

    std::ofstream ofs(full_path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(data, len);
    return ofs.good();
}

/**
 * get_filename() - Extract just the filename from a full path
 *
 * Uses std::filesystem::path to handle path separators correctly
 * across platforms (/ on Unix, \ on Windows).
 * Example: "/home/user/photos/vacation.jpg" -> "vacation.jpg"
 */
std::string get_filename(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

} // namespace uair