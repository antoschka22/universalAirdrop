#include "universal_airdrop/gui_helpers.h"
#include "universal_airdrop/platform.h"
#include <thread>
#include <atomic>

namespace uair {

// ========== DiscoveryWorker ==========

DiscoveryWorker::DiscoveryWorker(const std::string& name, QObject* parent)
    : QObject(parent), discovery_(name), server_() {}

void DiscoveryWorker::start() {
    discovery_.set_on_device_found([this](const DeviceInfo& dev) {
        emit deviceFound(dev);
    });
    discovery_.start();
    server_.set_receive_dir("./received");
    server_.start();
}

void DiscoveryWorker::stop() {
    discovery_.stop();
    server_.stop();
}

// ========== SendWorker ==========

SendWorker::SendWorker(QObject* parent) : QObject(parent) {}

void SendWorker::send(const std::string& ip, uint16_t port,
                      const std::string& filepath,
                      const std::string& passphrase) {
    TcpClient client;
    bool ok = client.send_file(ip, port, filepath, passphrase,
        [this](uint64_t sent, uint64_t total) {
            emit progress(sent, total);
        });
    emit finished(ok);
}

// ========== ReceiveWorker ==========

ReceiveWorker::ReceiveWorker(QObject* parent)
    : QObject(parent), server_(nullptr), discovery_("AirdropReceiver") {}

void ReceiveWorker::start(uint16_t port, const std::string& dir,
                          const std::string& passphrase) {
    server_ = std::make_unique<TcpServer>(port);
    server_->set_receive_dir(dir);
    if (!passphrase.empty()) {
        server_->set_passphrase(passphrase);
    }
    server_->start();
    discovery_.start();
    emit started(port);
}

void ReceiveWorker::stop() {
    if (server_) {
        server_->stop();
    }
    discovery_.stop();
}

} // namespace uair