#include <QApplication>
#include <QCommandLineParser>
#include <QFontDatabase>
#include "mainwindow.h"
#include "utils/logging.h"
#include "utils/thememanager.h"
#include "version.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    app.setApplicationName("r64u");
    app.setApplicationVersion(R64U_VERSION);
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
        qWarning() << "Failed to load C64 Pro Mono font from resources";
    } else {
        QStringList families = QFontDatabase::applicationFontFamilies(fontId);
        if (families.isEmpty()) {
            qWarning() << "C64 font loaded but no family names returned";
        } else {
            // Always log the font family name so we can verify it matches our usage
            qDebug() << "Loaded C64 font, family name:" << families.first();
            if (families.first() != "C64 Pro Mono") {
                qWarning() << "C64 font family name mismatch! Expected 'C64 Pro Mono', got:" << families.first();
            }
        }
    }

    // Initialize and apply theme
    ThemeManager::instance()->applyTheme();

    MainWindow window;
    window.show();

    return app.exec();
}
