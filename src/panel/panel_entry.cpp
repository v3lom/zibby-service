#include "panel/panel_entry.h"

#ifdef ZIBBY_HAS_PANEL

#include "panel/main_window.h"

#include <QApplication>
#include <QColor>
#include <QPalette>

namespace zibby::panel {

int runPanel(int argc, char** argv) {
    // Note: QApplication must be created in the thread that runs the event loop.
    QApplication app(argc, argv);
    QApplication::setApplicationName("zibby-service");

    // Premium-like dark minimal theme (Telegram-ish) using standard Qt roles.
    QApplication::setStyle("Fusion");

    QPalette pal;
    pal.setColor(QPalette::Window, QColor(16, 16, 16));
    pal.setColor(QPalette::WindowText, QColor(242, 242, 242));
    pal.setColor(QPalette::Base, QColor(22, 22, 22));
    pal.setColor(QPalette::AlternateBase, QColor(28, 28, 28));
    pal.setColor(QPalette::ToolTipBase, QColor(22, 22, 22));
    pal.setColor(QPalette::ToolTipText, QColor(242, 242, 242));
    pal.setColor(QPalette::Text, QColor(242, 242, 242));
    pal.setColor(QPalette::Button, QColor(32, 32, 32));
    pal.setColor(QPalette::ButtonText, QColor(242, 242, 242));
    pal.setColor(QPalette::BrightText, QColor(255, 80, 80));
    pal.setColor(QPalette::Highlight, QColor(44, 44, 44));
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(pal);

    app.setStyleSheet(
        "QLineEdit, QPlainTextEdit {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 6px 8px;"
        "  background: palette(base);"
        "  selection-background-color: palette(highlight);"
        "}"
        "QListView {"
        "  border: none;"
        "  background: palette(base);"
        "  outline: none;"
        "}"
        "QListView::item {"
        "  border-radius: 10px;"
        "}"
        "QScrollBar:vertical {"
        "  background: transparent;"
        "  width: 10px;"
        "  margin: 0px;"
        "}"
        "QScrollBar::handle:vertical {"
        "  background: palette(mid);"
        "  border-radius: 5px;"
        "  min-height: 24px;"
        "}"
        "QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {"
        "  height: 0px;"
        "}"
        "QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {"
        "  background: transparent;"
        "}"
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 6px 10px;"
        "  background: palette(button);"
        "}"
        "QPushButton:hover {"
        "  border-color: palette(light);"
        "}"
        "QPushButton:pressed {"
        "  background: palette(dark);"
        "}"
        "QTabWidget::pane {"
        "  border: 1px solid palette(mid);"
        "  top: -1px;"
        "}"
        "QToolTip {"
        "  color: palette(text);"
        "  background-color: palette(base);"
        "  border: 1px solid palette(mid);"
        "}"
    );

    MainWindow w;
    w.show();

    return app.exec();
}

} // namespace zibby::panel

#endif
