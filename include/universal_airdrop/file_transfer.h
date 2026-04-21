#pragma once

#include <string>
#include <vector>
#include <cstdint>

namespace uair {

struct FileData {
    std::string filename;
    std::vector<char> bytes;
};

bool read_file(const std::string& path, FileData& out);
bool write_file(const std::string& dir, const std::string& filename,
                const char* data, size_t len);
std::string get_filename(const std::string& path);

} // namespace uair