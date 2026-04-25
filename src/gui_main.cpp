#include "universal_airdrop/platform.h"
#include "universal_airdrop/main_window.h"
#include <QApplication>
#include <iostream>

int main(int argc, char* argv[]) {
    if (init_networking() != 0) {
        std::cerr << "Failed to initialize networking\n";
        return 1;
    }

    QApplication app(argc, argv);
    app.setApplicationName("Universal Airdrop");
    app.setApplicationVersion("1.0.0");

    uair::MainWindow window;
    window.show();

    int result = app.exec();
    cleanup_networking();
    return result;
}