#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
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

    // Load C64 font from resources
    int fontId = QFontDatabase::addApplicationFont(":/fonts/C64_Pro_Mono.ttf");
    if (fontId == -1) {
        qWarning() << "Failed to load C64 Pro Mono font";
    } else if (r64u::verboseLogging) {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (!families.isEmpty()) {
            qDebug() << "Loaded C64 font:" << families.first();
        }
    }

    MainWindow window;
    window.show();

    return app.exec();
}
