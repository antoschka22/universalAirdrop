/**
 * udp_discovery.cpp - UDP broadcast-based device discovery implementation
 *
 * This file implements the UdpDiscovery class declared in udp_discovery.h.
 * It uses two UDP sockets operating on separate threads:
 *   - broadcast_sock_: sends discovery messages to 255.255.255.255 every 2s
 *   - listen_sock_: listens for other devices' broadcasts on the same port
 *
 * Key design decisions:
 *   - SO_BROADCAST is enabled on the broadcast socket (required by the OS
 *     to send packets to the broadcast address).
 *   - SO_REUSEADDR is enabled on the listen socket so multiple instances
 *     can bind to the same port (useful during development).
 *   - Devices are identified by name — broadcasts from our own device
 *     are skipped to avoid listing ourselves.
 *   - The devices_ list is protected by a mutex for thread-safe access.
 */
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

/**
 * start() - Create sockets, bind, and launch broadcast/listen threads
 *
 * This function sets up two UDP sockets:
 *   1. broadcast_sock_: Configured with SO_BROADCAST to send to
 *      255.255.255.255. No bind needed — the OS picks a source port.
 *   2. listen_sock_: Bound to udp_port_ so it can receive broadcasts
 *      from other devices on that port. SO_REUSEADDR allows quick
 *      rebinding after the program exits.
 *
 * Both threads run in the background; the calling thread can continue.
 *
 * @return true if both sockets were created and bound successfully
 */
bool UdpDiscovery::start() {
    // Create the broadcast socket and enable broadcast mode.
    // SO_BROADCAST is a socket option that tells the OS to allow
    // sending packets to the broadcast address (255.255.255.255).
    // Without this, sendto() to a broadcast address would fail.
    broadcast_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (broadcast_sock_ == INVALID_SOCK) {
        std::cerr << "[UDP] Failed to create broadcast socket\n";
        return false;
    }

    int broadcast_enable = 1;
    setsockopt(broadcast_sock_, SOL_SOCKET, SO_BROADCAST,
               reinterpret_cast<const char*>(&broadcast_enable),
               sizeof(broadcast_enable));

    // Create the listen socket and bind it to the UDP port.
    // SO_REUSEADDR allows the port to be reused quickly after the
    // program exits (similar to TCP SO_REUSEADDR).
    listen_sock_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (listen_sock_ == INVALID_SOCK) {
        std::cerr << "[UDP] Failed to create listen socket\n";
        SOCK_CLOSE(broadcast_sock_);
        return false;
    }

    int reuse = 1;
    setsockopt(listen_sock_, SOL_SOCKET, SO_REUSEADDR,
               reinterpret_cast<const char*>(&reuse), sizeof(reuse));

    // Bind to INADDR_ANY on the UDP port so we receive broadcasts
    // from any network interface on this machine.
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

/**
 * stop() - Signal threads to stop, close sockets, and join threads
 *
 * Sets running_ to false (which causes both loops to exit), then
 * closes both sockets (which unblocks any blocking recvfrom() calls).
 * Finally, joins both threads to ensure clean shutdown.
 */
void UdpDiscovery::stop() {
    running_ = false;
    // Closing the sockets will cause blocking recvfrom/sendto calls
    // to return with an error, unblocking the threads.
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

/**
 * get_devices() - Return a snapshot of all discovered devices
 *
 * Thread-safe: acquires the mutex to prevent data races with
 * the listen thread which may be adding devices concurrently.
 */
UdpDiscovery::DeviceList UdpDiscovery::get_devices() const {
    std::lock_guard<std::mutex> lock(devices_mutex_);
    return devices_;
}

/**
 * set_on_device_found() - Register a callback for new device discoveries
 *
 * The callback is called on the listen thread whenever a new device
 * is found. It should be fast and non-blocking. In the CLI, it
 * prints the device info. In the Qt GUI, it emits a signal.
 */
void UdpDiscovery::set_on_device_found(DeviceCallback cb) {
    on_device_found_ = std::move(cb);
}

/**
 * broadcast_loop() - Periodically send discovery messages
 *
 * Runs on its own thread. Builds a DeviceInfo with this device's
 * name, OS (from get_os_name()), and TCP port, serializes it to
 * the protocol format, and sends it to 255.255.255.255 every
 * DISCOVERY_INTERVAL_SEC seconds.
 *
 * The broadcast address means every device on the local network
 * will receive this packet (assuming they're listening on the
 * same UDP port).
 */
void UdpDiscovery::broadcast_loop() {
    sockaddr_in dest{};
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(BROADCAST_ADDR);
    dest.sin_port = htons(udp_port_);

    // Build our device info once — it doesn't change during the session
    DeviceInfo info;
    info.name = device_name_;
    info.os = get_os_name();  // Platform detection from platform.h
    info.tcp_port = tcp_port_;

    std::string payload = serialize_discovery(info);

    while (running_) {
        sendto(broadcast_sock_, payload.c_str(), payload.size(), 0,
               reinterpret_cast<sockaddr*>(&dest), sizeof(dest));
        std::this_thread::sleep_for(
            std::chrono::seconds(DISCOVERY_INTERVAL_SEC));
    }
}

/**
 * listen_loop() - Receive and process discovery broadcasts from other devices
 *
 * Runs on its own thread. Blocks on recvfrom() waiting for UDP packets.
 * When a valid discovery message arrives from a device that isn't already
 * known, it adds the device to the devices_ list and calls the
 * on_device_found_ callback.
 *
 * Key behaviors:
 *   - Skips messages from itself (by matching device_name_) to avoid
 *     listing this device in its own discovery results.
 *   - Extracts the sender's IP from the recvfrom() source address,
 *     since the sender doesn't need to know its own IP.
 *   - Uses a mutex to protect the devices_ list from concurrent access.
 */
void UdpDiscovery::listen_loop() {
    char buf[1024];
    while (running_) {
        sockaddr_in sender{};
        SOCKLEN_T sender_len = sizeof(sender);

        // Block until a UDP packet arrives
        int n = recvfrom(listen_sock_, buf, sizeof(buf) - 1, 0,
                         reinterpret_cast<sockaddr*>(&sender), &sender_len);
        if (n <= 0) continue;

        buf[n] = '\0';
        std::string msg(buf, n);

        // Try to parse the message as a discovery broadcast
        DeviceInfo device{};
        if (!parse_discovery(msg, device)) continue;

        // Extract the sender's IP address from the packet's source address.
        // This is more reliable than having the sender include its own IP,
        // because a device might not know its own IP (e.g., on multi-homed hosts).
        char ip_str[INET_ADDRSTRLEN];
        inet_ntop(AF_INET, &sender.sin_addr, ip_str, sizeof(ip_str));
        device.ip = ip_str;

        // Skip our own broadcasts — we don't want to discover ourselves
        if (device.name == device_name_) continue;

        // Add the device to our list if we haven't seen it before.
        // The mutex ensures thread-safe access to devices_.
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