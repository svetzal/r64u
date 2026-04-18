#include "screenshotservice.h"

#include <QDateTime>
#include <QDir>
#include <QImage>

ScreenshotService::ScreenshotService(QObject *parent) : QObject(parent) {}

QString ScreenshotService::capture(const QImage &frame, const QString &outputDir)
{
    QDir dir(outputDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString filename = QString("r64u_screenshot_%1.png").arg(timestamp);
    QString filePath = dir.filePath(filename);

    if (!frame.save(filePath, "PNG")) {
        return {};
    }

    return filename;
}
