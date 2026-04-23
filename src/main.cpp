#include "universal_airdrop/tcp_server.h"
#include "universal_airdrop/tcp_client.h"
#include "universal_airdrop/udp_discovery.h"
#include "universal_airdrop/platform.h"
#include <iostream>
#include <string>
#include <atomic>
#include <thread>

static std::atomic<bool> g_running{true};

static void signal_handler(int) {
    g_running = false;
}

static void print_usage() {
    std::cout << R"(
Universal Airdrop - Cross-platform local file sharing

Usage:
  airdrop discover <your_name>                       Discover devices on the network
  airdrop send <ip> <port> <filepath> [passphrase]   Send a file (optional encryption)
  airdrop receive [port] [output_dir] [passphrase]  Receive files (optional encryption)

Options:
  When a passphrase is provided, files are encrypted with AES-256-GCM before transfer.

Examples:
  airdrop discover "John's Laptop"
  airdrop send 192.168.1.50 9090 ./photo.jpg
  airdrop send 192.168.1.50 9090 ./secret.txt mypassword
  airdrop receive 9090 ./downloads mypassword
)";
}

int cmd_discover(const std::string& name) {
    uair::UdpDiscovery discovery(name);

    discovery.set_on_device_found([](const uair::DeviceInfo& dev) {
        std::cout << "  -> " << dev.name << " (" << dev.os
                  << ") at " << dev.ip << ":" << dev.tcp_port << "\n";
    });

    if (!discovery.start()) return 1;

    uair::TcpServer server;
    server.set_receive_dir("./received");
    server.start();

    std::cout << "Scanning for devices... Press Ctrl+C to stop.\n\n";

    install_signal_handler(signal_handler);
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

int cmd_send(const std::string& ip, uint16_t port,
             const std::string& filepath, const std::string& passphrase) {
    uair::TcpClient client;
    bool ok = client.send_file(ip, port, filepath, passphrase,
        [](uint64_t sent, uint64_t total) {
            uint64_t pct = (sent * 100) / total;
            std::cout << "\r[TCP] Sending: " << pct << "%" << std::flush;
        });
    return ok ? 0 : 1;
}

int cmd_receive(uint16_t port, const std::string& output_dir,
                const std::string& passphrase) {
    uair::TcpServer server(port);
    server.set_receive_dir(output_dir);
    if (!passphrase.empty()) {
        server.set_passphrase(passphrase);
    }

    if (!server.start()) return 1;

    uair::UdpDiscovery discovery("AirdropReceiver");
    discovery.start();

    std::cout << "Waiting for incoming files on port " << port
              << ". Files saved to " << output_dir << "/\n";
    if (!passphrase.empty()) {
        std::cout << "[CRYPTO] Encryption enabled (AES-256-GCM)\n";
    }
    std::cout << "Press Ctrl+C to stop.\n";

    install_signal_handler(signal_handler);
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
        std::string passphrase = (argc >= 6) ? argv[5] : "";
        result = cmd_send(argv[2], static_cast<uint16_t>(std::stoi(argv[3])),
                         argv[4], passphrase);
    } else if (cmd == "receive") {
        uint16_t port = (argc >= 3) ? static_cast<uint16_t>(std::stoi(argv[2]))
                                    : uair::DEFAULT_TCP_PORT;
        std::string dir = (argc >= 4) ? argv[3] : "./received";
        std::string passphrase = (argc >= 5) ? argv[4] : "";
        result = cmd_receive(port, dir, passphrase);
    } else {
        print_usage();
    }

    cleanup_networking();
    return result;
}