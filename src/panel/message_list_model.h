#pragma once

#ifdef ZIBBY_HAS_PANEL

#include <QAbstractListModel>
#include <QString>
#include <QVector>

namespace zibby::panel {

struct MessageItem {
    QString id;
    QString chat;
    QString from;
    QString to;
    QString text;
    QString createdAt;
    bool outgoing = false;
    bool edited = false;
    bool read = false;
};

class MessageListModel final : public QAbstractListModel {
    Q_OBJECT

public:
    enum Roles {
        TextRole = Qt::UserRole + 1,
        OutgoingRole,
        TimeRole,
        FromRole,
        ToRole,
        EditedRole,
        ReadRole,
    };

    explicit MessageListModel(QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    void setMessages(QVector<MessageItem> messages);

private:
    QVector<MessageItem> messages_;
};

} // namespace zibby::panel

#endif
