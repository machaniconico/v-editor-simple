#include <QApplication>
#include "MainWindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName("V Editor Simple");
    app.setApplicationVersion(APP_VERSION);

    MainWindow window;
    window.show();

    return app.exec();
}
