#include <QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("r64u");
    app.setApplicationVersion("0.1.0");
    app.setOrganizationName("r64u");
    app.setOrganizationDomain("example.com");

    MainWindow window;
    window.show();

    return app.exec();
}
