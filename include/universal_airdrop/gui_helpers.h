#pragma once

#include <QObject>
#include <string>
#include "universal_airdrop/protocol.h"
#include "universal_airdrop/tcp_server.h"
#include "universal_airdrop/tcp_client.h"
#include "universal_airdrop/udp_discovery.h"

namespace uair {

// Bridge between raw C++ networking and Qt signals/slots.
// Runs networking on background threads, emits signals for UI updates.

class DiscoveryWorker : public QObject {
    Q_OBJECT
public:
    explicit DiscoveryWorker(const std::string& name, QObject* parent = nullptr);
    void start();
    void stop();

signals:
    void deviceFound(const uair::DeviceInfo& device);
    void deviceListUpdated(const std::vector<uair::DeviceInfo>& devices);

private:
    UdpDiscovery discovery_;
    TcpServer server_;
};

class SendWorker : public QObject {
    Q_OBJECT
public:
    explicit SendWorker(QObject* parent = nullptr);

    void send(const std::string& ip, uint16_t port,
              const std::string& filepath, const std::string& passphrase);

signals:
    void progress(uint64_t sent, uint64_t total);
    void finished(bool success);
};

class ReceiveWorker : public QObject {
    Q_OBJECT
public:
    explicit ReceiveWorker(QObject* parent = nullptr);

    void start(uint16_t port, const std::string& dir, const std::string& passphrase);
    void stop();

signals:
    void fileReceived(const std::string& filename, uint64_t size, bool encrypted);
    void started(uint16_t port);

private:
    TcpServer server_;
    UdpDiscovery discovery_;
};

} // namespace uair