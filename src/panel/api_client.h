#pragma once

#include <QObject>
#include <QString>

class QTcpSocket;

namespace zibby::panel {

class ApiClient : public QObject {
    Q_OBJECT

public:
    explicit ApiClient(QObject* parent = nullptr);

    void setEndpoint(const QString& endpointHostPort);
    void setToken(const QString& token);

    void connectAndLogin();
    void disconnectFromHost();

    void requestSystemPing();
    void requestProfileGet();
    void requestPeersList();
    void requestMessageHistory(const QString& chatId, int limit);
    void requestMessageSend(const QString& chatId, const QString& from, const QString& to, const QString& text);

signals:
    void statusChanged(const QString& status);
    void rpcResponse(const QString& rawJsonLine);

private:
    void sendJsonLine(const QByteArray& jsonLine);

    QString endpoint_;
    QString token_;
    QTcpSocket* socket_ = nullptr;
    quint64 nextId_ = 1;
    bool authed_ = false;
};

} // namespace zibby::panel
