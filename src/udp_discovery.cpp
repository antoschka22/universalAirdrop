#include "universal_airdrop/udp_discovery.h"
#include <iostream>
#include <cstring>

#ifdef _WIN32
    #include <iphlpapi.h>
#else
    #include <ifaddrs.h>
    #include <net/if.h>
#endif

namespace uair {

UdpDiscovery::UdpDiscovery(const std::string& device_name,
                           uint16_t udp_port, uint16_t tcp_port)
    : device_name_(device_name), udp_port_(udp_port), tcp_port_(tcp_port) {}

UdpDiscovery::~UdpDiscovery() {
    stop();
}

bool UdpDiscovery::start() {
    // Broadcast socket
    broadcast_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcast_sock_ == INVALID_SOCK) {
        std::cerr << "[UDP] Failed to create broadcast socket\n";
        return false;
    }

    int broadcast_enable = 1;
    setsockopt(broadcast_sock_, SOL_SOCKET, SO_BROADCAST,
               reinterpret_cast<const char*>(&broadcast_enable),
               sizeof(broadcast_enable));

    // Listen socket
    listen_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sock_ == INVALID_SOCK) {
        std::cerr << "[UDP] Failed to create listen socket\n";
        SOCK_CLOSE(broadcast_sock_);
        return false;
    }

    int reuse = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    sockaddr_in listen_addr{};
    listen_addr.sin_family = AF_INET;
    listen_addr.sin_addr.s_addr = INADDR_ANY;
    listen_addr.sin_port = htons(udp_port_);

    if (bind(listen_sock_, reinterpret_cast<sockaddr*>(&listen_addr),
             sizeof(listen_addr)) < 0) {
        std::cerr << "[UDP] Bind failed on port " << udp_port_ << "\n";
        SOCK_CLOSE(broadcast_sock_);
        SOCK_CLOSE(listen_sock_);
        return false;
    }

    running_ = true;
    broadcast_thread_ = std::thread(&UdpDiscovery::broadcast_loop, this);
    listen_thread_ = std::thread(&UdpDiscovery::listen_loop, this);

    std::cout << "[UDP] Discovery started on port " << udp_port_ << "\n";
    return true;
}

void UdpDiscovery::stop() {
    running_ = false;
    if (broadcast_sock_ != INVALID_SOCK) {
        SOCK_CLOSE(broadcast_sock_);
        broadcast_sock_ = INVALID_SOCK;
    }
    if (listen_sock_ != INVALID_SOCK) {
        SOCK_CLOSE(listen_sock_);
        listen_sock_ = INVALID_SOCK;
    }
    if (broadcast_thread_.joinable()) broadcast_thread_.join();
    if (listen_thread_.joinable()) listen_thread_.join();
}

UdpDiscovery::DeviceList UdpDiscovery::get_devices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return devices_;
}

void UdpDiscovery::set_on_device_found(DeviceCallback cb) {
    on_device_found_ = std::move(cb);
}

void UdpDiscovery::broadcast_loop() {
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    dest.sin_port = htons(udp_port_);

    DeviceInfo info;
    info.name = device_name_;
#ifdef _WIN32
    info.os = "Windows";
#elif __APPLE__
    info.os = "macOS";
#elif __linux__
    info.os = "Linux";
#else
    info.os = "Unknown";
#endif
    info.tcp_port = tcp_port_;

    std::string payload = serialize_discovery(info);

    while (running_) {
        sendto(broadcast_sock_, payload.c_str(), payload.size(), 0,
               reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
        std::this_thread::sleep_for(
            std::chrono::seconds(DISCOVERY_INTERVAL_SEC));
    }
}

void UdpDiscovery::listen_loop() {
    char buf[1024];
    while (running_) {
        sockaddr_in sender{};
        socklen_t sender_len = sizeof(sender);

        int n = recvfrom(listen_sock_, buf, sizeof(buf) - 1, 0,
                         reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n <= 0) continue;

        buf[n] = '\0';
        std::string msg(buf, n);

        DeviceInfo device{};
        if (!parse_discovery(msg, device)) continue;

        // Get sender IP
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender.sin_addr, ip_str, sizeof(ip_str));
        device.ip = ip_str;

        // Skip our own broadcasts (by name match)
        if (device.name == device_name_) continue;

        {
            std::lock_guard<std::mutex> lock(devices_mutex_);
            bool known = false;
            for (const auto& d : devices_) {
                if (d.name == device.name && d.ip == device.ip) {
                    known = true;
                    break;
                }
            }
            if (!known) {
                devices_.push_back(device);
                std::cout << "[UDP] Found device: " << device.name
                          << " (" << device.os << ") at " << device.ip
                          << ":" << device.tcp_port << "\n";
                if (on_device_found_) on_device_found_(device);
            }
        }
    }
}

} // namespace uair