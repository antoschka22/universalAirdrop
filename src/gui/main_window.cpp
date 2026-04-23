#include "universal_airdrop/main_window.h"
#include "ui_main_window.h"
#include <QFileDialog>
#include <QMessageBox>
#include <QStandardPaths>

namespace uair {

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), ui(new Ui::MainWindow) {
    ui->setupUi(this);

    connect(ui->scanButton, &QPushButton::clicked,
            this, &MainWindow::on_scan_clicked);
    connect(ui->sendButton, &QPushButton::clicked,
            this, &MainWindow::on_send_clicked);
    connect(ui->browseButton, &QPushButton::clicked,
            this, &MainWindow::on_browse_file);
    connect(ui->dirBrowseButton, &QPushButton::clicked,
            this, &MainWindow::on_browse_dir);
    connect(ui->receiveButton, &QPushButton::toggled,
            this, &MainWindow::on_receive_toggled);

    // Double-click a device to fill in its IP
    connect(ui->deviceList, &QListWidget::itemDoubleClicked, [this](QListWidgetItem* item) {
        int row = ui->deviceList->row(item);
        if (row >= 0 && row < static_cast<int>(devices_.size())) {
            ui->ipEdit->setText(QString::fromStdString(devices_[row].ip));
            ui->portSpin->setValue(devices_[row].tcp_port);
            ui->tabWidget->setCurrentIndex(1);
        }
    });

    statusBar()->showMessage("Ready");
}

MainWindow::~MainWindow() {
    if (discovery_) {
        discovery_->stop();
    }
    if (receiver_) {
        receiver_->stop();
    }
    delete ui;
}

void MainWindow::on_scan_clicked() {
    if (discovery_) {
        discovery_->stop();
        delete discovery_;
        discovery_ = nullptr;
        ui->scanButton->setText("Scan");
        ui->deviceList->clear();
        devices_.clear();
        statusBar()->showMessage("Discovery stopped");
        return;
    }

    QString name = ui->nameEdit->text().trimmed();
    if (name.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please enter a device name.");
        return;
    }

    discovery_ = new DiscoveryWorker(name.toStdString(), this);
    connect(discovery_, &DiscoveryWorker::deviceFound,
            this, &MainWindow::on_device_found);
    discovery_->start();
    ui->scanButton->setText("Stop");
    statusBar()->showMessage("Scanning for devices...");
}

void MainWindow::on_device_found(const DeviceInfo& device) {
    devices_.push_back(device);
    QString label = QString("%1 (%2) - %3:%4")
        .arg(QString::fromStdString(device.name))
        .arg(QString::fromStdString(device.os))
        .arg(QString::fromStdString(device.ip))
        .arg(device.tcp_port);
    ui->deviceList->addItem(label);
    statusBar()->showMessage(
        QString("Found device: %1").arg(QString::fromStdString(device.name)));
}

void MainWindow::on_send_clicked() {
    QString ip = ui->ipEdit->text().trimmed();
    uint16_t port = static_cast<uint16_t>(ui->portSpin->value());
    QString filepath = ui->fileEdit->text().trimmed();
    QString passphrase = ui->passphraseEdit->text().toStdString().empty()
                             ? "" : ui->passphraseEdit->text();

    if (ip.isEmpty() || filepath.isEmpty()) {
        QMessageBox::warning(this, "Error", "Please fill in IP address and file path.");
        return;
    }

    ui->sendButton->setEnabled(false);
    ui->sendProgress->setValue(0);
    ui->sendStatus->setText("Sending...");

    auto* worker = new SendWorker();
    auto* thread = new QThread;
    worker->moveToThread(thread);

    connect(thread, &QThread::started, [worker, ip, port, filepath, passphrase]() {
        worker->send(ip.toStdString(), port,
                     filepath.toStdString(), passphrase.toStdString());
    });
    connect(worker, &SendWorker::progress, this, [this](uint64_t sent, uint64_t total) {
        ui->sendProgress->setValue(static_cast<int>((sent * 100) / total));
    });
    connect(worker, &SendWorker::finished, this, [this](bool success) {
        ui->sendButton->setEnabled(true);
        ui->sendStatus->setText(success ? "Sent successfully!" : "Send failed.");
        if (success) {
            ui->sendProgress->setValue(100);
            statusBar()->showMessage("File sent successfully");
        } else {
            statusBar()->showMessage("File send failed");
        }
    });
    connect(worker, &SendWorker::finished, thread, &QThread::quit);
    connect(thread, &QThread::finished, worker, &QObject::deleteLater);
    connect(thread, &QThread::finished, thread, &QObject::deleteLater);

    send_thread_ = thread;
    thread->start();
}

void MainWindow::on_browse_file() {
    QString path = QFileDialog::getOpenFileName(this, "Select File to Send");
    if (!path.isEmpty()) {
        ui->fileEdit->setText(path);
    }
}

void MainWindow::on_browse_dir() {
    QString dir = QFileDialog::getExistingDirectory(this, "Select Save Directory");
    if (!dir.isEmpty()) {
        ui->dirEdit->setText(dir);
    }
}

void MainWindow::on_receive_toggled(bool checked) {
    if (checked) {
        uint16_t port = static_cast<uint16_t>(ui->recvPortSpin->value());
        QString dir = ui->dirEdit->text().trimmed();
        QString passphrase = ui->recvPassphraseEdit->text();

        receiver_ = new ReceiveWorker(this);
        connect(receiver_, &ReceiveWorker::started, this, [this, port]() {
            ui->receiveStatus->setText(
                QString("Listening on port %1...").arg(port));
            statusBar()->showMessage("Receiving files...");
        });

        receiver_->start(port, dir.toStdString(), passphrase.toStdString());
        ui->receiveButton->setText("Stop Receiving");
    } else {
        if (receiver_) {
            receiver_->stop();
            delete receiver_;
            receiver_ = nullptr;
        }
        ui->receiveStatus->setText("Not receiving");
        ui->receiveButton->setText("Start Receiving");
        statusBar()->showMessage("Ready");
    }
}

} // namespace uair