#include "mocks/mockfiledownloaderservice.h"
#include "mocks/mockrestclient.h"
#include "models/remotefilemodel.h"
#include "services/configfileloader.h"
#include "services/errorhandler.h"
#include "services/filepreviewservice.h"
#include "services/gamebase64service.h"
#include "services/hvscmetadataservice.h"
#include "services/iaudioplaybackservice.h"
#include "services/ierroremitter.h"
#include "services/iftpclient.h"
#include "services/keyboardinputservice.h"
#include "services/remotefileoperations.h"
#include "services/songlengthsdatabaseservice.h"
#include "services/videorecordingservice.h"

#include <QSignalSpy>
#include <QtTest/QtTest>

// ---------------------------------------------------------------------------
// Inline signal-emitting wrappers
// ---------------------------------------------------------------------------

class MinimalFtpSource : public IFtpClient
{
    Q_OBJECT

public:
    explicit MinimalFtpSource(QObject *parent = nullptr) : IFtpClient(parent) {}
    void setHost(const QString &, quint16) override {}
    [[nodiscard]] QString host() const override { return {}; }
    void setCredentials(const QString &, const QString &) override {}
    [[nodiscard]] State state() const override { return State::Disconnected; }
    [[nodiscard]] bool isConnected() const override { return false; }
    [[nodiscard]] bool isLoggedIn() const override { return false; }
    void connectToHost() override {}
    void disconnect() override {}
    void list(const QString &) override {}
    void changeDirectory(const QString &) override {}
    void makeDirectory(const QString &) override {}
    void removeDirectory(const QString &) override {}
    [[nodiscard]] QString currentDirectory() const override { return {}; }
    void download(const QString &, const QString &) override {}
    void downloadToMemory(const QString &) override {}
    void upload(const QString &, const QString &) override {}
    void remove(const QString &) override {}
    void rename(const QString &, const QString &) override {}
    void abort() override {}

    void emitError(const QString &msg) { emit error(msg); }
};

class SignallingRemoteFileModel : public RemoteFileModel
{
    Q_OBJECT

public:
    explicit SignallingRemoteFileModel(QObject *parent = nullptr) : RemoteFileModel(parent) {}
    void emitError(const QString &msg) { emit errorOccurred(msg); }
};

class SignallingFilePreviewService : public FilePreviewService
{
    Q_OBJECT

public:
    explicit SignallingFilePreviewService(IFtpClient *ftp, QObject *parent = nullptr)
        : FilePreviewService(ftp, parent)
    {
    }
    void emitPreviewFailed(const QString &path, const QString &err)
    {
        emit previewFailed(path, err);
    }
};

class SignallingConfigFileLoader : public ConfigFileLoader
{
    Q_OBJECT

public:
    explicit SignallingConfigFileLoader(QObject *parent = nullptr) : ConfigFileLoader(parent) {}
    void emitLoadFailed(const QString &path, const QString &err) { emit loadFailed(path, err); }
};

class SignallingSonglengthsDatabase : public SonglengthsDatabaseService
{
    Q_OBJECT

public:
    explicit SignallingSonglengthsDatabase(IFileDownloaderService *dl, QObject *parent = nullptr)
        : SonglengthsDatabaseService(dl, parent)
    {
    }
    void emitDownloadFailed(const QString &err)
    {
        emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                           tr("Song lengths database download failed"), err);
    }
};

class SignallingHVSCMetadataService : public HVSCMetadataService
{
    Q_OBJECT

public:
    explicit SignallingHVSCMetadataService(IFileDownloaderService *s, IFileDownloaderService *b,
                                           QObject *parent = nullptr)
        : HVSCMetadataService(s, b, parent)
    {
    }
    void emitStilDownloadFailed(const QString &err)
    {
        emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                           tr("HVSC STIL download failed"), err);
    }
    void emitBuglistDownloadFailed(const QString &err)
    {
        emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                           tr("HVSC BUGlist download failed"), err);
    }
};

class SignallingGameBase64Service : public GameBase64Service
{
    Q_OBJECT

public:
    explicit SignallingGameBase64Service(IFileDownloaderService *dl, QObject *parent = nullptr)
        : GameBase64Service(dl, parent)
    {
    }
    void emitDownloadFailed(const QString &err)
    {
        emit errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                           tr("GameBase64 download failed"), err);
    }
};

class MinimalAudioPlaybackSource : public IAudioPlaybackService
{
    Q_OBJECT

public:
    explicit MinimalAudioPlaybackSource(QObject *parent = nullptr) : IAudioPlaybackService(parent)
    {
    }
    bool start() override { return true; }
    void stop() override {}
    [[nodiscard]] bool isPlaying() const override { return false; }
    void writeSamples(const QByteArray & /*samples*/, int /*sampleCount*/) override {}

    void emitError(const QString &msg) { emit errorOccurred(msg); }
};

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestErrorSourceConnector : public QObject
{
    Q_OBJECT

private:
    ErrorHandler *handler_ = nullptr;
    MockFileDownloaderService *downloader1_ = nullptr;
    MockFileDownloaderService *downloader2_ = nullptr;
    MockFileDownloaderService *downloader3_ = nullptr;

private slots:
    void init();
    void cleanup();

    void testNullSafety_AllNullSources();
    void testFtpClient_ErrorRoutedToDataError();
    void testRestClient_OperationFailedRoutedToOperationFailed();
    void testRemoteFileModel_ErrorOccurredRoutedToDataError();
    void testDeviceConnectionManager_SkippedDueToCriticalDialog();
    void testFilePreviewService_PreviewFailedRoutedToOperationFailed();
    void testConfigFileLoader_LoadFailedRoutedToOperationFailed();
    void testTransferService_SkippedDueToHeavyDependencies();
    void testSonglengthsDatabase_DownloadFailedRoutedToDownloadError();
    void testHvscMetadataService_StilDownloadFailedRoutedToDownloadError();
    void testHvscMetadataService_BuglistDownloadFailedRoutedToDownloadError();
    void testGameBase64Service_DownloadFailedRoutedToDownloadError();
    void testRemoteFileOperations_OperationFailedRoutedToOperationFailed();
    void testAudioPlaybackService_ErrorOccurredRoutedToStreamingError();
    void testVideoRecordingService_ErrorRoutedToStreamingError();
    void testKeyboardInputService_ErrorOccurredRoutedToDataError();
};

void TestErrorSourceConnector::init()
{
    handler_ = new ErrorHandler(nullptr);
    downloader1_ = new MockFileDownloaderService();
    downloader2_ = new MockFileDownloaderService();
    downloader3_ = new MockFileDownloaderService();
}

void TestErrorSourceConnector::cleanup()
{
    delete handler_;
    handler_ = nullptr;
    delete downloader1_;
    downloader1_ = nullptr;
    delete downloader2_;
    downloader2_ = nullptr;
    delete downloader3_;
    downloader3_ = nullptr;
}

void TestErrorSourceConnector::testNullSafety_AllNullSources()
{
    // connectSources() with all nulls must not crash
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr);
    // If we reach here without crashing, the null guard works
    QVERIFY(true);
}

void TestErrorSourceConnector::testFtpClient_ErrorRoutedToDataError()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    MinimalFtpSource ftpSource;
    handler_->connectSources(nullptr, nullptr, nullptr, &ftpSource, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    ftpSource.emitError("FTP connection dropped");

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toString().contains("FTP connection dropped"));
}

void TestErrorSourceConnector::testRestClient_OperationFailedRoutedToOperationFailed()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    MockRestClient restClient;
    handler_->connectSources(nullptr, &restClient, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr);

    restClient.mockEmitOperationFailed("getVersion", "Host unreachable");

    QCOMPARE(spy.count(), 1);
    const QString msg = spy.at(0).at(0).toString();
    QVERIFY(msg.contains("getVersion") || msg.contains("failed"));
    QVERIFY(msg.contains("Host unreachable"));
}

void TestErrorSourceConnector::testRemoteFileModel_ErrorOccurredRoutedToDataError()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    SignallingRemoteFileModel model;
    handler_->connectSources(nullptr, nullptr, &model, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr);

    model.emitError("Failed to list directory");

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toString().contains("Failed to list directory"));
}

void TestErrorSourceConnector::testDeviceConnectionManager_SkippedDueToCriticalDialog()
{
    QSKIP("DeviceConnectionManager::connectionError routes to handleConnectionError which uses "
          "Critical "
          "severity and shows a blocking QMessageBox dialog");
}

void TestErrorSourceConnector::testFilePreviewService_PreviewFailedRoutedToOperationFailed()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    MinimalFtpSource ftpSource;
    SignallingFilePreviewService fps(&ftpSource);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, &fps, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr);

    fps.emitPreviewFailed("/SD/game.prg", "Download timed out");

    QCOMPARE(spy.count(), 1);
    const QString msg = spy.at(0).at(0).toString();
    QVERIFY(msg.contains("Preview of"));
    QVERIFY(msg.contains("/SD/game.prg"));
}

void TestErrorSourceConnector::testConfigFileLoader_LoadFailedRoutedToOperationFailed()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    SignallingConfigFileLoader loader;
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, &loader, nullptr, nullptr,
                             nullptr, nullptr, nullptr);

    loader.emitLoadFailed("/SD/config/myconfig.cfg", "File not found");

    QCOMPARE(spy.count(), 1);
    const QString msg = spy.at(0).at(0).toString();
    // ConfigFileLoader lambda uses QFileInfo(path).fileName() which gives the basename
    QVERIFY(msg.contains("myconfig.cfg"));
}

void TestErrorSourceConnector::testTransferService_SkippedDueToHeavyDependencies()
{
    QSKIP("TransferService requires a heavy FTP/REST/queue chain to construct; "
          "its signal routing is covered by the existing TransferService test suite");
}

void TestErrorSourceConnector::testSonglengthsDatabase_DownloadFailedRoutedToDownloadError()
{
    QSignalSpy spy(handler_, &ErrorHandler::errorLogged);

    SignallingSonglengthsDatabase sld(downloader1_);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &sld,
                             nullptr, nullptr, nullptr);

    sld.emitDownloadFailed("Connection timed out");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QVERIFY(spy.at(0).at(2).toString().contains("Song lengths database"));
    QCOMPARE(spy.at(0).at(3).toString(), QString("Connection timed out"));
}

void TestErrorSourceConnector::testHvscMetadataService_StilDownloadFailedRoutedToDownloadError()
{
    QSignalSpy spy(handler_, &ErrorHandler::errorLogged);

    SignallingHVSCMetadataService hvsc(downloader1_, downloader2_);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             &hvsc, nullptr, nullptr);

    hvsc.emitStilDownloadFailed("Server error 503");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QVERIFY(spy.at(0).at(2).toString().contains("HVSC STIL"));
    QCOMPARE(spy.at(0).at(3).toString(), QString("Server error 503"));
}

void TestErrorSourceConnector::testHvscMetadataService_BuglistDownloadFailedRoutedToDownloadError()
{
    QSignalSpy spy(handler_, &ErrorHandler::errorLogged);

    SignallingHVSCMetadataService hvsc(downloader1_, downloader2_);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             &hvsc, nullptr, nullptr);

    hvsc.emitBuglistDownloadFailed("Network unreachable");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QVERIFY(spy.at(0).at(2).toString().contains("HVSC BUGlist"));
    QCOMPARE(spy.at(0).at(3).toString(), QString("Network unreachable"));
}

void TestErrorSourceConnector::testGameBase64Service_DownloadFailedRoutedToDownloadError()
{
    QSignalSpy spy(handler_, &ErrorHandler::errorLogged);

    SignallingGameBase64Service gb64(downloader1_);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, &gb64, nullptr);

    gb64.emitDownloadFailed("Disk full");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QVERIFY(spy.at(0).at(2).toString().contains("GameBase64"));
    QCOMPARE(spy.at(0).at(3).toString(), QString("Disk full"));
}

void TestErrorSourceConnector::testRemoteFileOperations_OperationFailedRoutedToOperationFailed()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    RemoteFileOperations rfo(nullptr);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, &rfo);

    rfo.createFolder("/some/path");

    QCOMPARE(spy.count(), 1);
    const QString msg = spy.at(0).at(0).toString();
    QVERIFY(msg.contains("Create folder") || msg.contains("FTP client not configured"));
}

void TestErrorSourceConnector::testAudioPlaybackService_ErrorOccurredRoutedToStreamingError()
{
    QSignalSpy spy(handler_, &ErrorHandler::errorLogged);

    MinimalAudioPlaybackSource apb;
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr, &apb);

    apb.emitError("No audio output device available");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::System);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QVERIFY(spy.at(0).at(2).toString().contains("Streaming Error"));
    QCOMPARE(spy.at(0).at(3).toString(), QString("No audio output device available"));
}

void TestErrorSourceConnector::testVideoRecordingService_ErrorRoutedToStreamingError()
{
    QSignalSpy spy(handler_, &ErrorHandler::errorLogged);

    VideoRecordingService vrs;
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr, nullptr, &vrs);

    emit vrs.errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           "Failed to write AVI frame");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::System);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QVERIFY(spy.at(0).at(2).toString().contains("Streaming Error"));
    QCOMPARE(spy.at(0).at(3).toString(), QString("Failed to write AVI frame"));
}

void TestErrorSourceConnector::testKeyboardInputService_ErrorOccurredRoutedToDataError()
{
    QSignalSpy spy(handler_, &ErrorHandler::errorLogged);

    KeyboardInputService kis(nullptr);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr, nullptr, nullptr, nullptr, &kis);

    // KeyboardInputService emits errorReported via its own emission in sendPetscii.
    // Emit errorReported directly to test the routing plumbing.
    emit kis.errorReported(ErrorCategory::FileOperation, ErrorSeverity::Warning, tr("Error"),
                           "Failed to send PETSCII key");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QCOMPARE(spy.at(0).at(3).toString(), QString("Failed to send PETSCII key"));
}

QTEST_MAIN(TestErrorSourceConnector)
#include "test_errorsourceconnector.moc"
