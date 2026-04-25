#include "mocks/mockdetailsdisplay.h"
#include "mocks/mockftpclient.h"
#include "services/filepreviewservice.h"
#include "ui/previewcoordinator.h"

#include <QSignalSpy>
#include <QtTest>

// ---------------------------------------------------------------------------
// SignallingPreviewService — thin subclass that exposes emit helpers so tests
// can fire previewReady / previewFailed without going through real FTP.
// ---------------------------------------------------------------------------

class SignallingPreviewService : public FilePreviewService
{
    Q_OBJECT
public:
    explicit SignallingPreviewService(IFtpClient *ftp, QObject *parent = nullptr)
        : FilePreviewService(ftp, parent)
    {
    }

    void emitPreviewReady(const QString &path, const QByteArray &data)
    {
        emit previewReady(path, data);
    }

    void emitPreviewFailed(const QString &path, const QString &error)
    {
        emit previewFailed(path, error);
    }
};

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestPreviewCoordinator : public QObject
{
    Q_OBJECT

private:
    MockFtpClient *mockFtp_ = nullptr;
    SignallingPreviewService *previewService_ = nullptr;
    MockDetailsDisplay mockDisplay_;
    PreviewCoordinator *coord_ = nullptr;

private slots:
    void init()
    {
        mockFtp_ = new MockFtpClient(this);
        previewService_ = new SignallingPreviewService(mockFtp_);  // no parent — cleanup owns it
        coord_ = new PreviewCoordinator(previewService_, &mockDisplay_, nullptr);  // no parent
    }

    void cleanup()
    {
        delete coord_;
        coord_ = nullptr;
        delete previewService_;
        previewService_ = nullptr;
        // mockFtp_ is parented to `this` (the test object), cleaned up automatically
        mockDisplay_.reset();
    }

    // =========================================================
    // onFileContentRequested
    // =========================================================

    void testOnFileContentRequested_NullService_NoOp()
    {
        // Coordinator constructed with null service must not crash
        PreviewCoordinator nullServiceCoord(nullptr, &mockDisplay_, nullptr);
        // Should silently do nothing
        nullServiceCoord.onFileContentRequested("/any/file.sid");
        QVERIFY(mockDisplay_.sidDetailsCalls.isEmpty());
    }

    // =========================================================
    // onPreviewReady — routing by file extension
    // =========================================================

    void testOnPreviewReady_DiskImage_CallsShowDiskDirectory()
    {
        QByteArray diskData("D64DATA");
        previewService_->emitPreviewReady("/game.d64", diskData);

        QCOMPARE(mockDisplay_.diskDirectoryCalls.size(), 1);
        QCOMPARE(mockDisplay_.diskDirectoryCalls.at(0).first, QStringLiteral("/game.d64"));
        QCOMPARE(mockDisplay_.diskDirectoryCalls.at(0).second, diskData);
        QVERIFY(mockDisplay_.sidDetailsCalls.isEmpty());
        QVERIFY(mockDisplay_.textContentCalls.isEmpty());
    }

    void testOnPreviewReady_SidFile_CallsShowSidDetails()
    {
        QByteArray sidData("SIDDATA");
        previewService_->emitPreviewReady("/song.sid", sidData);

        QCOMPARE(mockDisplay_.sidDetailsCalls.size(), 1);
        QCOMPARE(mockDisplay_.sidDetailsCalls.at(0).first, QStringLiteral("/song.sid"));
        QCOMPARE(mockDisplay_.sidDetailsCalls.at(0).second, sidData);
        QVERIFY(mockDisplay_.diskDirectoryCalls.isEmpty());
        QVERIFY(mockDisplay_.textContentCalls.isEmpty());
    }

    void testOnPreviewReady_TextFile_CallsShowTextContent()
    {
        const QString content = QStringLiteral("Hello, world!");
        previewService_->emitPreviewReady("/readme.txt", content.toUtf8());

        QCOMPARE(mockDisplay_.textContentCalls.size(), 1);
        QCOMPARE(mockDisplay_.textContentCalls.at(0), content);
        QVERIFY(mockDisplay_.diskDirectoryCalls.isEmpty());
        QVERIFY(mockDisplay_.sidDetailsCalls.isEmpty());
    }

    void testOnPreviewReady_NullDisplay_NoCrash()
    {
        // Coordinator with null display must not crash when preview arrives
        PreviewCoordinator nullDisplayCoord(previewService_, nullptr, nullptr);
        // emitting here exercises the null-guard branch in onPreviewReady
        previewService_->emitPreviewReady("/game.d64", QByteArray("DATA"));
        // If we reach here without a crash the test passes
        QVERIFY(true);
    }

    // =========================================================
    // onPreviewFailed
    // =========================================================

    void testOnPreviewFailed_CallsShowError()
    {
        previewService_->emitPreviewFailed("/bad.sid", QStringLiteral("timeout"));

        QCOMPARE(mockDisplay_.errorCalls.size(), 1);
        QCOMPARE(mockDisplay_.errorCalls.at(0), QStringLiteral("timeout"));
    }

    void testOnPreviewFailed_NullDisplay_NoCrash()
    {
        PreviewCoordinator nullDisplayCoord(previewService_, nullptr, nullptr);
        previewService_->emitPreviewFailed("/bad.sid", QStringLiteral("timeout"));
        QVERIFY(true);
    }

    // =========================================================
    // onConfigLoadFinished
    // =========================================================

    void testOnConfigLoadFinished_EmitsStatusMessage()
    {
        QSignalSpy spy(coord_, &PreviewCoordinator::statusMessage);
        coord_->onConfigLoadFinished("/SD/config/myconfig.cfg");

        QCOMPARE(spy.count(), 1);
        const QString msg = spy.at(0).at(0).toString();
        QVERIFY(msg.contains("myconfig.cfg"));
    }

    // =========================================================
    // onConfigLoadFailed — blocked because QMessageBox::warning
    // =========================================================

    void testOnConfigLoadFailed_SkippedDueToBlockingDialog()
    {
        QSKIP("onConfigLoadFailed shows a blocking QMessageBox::warning which cannot run "
              "headlessly in a unit test");
    }

    // =========================================================
    // onPreviewReady with PlaylistManager — null playlist no crash
    // =========================================================

    void testOnPreviewReady_WithNullPlaylistManager_NoCrash()
    {
        // coord_ already has null PlaylistManager — SID preview must not crash
        QByteArray sidData("SIDDATA");
        previewService_->emitPreviewReady("/tune.sid", sidData);

        QCOMPARE(mockDisplay_.sidDetailsCalls.size(), 1);
        QCOMPARE(mockDisplay_.sidDetailsCalls.at(0).first, QStringLiteral("/tune.sid"));
    }
};

QTEST_MAIN(TestPreviewCoordinator)
#include "test_previewcoordinator.moc"
