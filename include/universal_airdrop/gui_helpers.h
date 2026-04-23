/**
 * gui_helpers.h - Qt bridge classes between networking code and GUI
 *
 * These classes wrap the raw C++ networking logic (TcpServer, TcpClient,
 * UdpDiscovery) and expose it through Qt's signal-slot mechanism. This
 * allows the GUI to receive updates from background threads safely.
 *
 * Why a bridge layer? The networking classes (TcpClient, TcpServer,
 * UdpDiscovery) are pure C++ with no Qt dependencies. They work in
 * both the CLI and GUI builds. The bridge classes add QObject inheritance
 * and Qt signals, keeping the networking code reusable outside of Qt
 * while enabling clean integration with the GUI.
 *
 * Thread safety: SendWorker runs on a QThread (via moveToThread) so
 * the file transfer doesn't freeze the GUI. Qt's signal-slot mechanism
 * uses queued connections to safely pass data between threads.
 */
#pragma once

#include <QObject>
#include <string>
#include "universal_airdrop/protocol.h"
#include "universal_airdrop/tcp_server.h"
#include "universal_airdrop/tcp_client.h"
#include "universal_airdrop/udp_discovery.h"

namespace uair {

/**
 * DiscoveryWorker - Manages UDP discovery and a TCP server for the GUI
 *
 * Wraps UdpDiscovery and TcpServer. When started, it begins broadcasting
 * discovery messages and listening for incoming file transfers.
 * Emits Qt signals when new devices are found.
 */
class DiscoveryWorker : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryWorker(const std::string& name, QObject* parent = nullptr);
    void start();
    void stop();

signals:
    /** Emitted when a new device is discovered on the network */
    void deviceFound(const uair::DeviceInfo& device);
    /** Emitted when the device list changes */
    void deviceListUpdated(const std::vector<uair::DeviceInfo>& devices);

private:
    UdpDiscovery discovery_;
    TcpServer server_;
};

/**
 * SendWorker - Sends a file on a background thread
 *
 * Created when the user clicks "Send File". The send operation is
 * moved to a QThread so the GUI stays responsive during the transfer.
 * Emits progress and finished signals.
 */
class SendWorker : public QObject {
    Q_OBJECT
public:
    explicit SendWorker(QObject* parent = nullptr);

    /**
     * Send a file to a remote device. Call this from the thread
     * the worker is moved to (via QThread + moveToThread).
     */
    void send(const std::string& ip, uint16_t port,
              const std::string& filepath, const std::string& passphrase);

signals:
    /** Emitted periodically during the transfer with bytes sent/total */
    void progress(uint64_t sent, uint64_t total);
    /** Emitted when the transfer completes (success or failure) */
    void finished(bool success);
};

/**
 * ReceiveWorker - Manages a TCP server and discovery for the GUI
 *
 * Used in the Receive tab to start/stop a listening server.
 * Also starts a UdpDiscovery so the device appears on the network.
 */
class ReceiveWorker : public QObject {
    Q_OBJECT
public:
    explicit ReceiveWorker(QObject* parent = nullptr);

    void start(uint16_t port, const std::string& dir, const std::string& passphrase);
    void stop();

signals:
    /** Eitted when a file is received */
    void fileReceived(const std::string& filename, uint64_t size, bool encrypted);
    /** Emitted when the server starts listening */
    void started(uint16_t port);

private:
    TcpServer server_;
    UdpDiscovery discovery_;
};

} // namespace uair