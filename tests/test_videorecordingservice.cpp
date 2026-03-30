/**
 * @file test_videorecordingservice.cpp
 * @brief Unit tests for VideoRecordingService.
 *
 * Tests cover:
 * - Initial state (not recording, empty path, zero frame count)
 * - startRecording() creates a file and emits recordingStarted
 * - stopRecording() finalizes the file and emits recordingStopped
 * - addFrame() increments frameCount
 * - addAudioSamples() accepted while recording
 * - Starting while already recording emits error
 * - Output file has non-zero size after stop
 * - RIFF/AVI header magic bytes in the output file
 * - Invalid path emits error signal
 */

#include "services/videorecordingservice.h"

#include <QFile>
#include <QImage>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestVideoRecordingService : public QObject
{
    Q_OBJECT

private:
    // Helper: build a minimal QImage test frame
    static QImage makeTestFrame(int width = 384, int height = 272)
    {
        QImage img(width, height, QImage::Format_RGB32);
        img.fill(Qt::blue);
        return img;
    }

    // Helper: build a one-packet worth of silence (192 stereo samples)
    static QByteArray makeSilence(int sampleCount = 192)
    {
        return QByteArray(static_cast<qsizetype>(sampleCount) * 4,
                          '\0');  // 4 bytes per stereo sample
    }

private slots:
    void init() { service_ = new VideoRecordingService(this); }

    void cleanup()
    {
        if (service_->isRecording()) {
            service_->stopRecording();
        }
        delete service_;
        service_ = nullptr;
    }

    // =========================================================
    // Initial state
    // =========================================================

    void testInitialState_NotRecording() { QVERIFY(!service_->isRecording()); }

    void testInitialState_EmptyPath() { QVERIFY(service_->recordingPath().isEmpty()); }

    void testInitialState_ZeroFrameCount() { QCOMPARE(service_->frameCount(), 0); }

    // =========================================================
    // startRecording
    // =========================================================

    void testStartRecording_ReturnsTrue()
    {
        QTemporaryDir dir;
        QVERIFY(dir.isValid());
        QString path = dir.filePath("test.avi");

        bool result = service_->startRecording(path);

        QVERIFY(result);
    }

    void testStartRecording_SetsIsRecording()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("test.avi"));

        QVERIFY(service_->isRecording());
    }

    void testStartRecording_SetsRecordingPath()
    {
        QTemporaryDir dir;
        QString path = dir.filePath("test.avi");

        service_->startRecording(path);

        QCOMPARE(service_->recordingPath(), path);
    }

    void testStartRecording_EmitsRecordingStarted()
    {
        QTemporaryDir dir;
        QString path = dir.filePath("test.avi");
        QSignalSpy spy(service_, &VideoRecordingService::recordingStarted);

        service_->startRecording(path);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), path);
    }

    void testStartRecording_WhileAlreadyRecording_EmitsError()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("first.avi"));

        QSignalSpy errorSpy(service_, &VideoRecordingService::error);
        bool result = service_->startRecording(dir.filePath("second.avi"));

        QVERIFY(!result);
        QCOMPARE(errorSpy.count(), 1);
    }

    void testStartRecording_InvalidPath_EmitsError()
    {
        QSignalSpy errorSpy(service_, &VideoRecordingService::error);

        bool result = service_->startRecording("/nonexistent_dir_xyz/test.avi");

        QVERIFY(!result);
        QCOMPARE(errorSpy.count(), 1);
    }

    // =========================================================
    // stopRecording
    // =========================================================

    void testStopRecording_ReturnsTrue()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("test.avi"));

        bool result = service_->stopRecording();

        QVERIFY(result);
    }

    void testStopRecording_ClearsIsRecording()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("test.avi"));
        service_->stopRecording();

        QVERIFY(!service_->isRecording());
    }

    void testStopRecording_EmitsRecordingStopped()
    {
        QTemporaryDir dir;
        QString path = dir.filePath("test.avi");
        service_->startRecording(path);
        QSignalSpy spy(service_, &VideoRecordingService::recordingStopped);

        service_->stopRecording();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), path);
    }

    void testStopRecording_ReportsFrameCount()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("test.avi"));
        service_->addFrame(makeTestFrame());
        service_->addFrame(makeTestFrame());
        QSignalSpy spy(service_, &VideoRecordingService::recordingStopped);

        service_->stopRecording();

        QCOMPARE(spy.first().at(1).toInt(), 2);
    }

    // =========================================================
    // addFrame
    // =========================================================

    void testAddFrame_IncrementsFrameCount()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("test.avi"));

        service_->addFrame(makeTestFrame());

        QCOMPARE(service_->frameCount(), 1);
    }

    void testAddFrame_MultipleFrames_CountsCorrectly()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("test.avi"));

        service_->addFrame(makeTestFrame());
        service_->addFrame(makeTestFrame());
        service_->addFrame(makeTestFrame());

        QCOMPARE(service_->frameCount(), 3);
    }

    void testAddFrame_WhenNotRecording_IsNoOp()
    {
        // Should not crash, count stays zero
        service_->addFrame(makeTestFrame());

        QCOMPARE(service_->frameCount(), 0);
    }

    // =========================================================
    // addAudioSamples
    // =========================================================

    void testAddAudioSamples_WhenRecording_DoesNotCrash()
    {
        QTemporaryDir dir;
        service_->startRecording(dir.filePath("test.avi"));

        service_->addAudioSamples(makeSilence(), 192);

        QVERIFY(true);  // no crash
    }

    void testAddAudioSamples_WhenNotRecording_IsNoOp()
    {
        // Should not crash
        service_->addAudioSamples(makeSilence(), 192);

        QVERIFY(true);
    }

    // =========================================================
    // Output file integrity
    // =========================================================

    void testOutputFile_NonZeroSizeAfterStop()
    {
        QTemporaryDir dir;
        QString path = dir.filePath("test.avi");

        service_->startRecording(path);
        service_->addFrame(makeTestFrame());
        service_->stopRecording();

        QFile file(path);
        QVERIFY(file.exists());
        QVERIFY(file.size() > 0);
    }

    void testOutputFile_StartsWithRiffMagic()
    {
        QTemporaryDir dir;
        QString path = dir.filePath("test.avi");

        service_->startRecording(path);
        service_->addFrame(makeTestFrame());
        service_->stopRecording();

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray header = file.read(4);
        QCOMPARE(header, QByteArray("RIFF"));
    }

    void testOutputFile_ContainsAviChunk()
    {
        QTemporaryDir dir;
        QString path = dir.filePath("test.avi");

        service_->startRecording(path);
        service_->addFrame(makeTestFrame());
        service_->stopRecording();

        QFile file(path);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QByteArray first12 = file.read(12);
        // Bytes 8..11 should be "AVI " (RIFF type)
        QCOMPARE(first12.mid(8, 4), QByteArray("AVI "));
    }

private:
    VideoRecordingService *service_ = nullptr;
};

QTEST_MAIN(TestVideoRecordingService)
#include "test_videorecordingservice.moc"
