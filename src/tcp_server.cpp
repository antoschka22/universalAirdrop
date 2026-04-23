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

bool TcpServer::start() {
    listen_sock_ = socket(AF_INET, SOCK_STREAM, 0);
    if (listen_sock_ == INVALID_SOCK) {
        std::cerr << "[TCP] Failed to create socket\n";
        return false;
    }

    int opt = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&opt), sizeof(opt));

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

void TcpServer::accept_loop() {
    while (running_) {
        sockaddr_in client_addr{};
        SOCKLEN_T addr_len = sizeof(client_addr);
        socket_t client_sock = accept(listen_sock_,
            reinterpret_cast<sockaddr*>(&client_addr), &addr_len);

        if (client_sock == INVALID_SOCK) continue;

        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str));

        std::thread(&TcpServer::handle_client, this, client_sock,
                     std::string(ip_str)).detach();
    }
}

void TcpServer::handle_client(socket_t client_sock, const std::string& client_ip) {
    std::cout << "[TCP] Connection from " << client_ip << "\n";

    // Read data until we find the header delimiter
    std::string header_str;
    std::vector<char> overflow;
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
            header_str += chunk.substr(0, delim_pos);
            size_t data_start = delim_pos + 1;
            if (data_start < chunk.size()) {
                overflow.insert(overflow.end(),
                               buf + data_start, buf + n);
            }
            break;
        }
        header_str += chunk;
    }

    FileHeader header{};
    if (!parse_file_header(header_str, header)) {
        std::cerr << "[TCP] Invalid file header\n";
        SOCK_CLOSE(client_sock);
        return;
    }

    std::cout << "[TCP] Receiving: " << header.filename
              << " (" << header.size << " bytes"
              << (header.encrypted ? ", encrypted" : "") << ") from " << client_ip << "\n";

    make_directory(receive_dir_.c_str());

    // Receive payload (encrypted or raw)
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
        // Decrypt the payload
        if (passphrase_.empty()) {
            std::cerr << "[CRYPTO] Encrypted file received but no passphrase set\n";
        } else {
            std::vector<uint8_t> enc_data(payload.begin(), payload.end());
            EncryptedBlob blob;
            if (!deserialize_encrypted(enc_data, blob)) {
                std::cerr << "[CRYPTO] Failed to deserialize encrypted data\n";
            } else {
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
        ok = write_file(receive_dir_, header.filename,
                        payload.data(), payload.size());
    }

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