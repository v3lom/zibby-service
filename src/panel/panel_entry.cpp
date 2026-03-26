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
    pal.setColor(QPalette::Window, QColor(18, 18, 18));
    pal.setColor(QPalette::WindowText, QColor(240, 240, 240));
    pal.setColor(QPalette::Base, QColor(24, 24, 24));
    pal.setColor(QPalette::AlternateBase, QColor(30, 30, 30));
    pal.setColor(QPalette::ToolTipBase, QColor(24, 24, 24));
    pal.setColor(QPalette::ToolTipText, QColor(240, 240, 240));
    pal.setColor(QPalette::Text, QColor(240, 240, 240));
    pal.setColor(QPalette::Button, QColor(34, 34, 34));
    pal.setColor(QPalette::ButtonText, QColor(240, 240, 240));
    pal.setColor(QPalette::BrightText, QColor(255, 80, 80));
    pal.setColor(QPalette::Highlight, QColor(48, 48, 48));
    pal.setColor(QPalette::HighlightedText, QColor(255, 255, 255));
    app.setPalette(pal);

    app.setStyleSheet(
        "QLineEdit, QPlainTextEdit, QListView {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 6px 8px;"
        "  background: palette(base);"
        "}"
        "QPushButton {"
        "  border: 1px solid palette(mid);"
        "  border-radius: 8px;"
        "  padding: 6px 10px;"
        "  background: palette(button);"
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
