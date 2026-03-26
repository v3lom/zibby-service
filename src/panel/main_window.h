#pragma once

#include <QMainWindow>
#include <QString>
#include <QVector>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QListView;
class QSortFilterProxyModel;
class QSystemTrayIcon;
class QTimer;

namespace zibby::panel {
class ApiClient;
class PeerListModel;
class MessageListModel;
}

namespace zibby::panel {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
    void handleRpcLine(const QString& rawJsonLine);
    void refreshPeersUi();
    void openPeerChat(const QString& peerId);
    void refreshCurrentChatHistory();
    void loadConfigAndWire();
    void refreshServiceStatus();
    void startService();
    void stopService();
    void checkUpdates();
    void refreshNetworkInfo();

    QString readApiTokenFile(const QString& dataDir) const;

    QLabel* serviceStatus_ = nullptr;
    QLabel* versionLabel_ = nullptr;
    QLabel* developerLabel_ = nullptr;

    QLabel* updatesLabel_ = nullptr;
    QPushButton* updatesButton_ = nullptr;

    QPlainTextEdit* networkText_ = nullptr;

    QPlainTextEdit* apiLog_ = nullptr;
    // Messenger UI
    QLineEdit* peerSearch_ = nullptr;
    QPushButton* discoverBtn_ = nullptr;
    QPushButton* refreshPeersBtn_ = nullptr;
    QListView* peersView_ = nullptr;
    PeerListModel* peersModel_ = nullptr;
    QSortFilterProxyModel* peersProxy_ = nullptr;
    QLabel* chatTitle_ = nullptr;
    QListView* messagesView_ = nullptr;
    MessageListModel* messagesModel_ = nullptr;
    QLineEdit* messageText_ = nullptr;
    QPushButton* sendBtn_ = nullptr;
    QTimer* peersTimer_ = nullptr;
    QString localProfileId_;
    QString currentPeerId_;

    QPushButton* startBtn_ = nullptr;
    QPushButton* stopBtn_ = nullptr;
    QPushButton* pingBtn_ = nullptr;

    QSystemTrayIcon* tray_ = nullptr;
    QTimer* statusTimer_ = nullptr;

    ApiClient* api_ = nullptr;

    QString exeDir_;
    QString dataDir_;
    QString apiEndpoint_;
    QString apiToken_;
    int controlPort_ = 9877;
};

} // namespace zibby::panel
