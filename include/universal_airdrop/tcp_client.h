#pragma once

#include "platform.h"
#include "protocol.h"
#include <string>
#include <functional>

namespace uair {

class TcpClient {
public:
    using ProgressCallback = std::function<void(uint64_t sent, uint64_t total)>;

    bool send_file(const std::string& ip, uint16_t port,
                   const std::string& filepath,
                   ProgressCallback progress = nullptr);

private:
    bool send_all(socket_t sock, const char* data, size_t len);
};

} // namespace uair