#include <QApplication>
#include <QCommandLineParser>
#include "mainwindow.h"
#include "utils/logging.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("r64u");
    app.setApplicationVersion("0.3.0");
    app.setOrganizationName("r64u");
    app.setOrganizationDomain("example.com");

    // Parse command line arguments
    QCommandLineParser parser;
    parser.setApplicationDescription("Remote access tool for Commodore 64 Ultimate devices");
    parser.addHelpOption();
    parser.addVersionOption();

    QCommandLineOption verboseOption(
        QStringList() << "V" << "verbose",
        "Enable verbose logging output");
    parser.addOption(verboseOption);

    parser.process(app);

    // Set verbose logging flag
    r64u::verboseLogging = parser.isSet(verboseOption);

    if (r64u::verboseLogging) {
        qDebug() << "Verbose logging enabled";
    }

    MainWindow window;
    window.show();

    return app.exec();
}
