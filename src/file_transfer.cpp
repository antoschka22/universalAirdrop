#include "universal_airdrop/file_transfer.h"
#include <fstream>
#include <filesystem>

namespace uair {

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

bool write_file(const std::string& dir, const std::string& filename,
                const char* data, size_t len) {
    std::filesystem::create_directories(dir);
    std::string full_path = dir + "/" + filename;

    std::ofstream ofs(full_path, std::ios::binary);
    if (!ofs) return false;
    ofs.write(data, len);
    return ofs.good();
}

std::string get_filename(const std::string& path) {
    return std::filesystem::path(path).filename().string();
}

} // namespace uair