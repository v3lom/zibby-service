#include "panel/api_client.h"

#include <QTcpSocket>
#include <QJsonDocument>
#include <QJsonObject>
#include <QHostInfo>

namespace zibby::panel {

namespace {

bool parseHostPort(const QString& endpoint, QString* host, quint16* port) {
    const int sep = endpoint.lastIndexOf(':');
    if (sep <= 0 || sep >= endpoint.size() - 1) {
        return false;
    }
    *host = endpoint.left(sep);
    bool ok = false;
    const int p = endpoint.mid(sep + 1).toInt(&ok);
    if (!ok || p < 1 || p > 65535) {
        return false;
    }
    *port = static_cast<quint16>(p);
    return true;
}

QByteArray toJsonLine(const QJsonObject& obj) {
    QJsonDocument doc(obj);
    QByteArray bytes = doc.toJson(QJsonDocument::Compact);
    bytes.append('\n');
    return bytes;
}

} // namespace

ApiClient::ApiClient(QObject* parent)
    : QObject(parent), socket_(new QTcpSocket(this)) {
    connect(socket_, &QTcpSocket::connected, this, [this]() {
        emit statusChanged("connected");
        authed_ = false;

        if (token_.isEmpty()) {
            emit statusChanged("error: empty token");
            return;
        }

        QJsonObject req;
        req["id"] = QString::number(nextId_++);
        req["method"] = "auth.login";
        QJsonObject params;
        params["token"] = token_;
        req["params"] = params;
        sendJsonLine(toJsonLine(req));
    });

    connect(socket_, &QTcpSocket::readyRead, this, [this]() {
        while (socket_->canReadLine()) {
            const QByteArray line = socket_->readLine();
            const QString s = QString::fromUtf8(line).trimmed();
            if (s.isEmpty()) {
                continue;
            }
            emit rpcResponse(s);

            if (!authed_) {
                const auto doc = QJsonDocument::fromJson(s.toUtf8());
                if (doc.isObject()) {
                    const auto obj = doc.object();
                    const bool ok = obj.value("ok").toBool(false);
                    if (ok) {
                        authed_ = true;
                        emit statusChanged("authed");
                    } else if (obj.contains("error")) {
                        emit statusChanged("auth_failed: " + obj.value("error").toString());
                    }
                }
            }
        }
    });

    connect(socket_, &QTcpSocket::errorOccurred, this, [this](QAbstractSocket::SocketError) {
        emit statusChanged("socket_error: " + socket_->errorString());
        authed_ = false;
    });

    connect(socket_, &QTcpSocket::disconnected, this, [this]() {
        emit statusChanged("disconnected");
        authed_ = false;
    });
}

void ApiClient::setEndpoint(const QString& endpointHostPort) {
    endpoint_ = endpointHostPort;
}

void ApiClient::setToken(const QString& token) {
    token_ = token;
}

void ApiClient::connectAndLogin() {
    QString host;
    quint16 port = 0;
    if (!parseHostPort(endpoint_, &host, &port)) {
        emit statusChanged("error: invalid endpoint");
        return;
    }

    socket_->abort();
    socket_->connectToHost(host, port);
    emit statusChanged("connecting");
}

void ApiClient::disconnectFromHost() {
    socket_->disconnectFromHost();
}

void ApiClient::sendJsonLine(const QByteArray& jsonLine) {
    if (socket_->state() != QAbstractSocket::ConnectedState) {
        emit statusChanged("error: not connected");
        return;
    }
    socket_->write(jsonLine);
}

void ApiClient::requestSystemPing() {
    if (!authed_) {
        emit statusChanged("error: not authed");
        return;
    }
    QJsonObject req;
    req["id"] = QString::number(nextId_++);
    req["method"] = "system.ping";
    sendJsonLine(toJsonLine(req));
}

void ApiClient::requestProfileGet() {
    if (!authed_) {
        emit statusChanged("error: not authed");
        return;
    }
    QJsonObject req;
    req["id"] = QString::number(nextId_++);
    req["method"] = "profile.get";
    sendJsonLine(toJsonLine(req));
}

void ApiClient::requestPeersList(int limit) {
    if (!authed_) {
        emit statusChanged("error: not authed");
        return;
    }
    QJsonObject req;
    req["id"] = QString::number(nextId_++);
    req["method"] = "peers.list";
    QJsonObject params;
    params["limit"] = limit;
    req["params"] = params;
    sendJsonLine(toJsonLine(req));
}

void ApiClient::requestPeersDiscover(int timeoutMs) {
    if (!authed_) {
        emit statusChanged("error: not authed");
        return;
    }
    QJsonObject req;
    req["id"] = QString::number(nextId_++);
    req["method"] = "peers.discover";
    QJsonObject params;
    params["timeout_ms"] = timeoutMs;
    req["params"] = params;
    sendJsonLine(toJsonLine(req));
}

void ApiClient::requestMessageHistory(const QString& chatId, int limit) {
    if (!authed_) {
        emit statusChanged("error: not authed");
        return;
    }
    QJsonObject req;
    req["id"] = QString::number(nextId_++);
    req["method"] = "message.history";
    QJsonObject params;
    params["chat"] = chatId;
    params["limit"] = limit;
    req["params"] = params;
    sendJsonLine(toJsonLine(req));
}

void ApiClient::requestMessageSend(const QString& chatId, const QString& from, const QString& to, const QString& text) {
    if (!authed_) {
        emit statusChanged("error: not authed");
        return;
    }
    QJsonObject req;
    req["id"] = QString::number(nextId_++);
    req["method"] = "message.send";
    QJsonObject params;
    params["chat"] = chatId;
    params["from"] = from;
    params["to"] = to;
    params["text"] = text;
    req["params"] = params;
    sendJsonLine(toJsonLine(req));
}

} // namespace zibby::panel
