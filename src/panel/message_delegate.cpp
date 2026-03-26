#include "panel/message_delegate.h"

#ifdef ZIBBY_HAS_PANEL

#include "panel/message_list_model.h"

#include <QPainter>
#include <QTextOption>

namespace zibby::panel {

MessageDelegate::MessageDelegate(QObject* parent)
    : QStyledItemDelegate(parent) {}

static QColor mixColors(const QColor& a, const QColor& b, qreal t) {
    const qreal u = 1.0 - t;
    return QColor(
        static_cast<int>(a.red() * u + b.red() * t),
        static_cast<int>(a.green() * u + b.green() * t),
        static_cast<int>(a.blue() * u + b.blue() * t),
        static_cast<int>(a.alpha() * u + b.alpha() * t));
}

static QColor bubbleBackground(const QPalette& pal, bool outgoing) {
    const QColor base = pal.color(QPalette::Base);
    const QColor alt = pal.color(QPalette::AlternateBase);
    const QColor button = pal.color(QPalette::Button);
    if (outgoing) {
        // Slightly more "solid" bubble for outgoing.
        return mixColors(button, base, 0.25);
    }
    return mixColors(alt, base, 0.15);
}

static QColor bubbleBorder(const QPalette& pal) {
    QColor c = pal.color(QPalette::Mid);
    c.setAlphaF(0.55);
    return c;
}

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
    const bool edited = index.data(MessageListModel::EditedRole).toBool();
    const bool read = index.data(MessageListModel::ReadRole).toBool();

    const QRect r = option.rect;
    const int pad = 12;
    const int maxBubble = static_cast<int>(r.width() * 0.74);

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
    const QColor bubbleBg = bubbleBackground(option.palette, outgoing);
    const QColor textColor = option.palette.color(QPalette::Text);

    painter->setPen(QPen(bubbleBorder(option.palette), 1));
    painter->setBrush(bubbleBg);
    painter->drawRoundedRect(bubble, 12, 12);

    painter->setPen(textColor);
    QRect inner = bubble.adjusted(pad, pad, -pad, -pad - footerH);
    painter->drawText(inner, Qt::TextWordWrap | Qt::AlignLeft | Qt::AlignTop, text);

    // Footer meta (time + status)
    QColor timeColor = textColor;
    timeColor.setAlphaF(0.6);
    painter->setPen(timeColor);
    QRect footer = bubble.adjusted(pad, bubble.height() - footerH - 2, -pad, -2);

    QString meta = time;
    if (edited) {
        if (!meta.isEmpty()) {
            meta += " · ";
        }
        meta += "edited";
    }
    if (outgoing) {
        const QString ticks = read ? QString::fromUtf8("✓✓") : QString::fromUtf8("✓");
        if (!meta.isEmpty()) {
            meta += "  ";
        }
        meta += ticks;
    }
    painter->drawText(footer, Qt::AlignRight | Qt::AlignVCenter, meta);

    painter->restore();
}

QSize MessageDelegate::sizeHint(const QStyleOptionViewItem& option, const QModelIndex& index) const {
    const QString text = index.data(MessageListModel::TextRole).toString();
    const QRect r = option.rect;
    const int width = r.width() > 0 ? r.width() : 480;
    const int maxBubble = static_cast<int>(width * 0.74);
    const int pad = 12;

    QFontMetrics fm(option.font);
    QRect textRect(0, 0, maxBubble - pad * 2, 10);
    textRect = fm.boundingRect(textRect, Qt::TextWordWrap, text);

    const int footerH = 16;
    return {width, textRect.height() + pad * 2 + footerH + 12};
}

} // namespace zibby::panel

#endif
