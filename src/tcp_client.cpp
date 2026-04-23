/**
 * tcp_client.cpp - TCP client implementation for sending files
 *
 * This file implements the TcpClient class declared in tcp_client.h.
 * The client connects to a remote TcpServer, optionally encrypts the
 * file data, sends the header and payload in chunks, and waits for
 * an acknowledgment response.
 */
#include "universal_airdrop/tcp_client.h"
#include "universal_airdrop/file_transfer.h"
#include "universal_airdrop/crypto.h"
#include <iostream>
#include <cstring>

namespace uair {

/**
 * send_all() - Send all bytes through a socket, retrying as needed
 *
 * The send() system call doesn't guarantee it will send all the bytes
 * you request — on a slow network, it might only send a portion.
 * This function loops until every byte has been sent, tracking
 * progress with the 'total' counter.
 *
 * @param sock  The connected socket to send on
 * @param data  Pointer to the data buffer
 * @param len   Number of bytes to send
 * @return     true if all bytes were sent, false on error/disconnection
 */
bool TcpClient::send_all(socket_t sock, const char* data, size_t len) {
    size_t total = 0;
    while (total < len) {
        int sent = ::send(sock, data + total, len - total, 0);
        if (sent <= 0) return false;
        total += sent;
    }
    return true;
}

/**
 * send_file() - Send a file to a remote server
 *
 * Complete flow:
 *   1. Read the file from disk into memory (via read_file)
 *   2. If a passphrase is given, encrypt the file data with AES-256-GCM
 *   3. Create a TCP socket and connect to the target IP:port
 *   4. Send the file header (with \n delimiter for TCP stream framing)
 *   5. Send the payload in 4KB chunks, calling the progress callback
 *   6. Wait for an ACK or ERR response from the server
 *   7. Close the socket and return success/failure
 *
 * @param ip          Target IP address (e.g., "192.168.1.50")
 * @param port        Target TCP port (e.g., 9090)
 * @param filepath    Local path to the file to send
 * @param passphrase  Optional encryption key (empty = no encryption)
 * @param progress    Optional callback invoked after each chunk
 * @return            true if the server acknowledged success
 */
bool TcpClient::send_file(const std::string& ip, uint16_t port,
                           const std::string& filepath,
                           const std::string& passphrase,
                           ProgressCallback progress) {
    // Step 1: Read the file from disk
    FileData file;
    if (!read_file(filepath, file)) {
        std::cerr << "[TCP] Cannot read file: " << filepath << "\n";
        return false;
    }

    // Step 2: Optionally encrypt the file data
    bool encrypted = !passphrase.empty();
    std::vector<uint8_t> payload;

    if (encrypted) {
        // Convert file bytes to uint8_t vector for encryption,
        // encrypt with AES-256-GCM, and serialize for network transmission.
        // The encrypted payload is slightly larger than the original
        // due to the 16-byte salt, 12-byte IV, and 16-byte auth tag.
        std::vector<uint8_t> plain(file.bytes.begin(), file.bytes.end());
        auto blob = encrypt(plain, passphrase);
        payload = serialize_encrypted(blob);
        std::cerr << "[CRYPTO] Encrypted " << file.bytes.size()
                  << " bytes -> " << payload.size() << " bytes (AES-256-GCM)\n";
    } else {
        // No passphrase — send raw file bytes
        payload.assign(file.bytes.begin(), file.bytes.end());
    }

    // Step 3: Create a TCP socket and connect to the server
    socket_t sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCK) {
        std::cerr << "[TCP] Failed to create socket\n";
        return false;
    }

    // Set up the server address structure:
    // sin_family = AF_INET means IPv4
    // sin_port = htons(port) converts the port to network byte order
    // inet_pton converts the IP string to binary format
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

    // Step 4: Send the file header (with \n delimiter)
    // The header includes the filename, payload size, and encryption flag.
    FileHeader header{file.filename, payload.size(), encrypted};
    std::string header_str = serialize_file_header(header);
    if (!send_all(sock, header_str.c_str(), header_str.size())) {
        std::cerr << "[TCP] Failed to send header\n";
        SOCK_CLOSE(sock);
        return false;
    }

    // Step 5: Send the payload in 4KB chunks
    // Chunking prevents sending a huge file in one system call,
    // which could fail or block for too long.
    uint64_t total_sent = 0;
    uint64_t total_size = payload.size();

    while (total_sent < total_size) {
        size_t chunk = std::min(static_cast<uint64_t>(CHUNK_SIZE),
                                total_size - total_sent);
        if (!send_all(sock, reinterpret_cast<const char*>(payload.data() + total_sent),
                      chunk)) {
            std::cerr << "[TCP] Send failed\n";
            SOCK_CLOSE(sock);
            return false;
        }
        total_sent += chunk;

        // Report progress via callback or console output
        if (progress) {
            progress(total_sent, total_size);
        } else {
            uint64_t pct = (total_sent * 100) / total_size;
            std::cout << "\r[TCP] Sending: " << pct << "% (" << total_sent
                      << "/" << total_size << " bytes)" << std::flush;
        }
    }
    std::cout << "\n";

    // Step 6: Wait for the server's ACK or ERR response
    char resp_buf[128]{};
    int resp_len = recv(sock, resp_buf, sizeof(resp_buf) - 1, 0);
    std::string resp(resp_buf, resp_len);

    // Check if the response contains the ACK message type
    bool success = resp.find(std::to_string(static_cast<int>(MsgType::FILE_ACK)))
                   != std::string::npos;

    // Step 7: Close the socket and report result
    SOCK_CLOSE(sock);

    if (success) {
        std::cout << "[TCP] File sent successfully!\n";
    } else {
        std::cerr << "[TCP] Receiver reported error\n";
    }

    return success;
}

} // namespace uair