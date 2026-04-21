#pragma once

#include "platform.h"
#include "protocol.h"
#include <string>
#include <vector>
#include <mutex>
#include <atomic>
#include <thread>
#include <functional>

namespace uair {

class UdpDiscovery {
public:
    using DeviceList = std::vector<DeviceInfo>;
    using DeviceCallback = std::function<void(const DeviceInfo&)>;

    explicit UdpDiscovery(const std::string& device_name,
                          uint16_t udp_port = DEFAULT_UDP_PORT,
                          uint16_t tcp_port = DEFAULT_TCP_PORT);
    ~UdpDiscovery();

    bool start();
    void stop();

    DeviceList get_devices() const;
    void set_on_device_found(DeviceCallback cb);

private:
    void broadcast_loop();
    void listen_loop();

    std::string device_name_;
    uint16_t udp_port_;
    uint16_t tcp_port_;
    socket_t broadcast_sock_ = INVALID_SOCK;
    socket_t listen_sock_ = INVALID_SOCK;
    std::atomic<bool> running_{false};
    std::thread broadcast_thread_;
    std::thread listen_thread_;

    mutable std::mutex devices_mutex_;
    DeviceList devices_;

    DeviceCallback on_device_found_;
};

} // namespace uair