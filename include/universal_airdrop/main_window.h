#pragma once

#include <QMainWindow>
#include <QThread>
#include "universal_airdrop/gui_helpers.h"

namespace Ui {
class MainWindow;
}

namespace uair {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private slots:
    void on_scan_clicked();
    void on_send_clicked();
    void on_browse_file();
    void on_browse_dir();
    void on_receive_toggled(bool checked);
    void on_device_found(const DeviceInfo& device);
    void on_send_progress(uint64_t sent, uint64_t total);
    void on_send_finished(bool success);

private:
    Ui::MainWindow* ui;
    DiscoveryWorker* discovery_ = nullptr;
    QThread* send_thread_ = nullptr;
    ReceiveWorker* receiver_ = nullptr;
    std::vector<DeviceInfo> devices_;
};

} // namespace uair