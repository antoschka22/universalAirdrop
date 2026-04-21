#include "universal_airdrop/tcp_server.h"
#include "universal_airdrop/tcp_client.h"
#include "universal_airdrop/udp_discovery.h"
#include "universal_airdrop/platform.h"
#include <iostream>
#include <string>
#include <csignal>
#include <atomic>
#include <thread>
#include <cstring>

static std::atomic<bool> g_running{true};

static void signal_handler(int) {
    g_running = false;
}

static void print_usage() {
    std::cout << R"(
Universal Airdrop - Cross-platform local file sharing

Usage:
  airdrop discover <your_name>              Discover devices on the network
  airdrop send <ip> <port> <filepath>       Send a file to a device
  airdrop receive [port] [output_dir]       Receive files (default: port 9090, dir ./received)

Examples:
  airdrop discover "John's Laptop"
  airdrop send 192.168.1.50 9090 ./photo.jpg
  airdrop receive 9090 ./downloads
)";
}

int cmd_discover(const std::string& name) {
    uair::UdpDiscovery discovery(name);

    discovery.set_on_device_found([](const uair::DeviceInfo& dev) {
        std::cout << "  -> " << dev.name << " (" << dev.os
                  << ") at " << dev.ip << ":" << dev.tcp_port << "\n";
    });

    if (!discovery.start()) return 1;

    // Also start a TCP server so others can send us files
    uair::TcpServer server;
    server.set_receive_dir("./received");
    server.start();

    std::cout << "Scanning for devices... Press Ctrl+C to stop.\n\n";

    signal(SIGINT, signal_handler);
    while (g_running) {
        auto devices = discovery.get_devices();
        std::cout << "\n--- Known Devices (" << devices.size() << ") ---\n";
        for (size_t i = 0; i < devices.size(); ++i) {
            std::cout << "  [" << i << "] " << devices[i].name
                      << " (" << devices[i].os << ") "
                      << devices[i].ip << ":" << devices[i].tcp_port << "\n";
        }
        std::cout << "----------------------\n";
        std::this_thread::sleep_for(std::chrono::seconds(5));
    }

    discovery.stop();
    server.stop();
    return 0;
}

int cmd_send(const std::string& ip, uint16_t port, const std::string& filepath) {
    uair::TcpClient client;
    bool ok = client.send_file(ip, port, filepath, [](uint64_t sent, uint64_t total) {
        uint64_t pct = (sent * 100) / total;
        std::cout << "\r[TCP] Sending: " << pct << "%" << std::flush;
    });
    return ok ? 0 : 1;
}

int cmd_receive(uint16_t port, const std::string& output_dir) {
    uair::TcpServer server(port);
    server.set_receive_dir(output_dir);

    if (!server.start()) return 1;

    // Also start discovery so we appear on the network
    uair::UdpDiscovery discovery("AirdropReceiver");
    discovery.start();

    std::cout << "Waiting for incoming files on port " << port
              << ". Files saved to " << output_dir << "/\n";
    std::cout << "Press Ctrl+C to stop.\n";

    signal(SIGINT, signal_handler);
    while (g_running) {
        std::this_thread::sleep_for(std::chrono::milliseconds(500));
    }

    server.stop();
    discovery.stop();
    return 0;
}

int main(int argc, char* argv[]) {
    if (init_networking() != 0) {
        std::cerr << "Failed to initialize networking\n";
        return 1;
    }

    if (argc < 2) {
        print_usage();
        cleanup_networking();
        return 1;
    }

    std::string cmd = argv[1];
    int result = 1;

    if (cmd == "discover" && argc >= 3) {
        result = cmd_discover(argv[2]);
    } else if (cmd == "send" && argc >= 5) {
        result = cmd_send(argv[2], static_cast<uint16_t>(std::stoi(argv[3])), argv[4]);
    } else if (cmd == "receive") {
        uint16_t port = (argc >= 3) ? static_cast<uint16_t>(std::stoi(argv[2])) : uair::DEFAULT_TCP_PORT;
        std::string dir = (argc >= 4) ? argv[3] : "./received";
        result = cmd_receive(port, dir);
    } else {
        print_usage();
    }

    cleanup_networking();
    return result;
}