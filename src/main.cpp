/**
 * main.cpp - CLI entry point for Universal Airdrop
 *
 * This file ties all the networking modules together into a command-line
 * interface with three commands:
 *
 *   airdrop discover <name>              — Find devices on the network
 *   airdrop send <ip> <port> <file> [pw] — Send a file (optionally encrypted)
 *   airdrop receive [port] [dir] [pw]    — Receive files (optionally encrypted)
 *
 * Program flow:
 *   1. Initialize networking (WSAStartup on Windows, no-op on Unix)
 *   2. Parse command-line arguments
 *   3. Dispatch to the appropriate command function
 *   4. Clean up networking (WSACleanup on Windows)
 *
 * The discover and receive commands run until Ctrl+C is pressed.
 * The signal handler sets a global atomic flag that causes the
 * main loop to exit gracefully.
 */
#include "universal_airdrop/tcp_server.h"
#include "universal_airdrop/tcp_client.h"
#include "universal_airdrop/udp_discovery.h"
#include "universal_airdrop/platform.h"
#include <iostream>
#include <string>
#include <atomic>
#include <thread>

/** Global flag set by the signal handler to request graceful shutdown */
static std::atomic<bool> g_running{true};

/**
 * signal_handler() - Called when the user presses Ctrl+C
 *
 * Sets g_running to false, which causes the main loop in discover
 * and receive commands to exit. On Unix, we use sigaction() (via
 * install_signal_handler()) which handles both SIGINT and SIGTERM.
 */
static void signal_handler(int) {
    g_running = false;
}

/**
 * print_usage() - Display CLI usage instructions
 */
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

/**
 * cmd_discover() - Discover devices on the local network
 *
 * This command starts both a UdpDiscovery (to find other devices)
 * and a TcpServer (so others can send files to us). It displays
 * the list of known devices every 5 seconds and stops when
 * Ctrl+C is pressed.
 *
 * @param name  The device name to broadcast (shown to other users)
 * @return      0 on success, 1 on error
 */
int cmd_discover(const std::string& name) {
    uair::UdpDiscovery discovery(name);

    // Register a callback that prints device info when a new device is found
    discovery.set_on_device_found([](const uair::DeviceInfo& dev) {
        std::cout << "  -> " << dev.name << " (" << dev.os
                  << ") at " << dev.ip << ":" << dev.tcp_port << "\n";
    });

    if (!discovery.start()) return 1;

    // Also start a TCP server so others can send us files while we're discovering
    uair::TcpServer server;
    server.set_receive_dir("./received");
    server.start();

    std::cout << "Scanning for devices... Press Ctrl+C to stop.\n\n";

    // Install signal handler for graceful shutdown on Ctrl+C
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

/**
 * cmd_send() - Send a file to a remote device
 *
 * Creates a TcpClient and sends the specified file. If a passphrase
 * is provided, the file is encrypted with AES-256-GCM before sending.
 * A progress callback updates the terminal with the percentage sent.
 *
 * @param ip          Target device IP address
 * @param port        Target device TCP port
 * @param filepath    Path to the local file to send
 * @param passphrase  Optional encryption passphrase (empty = no encryption)
 * @return            0 on success, 1 on failure
 */
int cmd_send(const std::string& ip, uint16_t port,
             const std::string& filepath, const std::string& passphrase) {
    uair::TcpClient client;
    bool ok = client.send_file(ip, port, filepath, passphrase, [](uint64_t sent, uint64_t total) {
        uint64_t pct = (sent * 100) / total;
        std::cout << "\r[TCP] Sending: " << pct << "%" << std::flush;
    });
    return ok ? 0 : 1;
}

/**
 * cmd_receive() - Start a server to receive files
 *
 * Creates a TcpServer and a UdpDiscovery. The server listens for
 * incoming file transfers and saves them to the output directory.
 * The discovery broadcasts our presence so senders can find us.
 * If a passphrase is provided, incoming encrypted files are
 * decrypted using it.
 *
 * Runs until Ctrl+C is pressed.
 *
 * @param port        TCP port to listen on
 * @param output_dir  Directory to save received files
 * @param passphrase  Optional decryption passphrase
 * @return            0 on success, 1 if the server fails to start
 */
int cmd_receive(uint16_t port, const std::string& output_dir,
                const std::string& passphrase) {
    uair::TcpServer server(port);
    server.set_receive_dir(output_dir);
    if (!passphrase.empty()) {
        server.set_passphrase(passphrase);
    }

    if (!server.start()) return 1;

    // Start discovery so senders can find us on the network
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

/**
 * main() - Program entry point
 *
 * Initializes the networking stack (required on Windows), parses
 * command-line arguments, dispatches to the appropriate command,
 * and cleans up before exiting.
 *
 * On Windows, WSAStartup() must be called before any socket functions.
 * On Unix, init_networking() is a no-op. Similarly, cleanup_networking()
 * calls WSACleanup() on Windows and does nothing on Unix.
 */
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
        // Optional 5th argument: passphrase for encryption
        std::string passphrase = (argc >= 6) ? argv[5] : "";
        result = cmd_send(argv[2], static_cast<uint16_t>(std::stoi(argv[3])),
                         argv[4], passphrase);
    } else if (cmd == "receive") {
        // All arguments are optional with sensible defaults
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