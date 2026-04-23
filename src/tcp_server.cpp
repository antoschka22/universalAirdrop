/**
 * tcp_server.cpp - TCP server implementation for receiving files
 *
 * This file implements the TcpServer class declared in tcp_server.h.
 * The server listens for incoming connections, receives file data,
 * optionally decrypts it, writes it to disk, and sends back an
 * acknowledgment (ACK) or error (ERR) response.
 *
 * Key design decisions:
 *   - The server spawns a detached thread per client connection.
 *     This allows multiple concurrent file transfers.
 *   - TCP stream framing uses a newline delimiter between the header
 *     and file data. The "overflow" mechanism preserves any file data
 *     that arrives in the same recv() call as the header.
 *   - Decryption is optional — it only happens when the header indicates
 *     an encrypted file AND a passphrase has been set on the server.
 */
#include "universal_airdrop/tcp_server.h"
#include "universal_airdrop/file_transfer.h"
#include "universal_airdrop/crypto.h"
#include <iostream>
#include <cstring>

namespace uair {

TcpServer::TcpServer(uint16_t port) : port_(port) {}

TcpServer::~TcpServer() {
    stop();
}

/**
 * start() - Bind to the port and begin listening for connections
 *
 * The server setup follows the standard TCP server pattern:
 *   1. socket() — Create a TCP socket
 *   2. setsockopt(SO_REUSEADDR) — Allow port reuse immediately after exit.
 *      Without this, the OS keeps the port in a "TIME_WAIT" state for
 *      several minutes after the program exits, preventing quick restarts.
 *   3. bind() — Associate the socket with the port and INADDR_ANY
 *      (listen on all network interfaces, not just one)
 *   4. listen() — Start accepting connections. The backlog of 5 means
 *      up to 5 connections can be waiting to be accepted before the
 *      OS starts refusing new ones.
 *   5. Launch accept_loop() on a background thread
 *
 * @return  true if the server started successfully, false on error
 */
bool TcpServer::start() {
    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ == INVALID_SOCK) {
        std::cerr << "[TCP] Failed to create socket\n";
        return false;
    }

    // SO_REUSEADDR allows the port to be reused immediately after the
    // program exits, instead of waiting for the OS TIME_WAIT timeout.
    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

    // Bind to the specified port on all network interfaces
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = INADDR_ANY;
    addr.sin_port = htons(port_);

    if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
        std::cerr << "[TCP] Bind failed on port " << port_ << "\n";
        SOCK_CLOSE(listen_sock_);
        listen_sock_ = INVALID_SOCK;
        return false;
    }

    // Start listening for connections (backlog of 5 pending connections)
    if (listen(listen_sock_, 5) < 0) {
        std::cerr << "[TCP] Listen failed\n";
        SOCK_CLOSE(listen_sock_);
        listen_sock_ = INVALID_SOCK;
        return false;
    }

    running_ = true;
    accept_thread_ = std::thread(&TcpServer::accept_loop, this);
    std::cout << "[TCP] Server listening on port " << port_ << "\n";
    return true;
}

/**
 * stop() - Shut down the server cleanly
 *
 * Sets running_ to false (signaling the accept loop to exit),
 * closes the listening socket (which causes accept() to return
 * with an error, unblocking the thread), and joins the thread.
 */
void TcpServer::stop() {
    running_ = false;
    if (listen_sock_ != INVALID_SOCK) {
        SOCK_CLOSE(listen_sock_);
        listen_sock_ = INVALID_SOCK;
    }
    if (accept_thread_.joinable()) {
        accept_thread_.join();
    }
}

void TcpServer::set_receive_dir(const std::string& dir) {
    receive_dir_ = dir;
}

void TcpServer::set_passphrase(const std::string& passphrase) {
    passphrase_ = passphrase;
}

/**
 * accept_loop() - Background thread that accepts incoming connections
 *
 * Runs in a loop calling accept() on the listening socket. When a
 * client connects, accept() returns a NEW socket (client_sock) that
 * represents this specific connection. The original listen_sock_
 * continues listening for more connections.
 *
 * Each client is handled on a detached thread so that multiple
 * file transfers can happen simultaneously. detach() means we
 * don't need to track or join the thread — it cleans up on its own.
 */
void TcpServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        SOCKLEN_T addr_len = sizeof(client_addr);
        socket_t client_sock = accept(listen_sock_,
            reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (client_sock == INVALID_SOCK) continue;

        // Convert the client's IP address from binary to string form
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

        // Handle this client on a separate thread
        std::thread(&TcpServer::handle_client, this, client_sock,
                     std::string(ip_str)).detach();
    }
}

/**
 * handle_client() - Handle a single file transfer from a connected client
 *
 * This is the most complex function in the server. It must handle
 * TCP stream framing correctly — the header and file data might
 * arrive in the same recv() call, or the header might be split
 * across multiple recv() calls.
 *
 * The solution: we read data until we find the \n delimiter.
 * Everything before \n is the header; everything after is file data.
 * This "overflow" mechanism preserves bytes that arrive in the same
 * recv() call as the header.
 *
 * After receiving the payload, if it's encrypted, we decrypt it
 * using the set passphrase (or report an error if no passphrase
 * is set or the passphrase is wrong).
 *
 * Finally, we send back an ACK or ERR response so the sender
 * knows whether the transfer succeeded.
 */
void TcpServer::handle_client(socket_t client_sock, const std::string& client_ip) {
    std::cout << "[TCP] Connection from " << client_ip << "\n";

    // Read data from the socket until we find the \n header delimiter.
    // TCP is a stream protocol, so the header and file data might arrive
    // together in one recv() call, or the header might span multiple calls.
    std::string header_str;
    std::vector<char> overflow;  // Bytes after the \n that belong to file data
    char buf[4096];

    while (true) {
        int n = recv(client_sock, buf, sizeof(buf), 0);
        if (n <= 0) {
            std::cerr << "[TCP] Failed to read header\n";
            SOCK_CLOSE(client_sock);
            return;
        }

        std::string chunk(buf, n);
        auto delim_pos = chunk.find(HEADER_DELIM);
        if (delim_pos != std::string::npos) {
            // Found the newline — everything before it is the header
            header_str += chunk.substr(0, delim_pos);
            // Everything after the newline is file data (overflow).
            // We must preserve these bytes — they're the start of the file.
            size_t data_start = delim_pos + 1;
            if (data_start < chunk.size()) {
                overflow.insert(overflow.end(),
                               buf + data_start, buf + n);
            }
            break;
        }
        // No newline yet — accumulate and keep reading
        header_str += chunk;
    }

    // Parse the header to get filename, size, and encryption flag
    FileHeader header{};
    if (!parse_file_header(header_str, header)) {
        std::cerr << "[TCP] Invalid file header\n";
        SOCK_CLOSE(client_sock);
        return;
    }

    std::cout << "[TCP] Receiving: " << header.filename
              << " (" << header.size << " bytes"
              << (header.encrypted ? ", encrypted" : "") << ") from " << client_ip << "\n";

    // Create the receive directory if it doesn't exist
    make_directory(receive_dir_.c_str());

    // Receive the payload (encrypted or raw).
    // Start with any overflow bytes from the header read,
    // then continue reading from the socket until we have header.size bytes.
    std::vector<char> payload;
    payload.reserve(header.size);
    uint64_t received = 0;

    if (!overflow.empty()) {
        size_t take = std::min(overflow.size(),
                               static_cast<size_t>(header.size));
        payload.insert(payload.end(), overflow.begin(),
                       overflow.begin() + take);
        received += take;
    }

    while (received < header.size) {
        size_t to_read = std::min(static_cast<uint64_t>(CHUNK_SIZE),
                                  header.size - received);
        int n = recv(client_sock, buf, to_read, 0);
        if (n <= 0) break;
        payload.insert(payload.end(), buf, buf + n);
        received += n;

        uint64_t pct = (received * 100) / header.size;
        std::cout << "\r[TCP] Progress: " << pct << "% (" << received
                  << "/" << header.size << " bytes)" << std::flush;
    }
    std::cout << "\n";

    bool ok = false;

    if (header.encrypted) {
        // The file was sent with encryption. Attempt to decrypt it.
        if (passphrase_.empty()) {
            std::cerr << "[CRYPTO] Encrypted file received but no passphrase set\n";
        } else {
            // Convert the raw bytes into an EncryptedBlob structure
            std::vector<uint8_t> enc_data(payload.begin(), payload.end());
            EncryptedBlob blob;
            if (!deserialize_encrypted(enc_data, blob)) {
                std::cerr << "[CRYPTO] Failed to deserialize encrypted data\n";
            } else {
                // Decrypt — if the passphrase is wrong, decrypt() returns empty
                auto decrypted = decrypt(blob, passphrase_);
                if (decrypted.empty()) {
                    std::cerr << "[CRYPTO] Decryption failed (wrong passphrase?)\n";
                } else {
                    std::cerr << "[CRYPTO] Decrypted " << payload.size()
                              << " -> " << decrypted.size() << " bytes\n";
                    ok = write_file(receive_dir_, header.filename,
                                    reinterpret_cast<const char*>(decrypted.data()),
                                    decrypted.size());
                }
            }
        }
    } else {
        // Unencrypted file — write directly to disk
        ok = write_file(receive_dir_, header.filename,
                        payload.data(), payload.size());
    }

    // Send ACK (success) or ERR (failure) response to the sender
    MsgType resp = ok ? MsgType::FILE_ACK : MsgType::FILE_ERR;
    std::string resp_str = std::string(MAGIC) + ":" +
                           std::to_string(static_cast<int>(resp)) + ":";
    send(client_sock, resp_str.c_str(), resp_str.size(), 0);

    if (ok) {
        std::cout << "[TCP] Saved: " << receive_dir_ << "/" << header.filename << "\n";
    } else {
        std::cerr << "[TCP] Failed to save file\n";
    }

    SOCK_CLOSE(client_sock);
}

} // namespace uair