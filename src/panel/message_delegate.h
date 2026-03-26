#pragma once

#ifdef ZIBBY_HAS_PANEL

#include <QStyledItemDelegate>

namespace zibby::panel {

class MessageDelegate final : public QStyledItemDelegate {
    Q_OBJECT

public:
    explicit MessageDelegate(QObject* parent = nullptr);

    void paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const override;
    QSize sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const override;
};

} // namespace zibby::panel

#endif
