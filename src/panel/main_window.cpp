#include "panel/main_window.h"

#include "core/config.h"
#include "core/update_checker.h"
#include "panel/api_client.h"

#include "version.h"

#include <QAction>
#include <QApplication>
#include <QDesktopServices>
#include <QFile>
#include <QFileInfo>
#include <QDir>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QMenu>
#include <QMetaObject>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QTcpSocket>

#include <atomic>
#include <thread>

#ifdef _WIN32
#include <windows.h>
#endif

namespace zibby::panel {

namespace {

QString exeDir() {
    return QFileInfo(QCoreApplication::applicationFilePath()).absolutePath();
}

bool controlRoundtrip(int port, const QByteArray& request, const QByteArray& expectContains) {
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", static_cast<quint16>(port));
    if (!socket.waitForConnected(200)) {
        return false;
    }
    socket.write(request);
    socket.flush();
    if (!socket.waitForReadyRead(200)) {
        return false;
    }
    const QByteArray resp = socket.readAll();
    return resp.contains(expectContains);
}

bool pingControlPort(int port) {
    return controlRoundtrip(port, "PING\n", "PONG");
}

bool sendControlCommand(int port, const QByteArray& cmd) {
    QTcpSocket socket;
    socket.connectToHost("127.0.0.1", static_cast<quint16>(port));
    if (!socket.waitForConnected(200)) {
        return false;
    }
    socket.write(cmd);
    socket.flush();
    socket.waitForBytesWritten(200);
    return true;
}

QIcon resolveAppIcon(const QString& exeDir) {
    const QStringList candidates = {
        QDir(exeDir).filePath("zibby-icon.ico"),
        QDir(exeDir).filePath("..\\share\\zibby\\zibby-icon.ico"),
        QDir(exeDir).filePath("..\\share\\zibby\\zibby-icon.png"),
    };
    for (const auto& c : candidates) {
        if (QFileInfo::exists(c)) {
            return QIcon(c);
        }
    }
    return QApplication::windowIcon();
}

} // namespace

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent), api_(new ApiClient(this)) {
    setWindowTitle("zibby-service (beta panel)");
    setWindowIcon(resolveAppIcon(::exeDir()));

    exeDir_ = ::exeDir();

    auto* tabs = new QTabWidget(this);
    setCentralWidget(tabs);

    // Status tab
    {
        auto* w = new QWidget(this);
        auto* layout = new QVBoxLayout(w);

        versionLabel_ = new QLabel(w);
        developerLabel_ = new QLabel(w);
        serviceStatus_ = new QLabel(w);

        startBtn_ = new QPushButton("Start service", w);
        stopBtn_ = new QPushButton("Stop service", w);
        pingBtn_ = new QPushButton("Ping API", w);

        auto* buttons = new QHBoxLayout();
        buttons->addWidget(startBtn_);
        buttons->addWidget(stopBtn_);
        buttons->addWidget(pingBtn_);
        buttons->addStretch(1);

        layout->addWidget(versionLabel_);
        layout->addWidget(developerLabel_);
        layout->addWidget(serviceStatus_);
        layout->addLayout(buttons);

        auto* openData = new QPushButton("Open data folder", w);
        auto* openLogs = new QPushButton("Open log file", w);
        layout->addWidget(openData);
        layout->addWidget(openLogs);
        layout->addStretch(1);

        tabs->addTab(w, "Status");

        connect(openData, &QPushButton::clicked, this, [this]() {
            if (!dataDir_.isEmpty()) {
                QDesktopServices::openUrl(QUrl::fromLocalFile(dataDir_));
            }
        });

        connect(openLogs, &QPushButton::clicked, this, [this]() {
            if (!dataDir_.isEmpty()) {
                const QString logPath = QDir(dataDir_).filePath("zibby.log");
                QDesktopServices::openUrl(QUrl::fromLocalFile(logPath));
            }
        });
    }

    // Updates tab
    {
        auto* w = new QWidget(this);
        auto* layout = new QVBoxLayout(w);

        updatesLabel_ = new QLabel("Updates: unknown", w);
        updatesButton_ = new QPushButton("Check updates", w);

        layout->addWidget(updatesLabel_);
        layout->addWidget(updatesButton_);
        layout->addStretch(1);

        tabs->addTab(w, "Updates");

        connect(updatesButton_, &QPushButton::clicked, this, [this]() { checkUpdates(); });
    }

    // Network tab
    {
        auto* w = new QWidget(this);
        auto* layout = new QVBoxLayout(w);
        networkText_ = new QPlainTextEdit(w);
        networkText_->setReadOnly(true);

        auto* refresh = new QPushButton("Refresh", w);
        layout->addWidget(refresh);
        layout->addWidget(networkText_);
        tabs->addTab(w, "Network");

        connect(refresh, &QPushButton::clicked, this, [this]() { refreshNetworkInfo(); });
    }

    // Messenger tab (minimal)
    {
        auto* w = new QWidget(this);
        auto* layout = new QVBoxLayout(w);

        apiLog_ = new QPlainTextEdit(w);
        apiLog_->setReadOnly(true);

        chatId_ = new QLineEdit(w);
        chatId_->setPlaceholderText("chat id");
        fromId_ = new QLineEdit(w);
        fromId_->setPlaceholderText("from");
        toId_ = new QLineEdit(w);
        toId_->setPlaceholderText("to");
        messageText_ = new QLineEdit(w);
        messageText_->setPlaceholderText("message text");

        auto* row1 = new QHBoxLayout();
        row1->addWidget(chatId_);
        auto* historyBtn = new QPushButton("History", w);
        row1->addWidget(historyBtn);

        auto* row2 = new QHBoxLayout();
        row2->addWidget(fromId_);
        row2->addWidget(toId_);

        auto* row3 = new QHBoxLayout();
        row3->addWidget(messageText_);
        auto* sendBtn = new QPushButton("Send", w);
        row3->addWidget(sendBtn);

        layout->addLayout(row1);
        layout->addLayout(row2);
        layout->addLayout(row3);
        layout->addWidget(apiLog_);

        tabs->addTab(w, "Messenger");

        connect(historyBtn, &QPushButton::clicked, this, [this]() {
            api_->requestMessageHistory(chatId_->text().trimmed(), 50);
        });
        connect(sendBtn, &QPushButton::clicked, this, [this]() {
            api_->requestMessageSend(chatId_->text().trimmed(), fromId_->text().trimmed(), toId_->text().trimmed(), messageText_->text());
        });
    }

    // About tab
    {
        auto* w = new QWidget(this);
        auto* layout = new QVBoxLayout(w);

        auto* about = new QLabel(w);
        about->setTextInteractionFlags(Qt::TextBrowserInteraction);
        about->setOpenExternalLinks(true);
        about->setText(
            "<b>zibby-service</b><br/>"
            "Developer: v3lom<br/>"
            "Repo: <a href=\"https://github.com/v3lom/zibby-service\">github.com/v3lom/zibby-service</a><br/>");

        layout->addWidget(about);
        layout->addStretch(1);
        tabs->addTab(w, "About");
    }

    // Tray
    tray_ = new QSystemTrayIcon(resolveAppIcon(exeDir_), this);
    tray_->setToolTip("zibby-service");
    auto* menu = new QMenu(this);
    auto* actShow = new QAction("Show", menu);
    auto* actQuit = new QAction("Quit", menu);
    menu->addAction(actShow);
    menu->addAction(actQuit);
    tray_->setContextMenu(menu);
    tray_->show();

    connect(actShow, &QAction::triggered, this, [this]() {
        showNormal();
        raise();
        activateWindow();
    });
    connect(actQuit, &QAction::triggered, qApp, &QApplication::quit);
    connect(tray_, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            showNormal();
            raise();
            activateWindow();
        }
    });

    statusTimer_ = new QTimer(this);
    statusTimer_->setInterval(1000);
    connect(statusTimer_, &QTimer::timeout, this, [this]() { refreshServiceStatus(); });

    connect(startBtn_, &QPushButton::clicked, this, [this]() { startService(); });
    connect(stopBtn_, &QPushButton::clicked, this, [this]() { stopService(); });
    connect(pingBtn_, &QPushButton::clicked, this, [this]() { api_->requestSystemPing(); });

    connect(api_, &ApiClient::statusChanged, this, [this](const QString& s) {
        apiLog_->appendPlainText("[api] " + s);
    });
    connect(api_, &ApiClient::rpcResponse, this, [this](const QString& line) {
        apiLog_->appendPlainText(line);
    });

    loadConfigAndWire();
    refreshNetworkInfo();
    refreshServiceStatus();
    statusTimer_->start();
}

QString MainWindow::readApiTokenFile(const QString& dataDir) const {
    QFile f(QDir(dataDir).filePath("api_token.txt"));
    if (!f.open(QIODevice::ReadOnly | QIODevice::Text)) {
        return {};
    }
    const QByteArray line = f.readLine().trimmed();
    return QString::fromUtf8(line);
}

void MainWindow::loadConfigAndWire() {
    zibby::core::ConfigManager cfg;
    const auto c = cfg.loadOrCreate();

    dataDir_ = QString::fromStdString(c.dataDir);
    controlPort_ = c.controlPort;
    apiEndpoint_ = QString::fromStdString("127.0.0.1:" + std::to_string(c.apiPort));
    apiToken_ = readApiTokenFile(dataDir_);

    versionLabel_->setText(QString("Version: %1").arg(QString::fromUtf8(ZIBBY_VERSION_STRING)));
    developerLabel_->setText("Developer: v3lom | Repo: https://github.com/v3lom/zibby-service");

    api_->setEndpoint(apiEndpoint_);
    api_->setToken(apiToken_);
    api_->connectAndLogin();
}

void MainWindow::refreshServiceStatus() {
    std::thread([this]() {
        const bool ok = pingControlPort(controlPort_);
        QMetaObject::invokeMethod(this, [this, ok]() {
            if (ok) {
                serviceStatus_->setText("Service: running");
            } else {
                serviceStatus_->setText("Service: NOT running (you can Start or Close)");
            }
        });
    }).detach();
}

void MainWindow::startService() {
    const QString exe = QDir(exeDir_).filePath("zibby-service.exe");
    if (!QFileInfo::exists(exe)) {
        QMessageBox::critical(this, "Start", "zibby-service.exe not found next to panel");
        return;
    }
    QStringList args;
    args << "--daemon";

    bool started = QProcess::startDetached(exe, args, exeDir_);
    if (!started) {
        QMessageBox::warning(this, "Start", "Unable to start service");
        return;
    }
}

void MainWindow::stopService() {
    if (!sendControlCommand(controlPort_, "STOP\n")) {
        QMessageBox::warning(this, "Stop", "Unable to send STOP to service");
        return;
    }
}

void MainWindow::checkUpdates() {
    updatesLabel_->setText("Updates: checking...");

    std::thread([this]() {
        const auto r = zibby::core::UpdateChecker::checkLatestRelease(
            "https://github.com/v3lom/zibby-service",
            ZIBBY_VERSION_STRING);

        QMetaObject::invokeMethod(this, [this, r]() {
            if (!r.ok) {
                updatesLabel_->setText("Updates: error: " + QString::fromStdString(r.error));
                return;
            }
            if (r.updateAvailable) {
                updatesLabel_->setText("Updates: available: " + QString::fromStdString(r.latestVersion));
            } else {
                updatesLabel_->setText("Updates: up to date (latest: " + QString::fromStdString(r.latestVersion) + ")");
            }
        });
    }).detach();
}

void MainWindow::refreshNetworkInfo() {
    QString out;
    const auto ifaces = QNetworkInterface::allInterfaces();
    for (const auto& iface : ifaces) {
        out += QString("%1 | %2\n").arg(iface.humanReadableName(), iface.hardwareAddress());
        const auto entries = iface.addressEntries();
        for (const auto& e : entries) {
            out += QString("  ip=%1 mask=%2\n").arg(e.ip().toString(), e.netmask().toString());
        }
        out += "\n";
    }
    networkText_->setPlainText(out);
}

} // namespace zibby::panel
