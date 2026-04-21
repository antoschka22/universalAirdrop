#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <sstream>

namespace uair {

constexpr uint16_t DEFAULT_TCP_PORT = 9090;
constexpr uint16_t DEFAULT_UDP_PORT = 9091;
constexpr const char* BROADCAST_ADDR = "255.255.255.255";
constexpr const char* MAGIC = "UAIRDROP";
constexpr const char* HEADER_DELIM = "\n";
constexpr size_t CHUNK_SIZE = 4096;
constexpr int DISCOVERY_INTERVAL_SEC = 2;

enum class MsgType : uint8_t {
    DISCOVERY = 1,
    FILE_HEADER = 2,
    FILE_DATA = 3,
    FILE_ACK = 4,
    FILE_ERR = 5,
};

struct DeviceInfo {
    std::string name;
    std::string os;
    uint16_t tcp_port;
    std::string ip;
};

inline std::string serialize_discovery(const DeviceInfo& info) {
    std::ostringstream oss;
    oss << MAGIC << ":" << static_cast<int>(MsgType::DISCOVERY) << ":"
        << info.name << ":" << info.os << ":" << info.tcp_port;
    return oss.str();
}

inline bool parse_discovery(const std::string& raw, DeviceInfo& out) {
    // format: UAIRDROP:1:name:os:port
    if (raw.rfind(MAGIC, 0) != 0) return false;

    std::istringstream iss(raw);
    std::string magic, type_str, name, os, port_str;
    if (!std::getline(iss, magic, ':')) return false;
    if (!std::getline(iss, type_str, ':')) return false;
    if (!std::getline(iss, name, ':')) return false;
    if (!std::getline(iss, os, ':')) return false;
    if (!std::getline(iss, port_str, ':')) return false;

    if (std::stoi(type_str) != static_cast<int>(MsgType::DISCOVERY)) return false;

    out.name = name;
    out.os = os;
    out.tcp_port = static_cast<uint16_t>(std::stoi(port_str));
    return true;
}

struct FileHeader {
    std::string filename;
    uint64_t size;
};

inline std::string serialize_file_header(const FileHeader& hdr) {
    std::ostringstream oss;
    oss << MAGIC << ":" << static_cast<int>(MsgType::FILE_HEADER) << ":"
        << hdr.filename << ":" << hdr.size << HEADER_DELIM;
    return oss.str();
}

inline bool parse_file_header(const std::string& raw, FileHeader& out) {
    if (raw.rfind(MAGIC, 0) != 0) return false;

    std::istringstream iss(raw);
    std::string magic, type_str, filename, size_str;
    if (!std::getline(iss, magic, ':')) return false;
    if (!std::getline(iss, type_str, ':')) return false;
    if (!std::getline(iss, filename, ':')) return false;
    if (!std::getline(iss, size_str, ':')) return false;

    if (std::stoi(type_str) != static_cast<int>(MsgType::FILE_HEADER)) return false;

    out.filename = filename;
    out.size = std::stoull(size_str);
    return true;
}

} // namespace uair