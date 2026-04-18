#include "../src/services/screenshotservice.h"

#include <QDir>
#include <QImage>
#include <QTemporaryDir>
#include <QtTest>

class TestScreenshotService : public QObject
{
    Q_OBJECT

private slots:
    void testCapture_savesFileWithCorrectNamePattern()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ScreenshotService service;
        QImage frame(32, 32, QImage::Format_RGB32);
        frame.fill(Qt::blue);

        QString filename = service.capture(frame, tempDir.path());

        QVERIFY(!filename.isEmpty());
        QVERIFY(filename.startsWith("r64u_screenshot_"));
        QVERIFY(filename.endsWith(".png"));

        // Verify the file actually exists on disk
        QVERIFY(QFile::exists(tempDir.filePath(filename)));
    }

    void testCapture_createsDirectoryIfMissing()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        QString subDir = tempDir.path() + "/nested/screenshots";
        QVERIFY(!QDir(subDir).exists());

        ScreenshotService service;
        QImage frame(16, 16, QImage::Format_RGB32);
        frame.fill(Qt::red);

        QString filename = service.capture(frame, subDir);

        QVERIFY(!filename.isEmpty());
        QVERIFY(QDir(subDir).exists());
        QVERIFY(QFile::exists(QDir(subDir).filePath(filename)));
    }

    void testCapture_returnsEmptyStringForNullImage()
    {
        QTemporaryDir tempDir;
        QVERIFY(tempDir.isValid());

        ScreenshotService service;
        QImage nullImage;  // Null image cannot be saved

        QString filename = service.capture(nullImage, tempDir.path());

        QVERIFY(filename.isEmpty());
    }
};

QTEST_MAIN(TestScreenshotService)
#include "test_screenshotservice.moc"
