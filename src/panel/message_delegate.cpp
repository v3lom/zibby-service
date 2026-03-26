#include "panel/message_delegate.h"

#ifdef ZIBBY_HAS_PANEL

#include "panel/message_list_model.h"

#include <QPainter>
#include <QTextOption>

namespace zibby::panel {

MessageDelegate::MessageDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

static QRect bubbleRectFor(const QRect& r, int bubbleWidth, bool outgoing) {
    const int pad = 10;
    const int top = r.top() + 6;
    const int bottom = r.bottom() - 6;
    const int h = bottom - top;

    if (outgoing) {
        return QRect(r.right() - pad - bubbleWidth, top, bubbleWidth, h);
    }
    return QRect(r.left() + pad, top, bubbleWidth, h);
}

void MessageDelegate::paint(QPainter* painter, const QStyleOptionViewItem& option, const QModelIndex& index) const {
    painter->save();
    painter->setRenderHint(QPainter::Antialiasing, true);

    const QString text = index.data(MessageListModel::TextRole).toString();
    const QString time = index.data(MessageListModel::TimeRole).toString();
    const bool outgoing = index.data(MessageListModel::OutgoingRole).toBool();

    const QRect r = option.rect;
    const int pad = 12;
    const int maxBubble = static_cast<int>(r.width() * 0.72);

    QFontMetrics fm(option.font);
    const int textWidth = qMin(maxBubble - pad * 2, qMax(80, fm.horizontalAdvance(text)));

    QTextOption to;
    to.setWrapMode(QTextOption::WordWrap);

    QRect textRect(0, 0, textWidth, 10);
    textRect = fm.boundingRect(textRect, Qt::TextWordWrap, text);

    const int footerH = 16;
    const int bubbleW = qMin(maxBubble, qMax(120, textRect.width() + pad * 2));
    const int bubbleH = textRect.height() + pad * 2 + footerH;

    QRect bubble = bubbleRectFor(r, bubbleW, outgoing);
    bubble.setHeight(bubbleH);

    // Colors: use palette roles (minimal hard-coded styling)
    QColor bubbleBg = outgoing ? option.palette.color(QPalette::Button) : option.palette.color(QPalette::AlternateBase);
    QColor textColor = option.palette.color(QPalette::Text);

    painter->setPen(Qt::NoPen);
    painter->setBrush(bubbleBg);
    painter->drawRoundedRect(bubble, 10, 10);

    painter->setPen(textColor);
    QRect inner = bubble.adjusted(pad, pad, -pad, -pad - footerH);
    painter->drawText(inner, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);

    // Footer time
    QColor timeColor = textColor;
    timeColor.setAlphaF(0.6);
    painter->setPen(timeColor);
    QRect footer = bubble.adjusted(pad, bubble.height() - footerH - 2, -pad, -2);
    painter->drawText(footer, Qt::AlignRight | Qt::AlignVCenter, time);

    painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const QString text = index.data(MessageListModel::TextRole).toString();
    const QRect r = option.rect;
    const int width = r.width() > 0 ? r.width() : 480;
    const int maxBubble = static_cast<int>(width * 0.72);
    const int pad = 12;

    QFontMetrics fm(option.font);
    QRect textRect(0, 0, maxBubble - pad * 2, 10);
    textRect = fm.boundingRect(textRect, Qt::TextWordWrap, text);

    const int footerH = 16;
    return {width, textRect.height() + pad * 2 + footerH + 12};
}

} // namespace zibby::panel

#endif
