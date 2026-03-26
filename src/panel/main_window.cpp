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
#include <QListWidget>
#include <QMenu>
#include <QMetaObject>
#include <QMessageBox>
#include <QNetworkInterface>
#include <QPlainTextEdit>
#include <QProcess>
#include <QPushButton>
#include <QSystemTrayIcon>
#include <QTabWidget>
#include <QTimer>
#include <QUrl>
#include <QVBoxLayout>
#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>

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
    setWindowIcon(resolveAppIcon(exeDir()));

    exeDir_ = exeDir();

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

    // Messenger tab (fuller UI)
    {
        auto* w = new QWidget(this);
        auto* root = new QHBoxLayout(w);

        // Left: peers/search
        auto* left = new QWidget(w);
        auto* leftLayout = new QVBoxLayout(left);
        peerSearch_ = new QLineEdit(left);
        peerSearch_->setPlaceholderText("Search peers...");
        discoverBtn_ = new QPushButton("Auto-discover", left);
        refreshPeersBtn_ = new QPushButton("Refresh list", left);
        peersList_ = new QListWidget(left);
        peersList_->setSelectionMode(QAbstractItemView::SingleSelection);

        leftLayout->addWidget(peerSearch_);
        leftLayout->addWidget(discoverBtn_);
        leftLayout->addWidget(refreshPeersBtn_);
        leftLayout->addWidget(peersList_, 1);

        // Right: chat
        auto* right = new QWidget(w);
        auto* rightLayout = new QVBoxLayout(right);
        chatTitle_ = new QLabel("Select a peer", right);
        messagesList_ = new QListWidget(right);
        messagesList_->setSelectionMode(QAbstractItemView::NoSelection);
        messagesList_->setWordWrap(true);

        auto* inputRow = new QHBoxLayout();
        messageText_ = new QLineEdit(right);
        messageText_->setPlaceholderText("Write a message...");
        sendBtn_ = new QPushButton("Send", right);
        inputRow->addWidget(messageText_, 1);
        inputRow->addWidget(sendBtn_);

        rightLayout->addWidget(chatTitle_);
        rightLayout->addWidget(messagesList_, 1);
        rightLayout->addLayout(inputRow);

        root->addWidget(left, 0);
        root->addWidget(right, 1);

        tabs->addTab(w, "Messenger");

        connect(discoverBtn_, &QPushButton::clicked, this, [this]() {
            api_->requestPeersDiscover(1200);
        });
        connect(refreshPeersBtn_, &QPushButton::clicked, this, [this]() {
            api_->requestPeersList(200);
        });
        connect(peerSearch_, &QLineEdit::textChanged, this, [this](const QString&) {
            refreshPeersUi();
        });
        connect(peersList_, &QListWidget::currentRowChanged, this, [this](int row) {
            if (row < 0 || row >= peersList_->count()) {
                return;
            }
            const auto peerId = peersList_->item(row)->data(Qt::UserRole).toString();
            openPeerChat(peerId);
        });
        connect(sendBtn_, &QPushButton::clicked, this, [this]() {
            if (currentPeerId_.isEmpty()) {
                QMessageBox::information(this, "Send", "Select a peer first");
                return;
            }
            const QString text = messageText_->text();
            if (text.trimmed().isEmpty()) {
                return;
            }
            const QString chatId = currentPeerId_;
            const QString from = localProfileId_.isEmpty() ? QString("local") : localProfileId_;
            const QString to = currentPeerId_;
            api_->requestMessageSend(chatId, from, to, text);
            messageText_->clear();
            refreshCurrentChatHistory();
        });
        connect(messageText_, &QLineEdit::returnPressed, sendBtn_, &QPushButton::click);
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
    connect(api_, &ApiClient::rpcResponse, this, [this](const QString& line) { handleRpcLine(line); });

    loadConfigAndWire();
    refreshNetworkInfo();
    refreshServiceStatus();
    statusTimer_->start();

    peersTimer_ = new QTimer(this);
    peersTimer_->setInterval(15000);
    connect(peersTimer_, &QTimer::timeout, this, [this]() {
        api_->requestPeersDiscover(900);
        api_->requestPeersList(200);
    });
    peersTimer_->start();
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

    api_->requestProfileGet();
    api_->requestPeersList(200);
}

void MainWindow::handleRpcLine(const QString& rawJsonLine) {
    // Keep lightweight: parse known responses and update UI; unknown lines are ignored.
    const auto doc = QJsonDocument::fromJson(rawJsonLine.toUtf8());
    if (!doc.isObject()) {
        return;
    }
    const auto obj = doc.object();
    if (!obj.value("ok").toBool(false)) {
        return;
    }
    const auto result = obj.value("result");
    if (!result.isObject()) {
        return;
    }
    const auto res = result.toObject();

    // profile.get
    if (res.contains("profile") && res.value("profile").isObject()) {
        const auto profile = res.value("profile").toObject();
        localProfileId_ = profile.value("id").toString();
        return;
    }

    // peers.list
    if (res.contains("peers") && res.value("peers").isArray()) {
        peers_.clear();
        const auto arr = res.value("peers").toArray();
        peers_.reserve(arr.size());
        for (const auto& v : arr) {
            if (!v.isObject()) {
                continue;
            }
            const auto p = v.toObject();
            PeerItem item;
            item.peerId = p.value("peer_id").toString();
            item.name = p.value("name").toString();
            item.host = p.value("host").toString();
            item.port = p.value("port").toInt();
            item.version = p.value("version").toString();
            item.lastSeen = p.value("last_seen").toString();
            if (!item.peerId.isEmpty()) {
                peers_.push_back(item);
            }
        }
        refreshPeersUi();
        return;
    }

    // message.history
    if (res.contains("messages") && res.value("messages").isArray()) {
        if (currentPeerId_.isEmpty()) {
            return;
        }
        messagesList_->clear();
        const auto arr = res.value("messages").toArray();
        for (const auto& v : arr) {
            if (!v.isObject()) {
                continue;
            }
            const auto m = v.toObject();
            const QString from = m.value("from").toString();
            const QString text = m.value("text").toString();
            const QString createdAt = m.value("created_at").toString();

            QString line = text;
            if (!createdAt.isEmpty()) {
                line = QString("%1\n%2").arg(createdAt, text);
            }

            auto* item = new QListWidgetItem(line, messagesList_);
            if (!from.isEmpty() && from == localProfileId_) {
                item->setTextAlignment(Qt::AlignRight);
            } else {
                item->setTextAlignment(Qt::AlignLeft);
            }
        }
        messagesList_->scrollToBottom();
        return;
    }
}

void MainWindow::refreshPeersUi() {
    if (!peersList_) {
        return;
    }
    const QString q = peerSearch_ ? peerSearch_->text().trimmed() : QString();
    peersList_->clear();
    for (const auto& p : peers_) {
        const QString title = (p.name.isEmpty() ? p.peerId : (p.name + " (" + p.peerId + ")"));
        if (!q.isEmpty() && !title.contains(q, Qt::CaseInsensitive) && !p.host.contains(q, Qt::CaseInsensitive)) {
            continue;
        }
        auto* item = new QListWidgetItem(title, peersList_);
        item->setData(Qt::UserRole, p.peerId);
        item->setToolTip(QString("%1:%2\n%3\nlast_seen=%4").arg(p.host).arg(p.port).arg(p.version, p.lastSeen));
    }
}

void MainWindow::openPeerChat(const QString& peerId) {
    currentPeerId_ = peerId;
    if (chatTitle_) {
        chatTitle_->setText(peerId.isEmpty() ? "Select a peer" : ("Chat: " + peerId));
    }
    refreshCurrentChatHistory();
}

void MainWindow::refreshCurrentChatHistory() {
    if (currentPeerId_.isEmpty()) {
        return;
    }
    api_->requestMessageHistory(currentPeerId_, 50);
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
