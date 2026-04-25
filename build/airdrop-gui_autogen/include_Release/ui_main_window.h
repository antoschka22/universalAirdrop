/********************************************************************************
** Form generated from reading UI file 'main_window.ui'
**
** Created by: Qt User Interface Compiler version 6.8.2
**
** WARNING! All changes made in this file will be lost when recompiling UI file!
********************************************************************************/

#ifndef UI_MAIN_WINDOW_H
#define UI_MAIN_WINDOW_H

#include <QtCore/QVariant>
#include <QtWidgets/QApplication>
#include <QtWidgets/QFormLayout>
#include <QtWidgets/QHBoxLayout>
#include <QtWidgets/QLabel>
#include <QtWidgets/QLineEdit>
#include <QtWidgets/QListWidget>
#include <QtWidgets/QMainWindow>
#include <QtWidgets/QProgressBar>
#include <QtWidgets/QPushButton>
#include <QtWidgets/QSpinBox>
#include <QtWidgets/QStatusBar>
#include <QtWidgets/QTabWidget>
#include <QtWidgets/QTextEdit>
#include <QtWidgets/QVBoxLayout>
#include <QtWidgets/QWidget>

QT_BEGIN_NAMESPACE

class Ui_MainWindow
{
public:
    QWidget *centralWidget;
    QVBoxLayout *mainLayout;
    QTabWidget *tabWidget;
    QWidget *discoverTab;
    QVBoxLayout *discoverLayout;
    QHBoxLayout *nameLayout;
    QLabel *nameLabel;
    QLineEdit *nameEdit;
    QPushButton *scanButton;
    QListWidget *deviceList;
    QWidget *sendTab;
    QVBoxLayout *sendLayout;
    QFormLayout *sendForm;
    QLabel *ipLabel;
    QLineEdit *ipEdit;
    QLabel *portLabel;
    QSpinBox *portSpin;
    QLabel *fileLabel;
    QHBoxLayout *filePickerLayout;
    QLineEdit *fileEdit;
    QPushButton *browseButton;
    QLabel *passLabel;
    QLineEdit *passphraseEdit;
    QPushButton *sendButton;
    QProgressBar *sendProgress;
    QLabel *sendStatus;
    QWidget *receiveTab;
    QVBoxLayout *receiveLayout;
    QFormLayout *receiveForm;
    QLabel *recvPortLabel;
    QSpinBox *recvPortSpin;
    QLabel *dirLabel;
    QHBoxLayout *dirPickerLayout;
    QLineEdit *dirEdit;
    QPushButton *dirBrowseButton;
    QLabel *recvPassLabel;
    QLineEdit *recvPassphraseEdit;
    QPushButton *receiveButton;
    QLabel *receiveStatus;
    QTextEdit *receiveLog;
    QStatusBar *statusBar;

    void setupUi(QMainWindow *MainWindow)
    {
        if (MainWindow->objectName().isEmpty())
            MainWindow->setObjectName("MainWindow");
        MainWindow->resize(640, 480);
        centralWidget = new QWidget(MainWindow);
        centralWidget->setObjectName("centralWidget");
        mainLayout = new QVBoxLayout(centralWidget);
        mainLayout->setObjectName("mainLayout");
        tabWidget = new QTabWidget(centralWidget);
        tabWidget->setObjectName("tabWidget");
        discoverTab = new QWidget();
        discoverTab->setObjectName("discoverTab");
        discoverLayout = new QVBoxLayout(discoverTab);
        discoverLayout->setObjectName("discoverLayout");
        nameLayout = new QHBoxLayout();
        nameLayout->setObjectName("nameLayout");
        nameLabel = new QLabel(discoverTab);
        nameLabel->setObjectName("nameLabel");

        nameLayout->addWidget(nameLabel);

        nameEdit = new QLineEdit(discoverTab);
        nameEdit->setObjectName("nameEdit");

        nameLayout->addWidget(nameEdit);

        scanButton = new QPushButton(discoverTab);
        scanButton->setObjectName("scanButton");

        nameLayout->addWidget(scanButton);


        discoverLayout->addLayout(nameLayout);

        deviceList = new QListWidget(discoverTab);
        deviceList->setObjectName("deviceList");

        discoverLayout->addWidget(deviceList);

        tabWidget->addTab(discoverTab, QString());
        sendTab = new QWidget();
        sendTab->setObjectName("sendTab");
        sendLayout = new QVBoxLayout(sendTab);
        sendLayout->setObjectName("sendLayout");
        sendForm = new QFormLayout();
        sendForm->setObjectName("sendForm");
        ipLabel = new QLabel(sendTab);
        ipLabel->setObjectName("ipLabel");

        sendForm->setWidget(0, QFormLayout::LabelRole, ipLabel);

        ipEdit = new QLineEdit(sendTab);
        ipEdit->setObjectName("ipEdit");

        sendForm->setWidget(0, QFormLayout::FieldRole, ipEdit);

        portLabel = new QLabel(sendTab);
        portLabel->setObjectName("portLabel");

        sendForm->setWidget(1, QFormLayout::LabelRole, portLabel);

        portSpin = new QSpinBox(sendTab);
        portSpin->setObjectName("portSpin");
        portSpin->setMinimum(1024);
        portSpin->setMaximum(65535);
        portSpin->setValue(9090);

        sendForm->setWidget(1, QFormLayout::FieldRole, portSpin);

        fileLabel = new QLabel(sendTab);
        fileLabel->setObjectName("fileLabel");

        sendForm->setWidget(2, QFormLayout::LabelRole, fileLabel);

        filePickerLayout = new QHBoxLayout();
        filePickerLayout->setObjectName("filePickerLayout");
        fileEdit = new QLineEdit(sendTab);
        fileEdit->setObjectName("fileEdit");

        filePickerLayout->addWidget(fileEdit);

        browseButton = new QPushButton(sendTab);
        browseButton->setObjectName("browseButton");

        filePickerLayout->addWidget(browseButton);


        sendForm->setLayout(2, QFormLayout::FieldRole, filePickerLayout);

        passLabel = new QLabel(sendTab);
        passLabel->setObjectName("passLabel");

        sendForm->setWidget(3, QFormLayout::LabelRole, passLabel);

        passphraseEdit = new QLineEdit(sendTab);
        passphraseEdit->setObjectName("passphraseEdit");
        passphraseEdit->setEchoMode(QLineEdit::Password);

        sendForm->setWidget(3, QFormLayout::FieldRole, passphraseEdit);


        sendLayout->addLayout(sendForm);

        sendButton = new QPushButton(sendTab);
        sendButton->setObjectName("sendButton");

        sendLayout->addWidget(sendButton);

        sendProgress = new QProgressBar(sendTab);
        sendProgress->setObjectName("sendProgress");
        sendProgress->setValue(0);

        sendLayout->addWidget(sendProgress);

        sendStatus = new QLabel(sendTab);
        sendStatus->setObjectName("sendStatus");
        sendStatus->setAlignment(Qt::AlignCenter);

        sendLayout->addWidget(sendStatus);

        tabWidget->addTab(sendTab, QString());
        receiveTab = new QWidget();
        receiveTab->setObjectName("receiveTab");
        receiveLayout = new QVBoxLayout(receiveTab);
        receiveLayout->setObjectName("receiveLayout");
        receiveForm = new QFormLayout();
        receiveForm->setObjectName("receiveForm");
        recvPortLabel = new QLabel(receiveTab);
        recvPortLabel->setObjectName("recvPortLabel");

        receiveForm->setWidget(0, QFormLayout::LabelRole, recvPortLabel);

        recvPortSpin = new QSpinBox(receiveTab);
        recvPortSpin->setObjectName("recvPortSpin");
        recvPortSpin->setMinimum(1024);
        recvPortSpin->setMaximum(65535);
        recvPortSpin->setValue(9090);

        receiveForm->setWidget(0, QFormLayout::FieldRole, recvPortSpin);

        dirLabel = new QLabel(receiveTab);
        dirLabel->setObjectName("dirLabel");

        receiveForm->setWidget(1, QFormLayout::LabelRole, dirLabel);

        dirPickerLayout = new QHBoxLayout();
        dirPickerLayout->setObjectName("dirPickerLayout");
        dirEdit = new QLineEdit(receiveTab);
        dirEdit->setObjectName("dirEdit");

        dirPickerLayout->addWidget(dirEdit);

        dirBrowseButton = new QPushButton(receiveTab);
        dirBrowseButton->setObjectName("dirBrowseButton");

        dirPickerLayout->addWidget(dirBrowseButton);


        receiveForm->setLayout(1, QFormLayout::FieldRole, dirPickerLayout);

        recvPassLabel = new QLabel(receiveTab);
        recvPassLabel->setObjectName("recvPassLabel");

        receiveForm->setWidget(2, QFormLayout::LabelRole, recvPassLabel);

        recvPassphraseEdit = new QLineEdit(receiveTab);
        recvPassphraseEdit->setObjectName("recvPassphraseEdit");
        recvPassphraseEdit->setEchoMode(QLineEdit::Password);

        receiveForm->setWidget(2, QFormLayout::FieldRole, recvPassphraseEdit);


        receiveLayout->addLayout(receiveForm);

        receiveButton = new QPushButton(receiveTab);
        receiveButton->setObjectName("receiveButton");
        receiveButton->setCheckable(true);

        receiveLayout->addWidget(receiveButton);

        receiveStatus = new QLabel(receiveTab);
        receiveStatus->setObjectName("receiveStatus");
        receiveStatus->setAlignment(Qt::AlignCenter);

        receiveLayout->addWidget(receiveStatus);

        receiveLog = new QTextEdit(receiveTab);
        receiveLog->setObjectName("receiveLog");
        receiveLog->setReadOnly(true);

        receiveLayout->addWidget(receiveLog);

        tabWidget->addTab(receiveTab, QString());

        mainLayout->addWidget(tabWidget);

        MainWindow->setCentralWidget(centralWidget);
        statusBar = new QStatusBar(MainWindow);
        statusBar->setObjectName("statusBar");
        MainWindow->setStatusBar(statusBar);

        retranslateUi(MainWindow);

        tabWidget->setCurrentIndex(0);


        QMetaObject::connectSlotsByName(MainWindow);
    } // setupUi

    void retranslateUi(QMainWindow *MainWindow)
    {
        MainWindow->setWindowTitle(QCoreApplication::translate("MainWindow", "Universal Airdrop", nullptr));
        nameLabel->setText(QCoreApplication::translate("MainWindow", "Your Name:", nullptr));
        nameEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "Enter your device name", nullptr));
        scanButton->setText(QCoreApplication::translate("MainWindow", "Scan", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(discoverTab), QCoreApplication::translate("MainWindow", "Discover", nullptr));
        ipLabel->setText(QCoreApplication::translate("MainWindow", "IP Address:", nullptr));
        ipEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "192.168.1.50", nullptr));
        portLabel->setText(QCoreApplication::translate("MainWindow", "Port:", nullptr));
        fileLabel->setText(QCoreApplication::translate("MainWindow", "File:", nullptr));
        fileEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "Path to file", nullptr));
        browseButton->setText(QCoreApplication::translate("MainWindow", "Browse...", nullptr));
        passLabel->setText(QCoreApplication::translate("MainWindow", "Passphrase:", nullptr));
        passphraseEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "Optional: encrypt with passphrase", nullptr));
        sendButton->setText(QCoreApplication::translate("MainWindow", "Send File", nullptr));
        sendButton->setStyleSheet(QCoreApplication::translate("MainWindow", "QPushButton { background-color: #007AFF; color: white; padding: 8px; font-size: 14px; border-radius: 4px; }\n"
"QPushButton:disabled { background-color: #999; }", nullptr));
        sendStatus->setText(QString());
        tabWidget->setTabText(tabWidget->indexOf(sendTab), QCoreApplication::translate("MainWindow", "Send", nullptr));
        recvPortLabel->setText(QCoreApplication::translate("MainWindow", "Port:", nullptr));
        dirLabel->setText(QCoreApplication::translate("MainWindow", "Save to:", nullptr));
        dirEdit->setText(QCoreApplication::translate("MainWindow", "./received", nullptr));
        dirBrowseButton->setText(QCoreApplication::translate("MainWindow", "Browse...", nullptr));
        recvPassLabel->setText(QCoreApplication::translate("MainWindow", "Passphrase:", nullptr));
        recvPassphraseEdit->setPlaceholderText(QCoreApplication::translate("MainWindow", "Optional: decrypt with passphrase", nullptr));
        receiveButton->setText(QCoreApplication::translate("MainWindow", "Start Receiving", nullptr));
        receiveButton->setStyleSheet(QCoreApplication::translate("MainWindow", "QPushButton { background-color: #34C759; color: white; padding: 8px; font-size: 14px; border-radius: 4px; }\n"
"QPushButton:checked { background-color: #FF3B30; }", nullptr));
        receiveStatus->setText(QCoreApplication::translate("MainWindow", "Not receiving", nullptr));
        tabWidget->setTabText(tabWidget->indexOf(receiveTab), QCoreApplication::translate("MainWindow", "Receive", nullptr));
    } // retranslateUi

};

namespace Ui {
    class MainWindow: public Ui_MainWindow {};
} // namespace Ui

QT_END_NAMESPACE

#endif // UI_MAIN_WINDOW_H
