#include "panel/peer_list_model.h"

#ifdef ZIBBY_HAS_PANEL

namespace zibby::panel {

PeerListModel::PeerListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int PeerListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return peers_.size();
}

QVariant PeerListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }
    const int row = index.row();
    if (row < 0 || row >= peers_.size()) {
        return {};
    }

    const auto& p = peers_.at(row);
    const QString title = p.name.isEmpty() ? p.peerId : p.name;
    const QString subtitle = QString("%1:%2").arg(p.host).arg(p.port);

    switch (role) {
    case Qt::DisplayRole:
    case TitleRole:
        return title;
    case Qt::ToolTipRole:
        return QString("%1\n%2\nlast_seen=%3").arg(p.peerId, p.version, p.lastSeen);
    case SubtitleRole:
        return subtitle;
    case PeerIdRole:
        return p.peerId;
    case NameRole:
        return p.name;
    case HostRole:
        return p.host;
    case PortRole:
        return p.port;
    case VersionRole:
        return p.version;
    case LastSeenRole:
        return p.lastSeen;
    default:
        return {};
    }
}

QHash<int, QByteArray> PeerListModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles[PeerIdRole] = "peerId";
    roles[NameRole] = "name";
    roles[HostRole] = "host";
    roles[PortRole] = "port";
    roles[VersionRole] = "version";
    roles[LastSeenRole] = "lastSeen";
    roles[TitleRole] = "title";
    roles[SubtitleRole] = "subtitle";
    return roles;
}

void PeerListModel::setPeers(QVector<PeerItem> peers) {
    beginResetModel();
    peers_ = std::move(peers);
    endResetModel();
}

const PeerItem* PeerListModel::peerAt(int row) const {
    if (row < 0 || row >= peers_.size()) {
        return nullptr;
    }
    return &peers_.at(row);
}

} // namespace zibby::panel

#endif
