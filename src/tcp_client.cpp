#include "universal_airdrop/tcp_client.h"
#include "universal_airdrop/file_transfer.h"
#include <iostream>
#include <cstring>

namespace uair {

bool TcpClient::send_all(socket_t sock, const char* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        int sent = ::send(sock, data + total, len - total, 0);
        if (sent <= 0) return false;
        total += sent;
    }
    return true;
}

bool TcpClient::send_file(const std::string& ip, uint16_t port,
                           const std::string& filepath,
                           ProgressCallback progress) {
    FileData file;
    if (!read_file(filepath, file)) {
        std::cerr << "[TCP] Cannot read file: " << filepath << "\n";
        return false;
    }

    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) {
        std::cerr << "[TCP] Failed to create socket\n";
        return false;
    }

    sockaddr_in server_addr{};
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(port);
    if (inet_pton(AF_INET, ip.c_str(), &server_addr.sin_addr) <= 0) {
        std::cerr << "[TCP] Invalid IP: " << ip << "\n";
        SOCK_CLOSE(sock);
        return false;
    }

    std::cout << "[TCP] Connecting to " << ip << ":" << port << "...\n";
    if (connect(sock, reinterpret_cast<sockaddr*>(&server_addr),
                sizeof(server_addr)) < 0) {
        std::cerr << "[TCP] Connection failed\n";
        SOCK_CLOSE(sock);
        return false;
    }

    // Send file header
    FileHeader header{file.filename, file.bytes.size()};
    std::string header_str = serialize_file_header(header);
    if (!send_all(sock, header_str.c_str(), header_str.size())) {
        std::cerr << "[TCP] Failed to send header\n";
        SOCK_CLOSE(sock);
        return false;
    }

    // Send file data in chunks
    uint64_t total_sent = 0;
    uint64_t total_size = file.bytes.size();

    while (total_sent < total_size) {
        size_t chunk = std::min(static_cast<uint64_t>(CHUNK_SIZE),
                                total_size - total_sent);
        if (!send_all(sock, file.bytes.data() + total_sent, chunk)) {
            std::cerr << "[TCP] Send failed\n";
            SOCK_CLOSE(sock);
            return false;
        }
        total_sent += chunk;

        if (progress) {
            progress(total_sent, total_size);
        } else {
            uint64_t pct = (total_sent * 100) / total_size;
            std::cout << "\r[TCP] Sending: " << pct << "% (" << total_sent
                      << "/" << total_size << " bytes)" << std::flush;
        }
    }
    std::cout << "\n";

    // Wait for ACK
    char resp_buf[128]{};
    int resp_len = recv(sock, resp_buf, sizeof(resp_buf) - 1, 0);
    std::string resp(resp_buf, resp_len);

    bool success = resp.find(std::to_string(static_cast<int>(MsgType::FILE_ACK))) != std::string::npos;

    SOCK_CLOSE(sock);

    if (success) {
        std::cout << "[TCP] File sent successfully!\n";
    } else {
        std::cerr << "[TCP] Receiver reported error\n";
    }

    return success;
}

} // namespace uair