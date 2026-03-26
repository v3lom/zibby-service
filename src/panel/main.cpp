#include "panel/main_window.h"

#include <QApplication>

int main(int argc, char* argv[]) {
    QApplication app(argc, argv);

    zibby::panel::MainWindow w;
    w.show();

    return app.exec();
}
