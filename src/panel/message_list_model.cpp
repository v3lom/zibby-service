#include "panel/message_list_model.h"

#ifdef ZIBBY_HAS_PANEL

#include <QDateTime>

namespace zibby::panel {

namespace {

QString formatTimeCompact(const QString& createdAt) {
    const auto s = createdAt.trimmed();
    if (s.isEmpty()) {
        return {};
    }

    // Try ISO 8601 first.
    QDateTime dt = QDateTime::fromString(s, Qt::ISODateWithMs);
    if (!dt.isValid()) {
        dt = QDateTime::fromString(s, Qt::ISODate);
    }
    if (dt.isValid()) {
        if (dt.timeSpec() == Qt::UTC) {
            dt = dt.toLocalTime();
        }
        return dt.time().toString("HH:mm");
    }

    // Fallback: keep only the last 5 chars if it looks like HH:MM.
    if (s.size() >= 5) {
        const QString tail = s.right(5);
        if (tail.size() == 5 && tail.at(2) == ':') {
            return tail;
        }
    }
    return s;
}

} // namespace

MessageListModel::MessageListModel(QObject* parent)
    : QAbstractListModel(parent) {}

int MessageListModel::rowCount(const QModelIndex& parent) const {
    if (parent.isValid()) {
        return 0;
    }
    return messages_.size();
}

QVariant MessageListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid()) {
        return {};
    }
    const int row = index.row();
    if (row < 0 || row >= messages_.size()) {
        return {};
    }

    const auto& m = messages_.at(row);
    switch (role) {
    case Qt::DisplayRole:
    case TextRole:
        return m.text;
    case OutgoingRole:
        return m.outgoing;
    case TimeRole:
        return formatTimeCompact(m.createdAt);
    case FromRole:
        return m.from;
    case ToRole:
        return m.to;
    case EditedRole:
        return m.edited;
    case ReadRole:
        return m.read;
    default:
        return {};
    }
}

QHash<int, QByteArray> MessageListModel::roleNames() const {
    auto roles = QAbstractListModel::roleNames();
    roles[TextRole] = "text";
    roles[OutgoingRole] = "outgoing";
    roles[TimeRole] = "time";
    roles[FromRole] = "from";
    roles[ToRole] = "to";
    roles[EditedRole] = "edited";
    roles[ReadRole] = "read";
    return roles;
}

void MessageListModel::setMessages(QVector<MessageItem> messages) {
    beginResetModel();
    messages_ = std::move(messages);
    endResetModel();
}

} // namespace zibby::panel

#endif
