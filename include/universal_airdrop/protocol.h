/**
 * protocol.h - The communication language between devices
 *
 * This file defines the message format that both the sender and receiver
 * agree on. For two devices to communicate, they need a shared "language"
 * — that's what this file provides.
 *
 * All messages start with the MAGIC string "UAIRDROP" so we can ignore
 * random network traffic that isn't from our application. The message
 * type (a number) follows, then colon-separated fields.
 *
 * Message types:
 *   1 (DISCOVERY)          - UDP: "I'm here!" broadcast
 *   2 (FILE_HEADER)         - TCP: file metadata (unencrypted)
 *   6 (FILE_HEADER_ENCRYPTED) - TCP: file metadata (encrypted)
 *   4 (FILE_ACK)            - TCP: "File received OK"
 *   5 (FILE_ERR)            - TCP: "Something went wrong"
 *
 * Wire format examples:
 *   Discovery:   UAIRDROP:1:John's Laptop:macOS:9090
 *   File header: UAIRDROP:2:photo.jpg:512000\n
 *   Encrypted:   UAIRDROP:6:photo.jpg:512044\n
 *
 * The \n (newline) at the end of file headers is critical. TCP is a stream
 * protocol — the header and file data might arrive in the same recv() call.
 * The newline gives us a clear boundary: everything before \n is the header,
 * everything after is file data.
 */
#pragma once

#include <string>
#include <cstdint>
#include <vector>
#include <sstream>

namespace uair {

/* Default network ports */
constexpr uint16_t DEFAULT_TCP_PORT = 9090;   /**< Default port for TCP file transfers */
constexpr uint16_t DEFAULT_UDP_PORT = 9091;   /**< Default port for UDP discovery broadcasts */
constexpr const char* BROADCAST_ADDR = "255.255.255.255"; /**< Broadcasts to all devices on the LAN */
constexpr const char* MAGIC = "UAIRDROP";      /**< Every message starts with this identifier */
constexpr const char* HEADER_DELIM = "\n";     /**< Newline separates the header from file data */
constexpr size_t CHUNK_SIZE = 4096;            /**< Data is sent/received in 4KB chunks */
constexpr int DISCOVERY_INTERVAL_SEC = 2;      /**< Broadcast discovery every 2 seconds */

/**
 * MsgType - Numeric identifiers for each message type
 *
 * Using an enum class (instead of a plain enum or #define) provides
 * type safety: you can't accidentally pass a MsgType where an int
 * is expected. The : uint8_t means each value is stored as a single byte.
 *
 * FILE_HEADER (2) is for unencrypted transfers.
 * FILE_HEADER_ENCRYPTED (6) is for encrypted transfers — the receiver
 * needs to know whether to decrypt before it can process the data.
 */
enum class MsgType : uint8_t {
    DISCOVERY = 1,             /**< UDP broadcast: "I'm here, I'm <name> on <os>" */
    FILE_HEADER = 2,          /**< TCP: unencrypted file metadata */
    FILE_DATA = 3,             /**< Reserved for future chunked streaming */
    FILE_ACK = 4,             /**< TCP: receiver confirms successful file save */
    FILE_ERR = 5,             /**< TCP: receiver reports an error */
    FILE_HEADER_ENCRYPTED = 6,/**< TCP: encrypted file metadata */
};

/**
 * DeviceInfo - Information about a discovered device
 *
 * This struct holds everything we know about a peer device found via
 * UDP discovery. Note that `ip` is NOT sent in the broadcast — it's
 * filled in by the receiver from the source address of the UDP packet.
 * This is a design choice: the sender doesn't need to know its own IP
 * (which is tricky to determine on multi-interface machines), and
 * the receiver already has it from the packet metadata.
 */
struct DeviceInfo {
    std::string name;      /**< Human-readable device name (e.g., "John's Laptop") */
    std::string os;        /**< Operating system (e.g., "macOS", "Windows") */
    uint16_t tcp_port;     /**< Port where this device's TCP server is listening */
    std::string ip;        /**< IP address (filled in by receiver, not from broadcast) */
};

/**
 * serialize_discovery() - Convert DeviceInfo to a network string
 *
 * Format: UAIRDROP:1:name:os:port
 * Example: UAIRDROP:1:John's Laptop:macOS:9090
 *
 * Uses ostringstream with colon delimiters for simple,
 * human-readable serialization. This is sent as the payload
 * of each UDP broadcast packet.
 */
inline std::string serialize_discovery(const DeviceInfo& info) {
    std::ostringstream oss;
    oss << MAGIC << ":" << static_cast<int>(MsgType::DISCOVERY) << ":"
        << info.name << ":" << info.os << ":" << info.tcp_port;
    return oss.str();
}

/**
 * parse_discovery() - Parse a discovery message string back into DeviceInfo
 *
 * @param raw  The raw string received from a UDP broadcast
 * @param out  [out] The parsed DeviceInfo struct
 * @return     true if parsing succeeded, false if the message is malformed
 *
 * First checks that the message starts with "UAIRDROP" (the MAGIC string)
 * to filter out unrelated network traffic. Then extracts colon-separated
 * fields using std::getline with ':' as delimiter.
 */
inline bool parse_discovery(const std::string& raw, DeviceInfo& out) {
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

/**
 * FileHeader - Metadata sent before the file data
 *
 * The header tells the receiver what file is coming and how many bytes
 * to expect. The `encrypted` flag indicates whether the following data
 * is encrypted with AES-256-GCM (type 6) or raw (type 2).
 *
 * Wire format: UAIRDROP:<type>:<filename>:<size>\n
 * The \n delimiter is essential for TCP stream framing — without it,
 * the receiver wouldn't know where the header ends and data begins.
 */
struct FileHeader {
    std::string filename;    /**< Name of the file (without path) */
    uint64_t size;           /**< Size of the payload in bytes (encrypted or raw) */
    bool encrypted = false;  /**< True if payload is AES-256-GCM encrypted */
};

/**
 * serialize_file_header() - Convert FileHeader to a network string
 *
 * Appends HEADER_DELIM ("\n") at the end. This newline is critical:
 * TCP is a stream protocol, so the receiver reads until it finds this
 * newline to separate the header from the file data that follows.
 *
 * For encrypted files (encrypted=true), the type is 6 and the size
 * is the size of the encrypted payload (which is larger than the
 * original file due to salt, IV, and auth tag overhead).
 */
inline std::string serialize_file_header(const FileHeader& hdr) {
    std::ostringstream oss;
    int type = hdr.encrypted ? static_cast<int>(MsgType::FILE_HEADER_ENCRYPTED)
                             : static_cast<int>(MsgType::FILE_HEADER);
    oss << MAGIC << ":" << type << ":"
        << hdr.filename << ":" << hdr.size << HEADER_DELIM;
    return oss.str();
}

/**
 * parse_file_header() - Parse a file header string back into FileHeader
 *
 * @param raw  The header string (everything before the \n delimiter)
 * @param out  [out] The parsed FileHeader struct
 * @return     true if parsing succeeded, false if the header is malformed
 *
 * Determines whether the file is encrypted by checking if the type
 * field is FILE_HEADER_ENCRYPTED (6) or FILE_HEADER (2).
 * The `size` field contains the payload size — for encrypted files,
 * this is the encrypted payload size (larger than original file).
 */
inline bool parse_file_header(const std::string& raw, FileHeader& out) {
    if (raw.rfind(MAGIC, 0) != 0) return false;

    std::istringstream iss(raw);
    std::string magic, type_str, filename, size_str;
    if (!std::getline(iss, magic, ':')) return false;
    if (!std::getline(iss, type_str, ':')) return false;
    if (!std::getline(iss, filename, ':')) return false;
    if (!std::getline(iss, size_str, ':')) return false;

    int type = std::stoi(type_str);
    if (type != static_cast<int>(MsgType::FILE_HEADER) &&
        type != static_cast<int>(MsgType::FILE_HEADER_ENCRYPTED))
        return false;

    out.filename = filename;
    out.size = std::stoull(size_str);
    out.encrypted = (type == static_cast<int>(MsgType::FILE_HEADER_ENCRYPTED));
    return true;
}

} // namespace uair