#include "panel/peer_delegate.h"

#ifdef ZIBBY_HAS_PANEL

#include "panel/peer_list_model.h"

#include <QPainter>
#include <QApplication>

namespace zibby::panel {

PeerDelegate::PeerDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

static QString firstLetter(const QString& s) {
    const auto t = s.trimmed();
    if (t.isEmpty()) {
        return "?";
    }
    return t.left(1).toUpper();
}

void PeerDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    QStyleOptionViewItem opt(option);
    initStyleOption(&opt, index);

    const bool selected = opt.state & QStyle::State_Selected;
    const bool hovered = opt.state & QStyle::State_MouseOver;
    const QRect r = opt.rect;

    QColor bg = opt.palette.color(QPalette::Base);
    if (hovered) {
        bg = opt.palette.color(QPalette::AlternateBase);
    }
    if (selected) {
        bg = opt.palette.color(QPalette::Highlight);
    }
    painter->fillRect(r, bg);

    const QString title = index.data(PeerListModel::TitleRole).toString();
    const QString subtitle = index.data(PeerListModel::SubtitleRole).toString();

    const int pad = 10;
    const int avatar = 32;
    QRect avatarRect(r.left() + pad, r.top() + (r.height() - avatar) / 2, avatar, avatar);

    // Avatar circle
    QColor avatarBg = opt.palette.color(QPalette::Mid);
    avatarBg.setAlphaF(selected ? 1.0 : 0.85);
    painter->setPen(Qt::NoPen);
    painter->setBrush(avatarBg);
    painter->drawEllipse(avatarRect);

    painter->setPen(opt.palette.color(QPalette::Text));
    QFont f = opt.font;
    f.setBold(true);
    painter->setFont(f);
    painter->drawText(avatarRect, Qt::AlignCenter, firstLetter(title));

    // Text
    const int textX = avatarRect.right() + pad;
    QRect titleRect(textX, r.top() + pad - 2, r.width() - textX - pad, 18);
    QRect subRect(textX, r.top() + pad + 16, r.width() - textX - pad, 16);

    painter->setFont(opt.font);
    painter->setPen(opt.palette.color(QPalette::Text));
    painter->drawText(titleRect, Qt::AlignLeft | Qt::AlignVCenter, title);

    QColor subColor = opt.palette.color(QPalette::Text);
    subColor.setAlphaF(0.65);
    painter->setPen(subColor);
    painter->drawText(subRect, Qt::AlignLeft | Qt::AlignVCenter, subtitle);

    painter->restore();
}

QSize PeerDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex&) const {
    // Fixed-height row for clean minimal list.
    return {option.rect.width(), 54};
}

} // namespace zibby::panel

#endif
