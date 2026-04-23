/**
 * udp_discovery.h - UDP broadcast-based device discovery
 *
 * The UdpDiscovery class makes devices visible to each other on the
 * local network. It does two things simultaneously:
 *   1. Broadcasts its own presence every 2 seconds via UDP
 *   2. Listens for other devices' broadcasts and maintains a list of peers
 *
 * How it works:
 *   - Each device creates TWO UDP sockets: one for broadcasting (with
 *     SO_BROADCAST enabled) and one for listening (bound to the UDP port).
 *   - The broadcast socket sends a discovery message to 255.255.255.255
 *     (the broadcast address) every DISCOVERY_INTERVAL_SEC seconds.
 *   - The listen socket receives broadcasts from other devices, parses
 *     them, and adds new devices to an internal list.
 *   - A user-provided callback (DeviceCallback) is called whenever a
 *     new device is discovered.
 *
 * Thread model:
 *   - broadcast_loop() runs on its own thread
 *   - listen_loop() runs on its own thread
 *   - The devices_ list is protected by devices_mutex_ for thread safety
 *   - The running_ atomic flag is used to signal both threads to stop
 *
 * Usage:
 *   UdpDiscovery discovery("My Laptop");
 *   discovery.set_on_device_found([](const DeviceInfo& d) { ... });
 *   discovery.start();
 *   // ... devices appear via callback ...
 *   discovery.stop();
 */
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
    using DeviceList = std::vector<DeviceInfo>; /**< List of discovered devices */
    using DeviceCallback = std::function<void(const DeviceInfo&)>; /**< Called when a new device is found */

    /**
     * Construct a discovery instance.
     * @param device_name  The name this device broadcasts to others
     * @param udp_port      Port for UDP broadcasts (default: 9091)
     * @param tcp_port      The TCP port this device listens on (included in broadcasts)
     */
    explicit UdpDiscovery(const std::string& device_name,
                          uint16_t udp_port = DEFAULT_UDP_PORT,
                          uint16_t tcp_port = DEFAULT_TCP_PORT);
    /** Stops both threads and closes sockets. */
    ~UdpDiscovery();

    /**
     * Start broadcasting and listening for devices.
     * Creates two sockets, binds the listener, and launches two threads.
     * @return true if both sockets were created and bound successfully
     */
    bool start();
    /** Signal both threads to stop, close sockets, and join threads. */
    void stop();

    /** Get a snapshot of all discovered devices (thread-safe). */
    DeviceList get_devices() const;

    /**
     * Register a callback to be called when a new device is found.
     * The callback runs on the listen thread, so it should be fast.
     * In the CLI, it prints the device info. In the Qt GUI, it emits
     * a signal that updates the device list widget.
     */
    void set_on_device_found(DeviceCallback cb);

private:
    /**
     * broadcast_loop() - Periodically send discovery messages
     *
     * Runs on its own thread. Builds a DeviceInfo with this device's
     * name, OS, and TCP port, serializes it, and sends it to the
     * broadcast address every DISCOVERY_INTERVAL_SEC seconds.
     */
    void broadcast_loop();

    /**
     * listen_loop() - Receive and process discovery messages from others
     *
     * Runs on its own thread. Blocks on recvfrom() waiting for UDP
     * packets. When a valid discovery message arrives from a device
     * that isn't already known, it adds it to the devices_ list
     * and calls the on_device_found_ callback.
     *
     * Skips messages from itself (by matching device_name_) to
     * avoid listing this device in its own discovery results.
     */
    void listen_loop();

    std::string device_name_;       /**< Name broadcast in discovery messages */
    uint16_t udp_port_;             /**< Port for UDP discovery */
    uint16_t tcp_port_;             /**< TCP port included in broadcasts */
    socket_t broadcast_sock_ = INVALID_SOCK; /**< Socket for sending broadcasts */
    socket_t listen_sock_ = INVALID_SOCK;    /**< Socket for receiving broadcasts */
    std::atomic<bool> running_{false};       /**< Flag to signal threads to stop */
    std::thread broadcast_thread_;           /**< Thread running broadcast_loop() */
    std::thread listen_thread_;              /**< Thread running listen_loop() */

    mutable std::mutex devices_mutex_; /**< Protects devices_ from concurrent access */
    DeviceList devices_;              /**< List of discovered peer devices */

    DeviceCallback on_device_found_;  /**< User-provided callback for new devices */
};

} // namespace uair