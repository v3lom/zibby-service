#pragma once

#ifdef ZIBBY_HAS_PANEL

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace zibby::panel {

struct PeerItem {
    QString peerId;
    QString name;
    QString host;
    int port = 0;
    QString version;
    QString lastSeen;
};

class PeerListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        PeerIdRole = Qt::UserRole + 1,
        NameRole,
        HostRole,
        PortRole,
        VersionRole,
        LastSeenRole,
        TitleRole,
        SubtitleRole,
    };

    explicit PeerListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setPeers(QVector<PeerItem> peers);
    const PeerItem* peerAt(int row) const;

private:
    QVector<PeerItem> peers_;
};

} // namespace zibby::panel

#endif
