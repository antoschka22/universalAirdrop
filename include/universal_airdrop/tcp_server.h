#pragma once

#include "platform.h"
#include "protocol.h"
#include <string>
#include <functional>
#include <atomic>
#include <thread>

namespace uair {

class TcpServer {
public:
    using ProgressCallback = std::function<void(uint64_t sent, uint64_t total)>;

    explicit TcpServer(uint16_t port = DEFAULT_TCP_PORT);
    ~TcpServer();

    bool start();
    void stop();
    void set_receive_dir(const std::string& dir);

private:
    void accept_loop();
    void handle_client(socket_t client_sock, const std::string& client_ip);

    uint16_t port_;
    std::string receive_dir_ = "./received";
    socket_t listen_sock_ = INVALID_SOCK;
    std::atomic<bool> running_{false};
    std::thread accept_thread_;
};

} // namespace uair