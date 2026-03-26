#include "panel/panel_entry.h"

#ifdef ZIBBY_HAS_PANEL

#include "panel/main_window.h"

#include <QApplication>

namespace zibby::panel {

int runPanel(int argc, char** argv) {
    // Note: QApplication must be created in the thread that runs the event loop.
    QApplication app(argc, argv);
    QApplication::setApplicationName("zibby-service");

    MainWindow w;
    w.show();

    return app.exec();
}

} // namespace zibby::panel

#endif
