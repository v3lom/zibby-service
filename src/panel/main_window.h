#pragma once

#include <QMainWindow>

class QLabel;
class QLineEdit;
class QPlainTextEdit;
class QPushButton;
class QSystemTrayIcon;
class QTimer;

namespace zibby::panel {
class ApiClient;
}

namespace zibby::panel {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);

private:
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
    QLineEdit* chatId_ = nullptr;
    QLineEdit* fromId_ = nullptr;
    QLineEdit* toId_ = nullptr;
    QLineEdit* messageText_ = nullptr;

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
