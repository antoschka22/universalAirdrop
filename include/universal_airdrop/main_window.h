/**
 * main_window.h - Main Qt GUI window class
 *
 * The MainWindow class manages the three-tab interface:
 *   - Discover tab: find devices on the network
 *   - Send tab: send a file to a device (with optional encryption)
 *   - Receive tab: receive files (with optional decryption)
 *
 * It uses Qt's signal-slot mechanism to connect UI events (button clicks,
 * list double-clicks) to networking operations. File transfers run on
 * background QThreads to keep the GUI responsive.
 *
 * Double-clicking a device in the Discover tab auto-fills its IP
 * and port in the Send tab for convenience.
 */
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
    /** Start or stop device scanning */
    void on_scan_clicked();
    /** Send a file to the specified IP/port */
    void on_send_clicked();
    /** Open a file picker dialog */
    void on_browse_file();
    /** Open a directory picker dialog */
    void on_browse_dir();
    /** Toggle receiving on/off */
    void on_receive_toggled(bool checked);
    /** Called when a new device is discovered */
    void on_device_found(const DeviceInfo& device);
    /** Called during file transfer with progress updates */
    void on_send_progress(uint64_t sent, uint64_t total);
    /** Called when a file transfer completes */
    void on_send_finished(bool success);

private:
    Ui::MainWindow* ui;
    DiscoveryWorker* discovery_ = nullptr;
    QThread* send_thread_ = nullptr;
    ReceiveWorker* receiver_ = nullptr;
    std::vector<DeviceInfo> devices_;
};

} // namespace uair