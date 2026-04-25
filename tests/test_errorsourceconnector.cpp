#include "mocks/mockfiledownloader.h"
#include "mocks/mockrestclient.h"
#include "models/remotefilemodel.h"
#include "services/configfileloader.h"
#include "services/errorhandler.h"
#include "services/filepreviewservice.h"
#include "services/gamebase64service.h"
#include "services/hvscmetadataservice.h"
#include "services/iftpclient.h"
#include "services/songlengthsdatabase.h"

#include <QSignalSpy>
#include <QWidget>
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

class SignallingSonglengthsDatabase : public SonglengthsDatabase
{
    Q_OBJECT

public:
    explicit SignallingSonglengthsDatabase(IFileDownloader *dl, QObject *parent = nullptr)
        : SonglengthsDatabase(dl, parent)
    {
    }
    void emitDownloadFailed(const QString &err) { emit downloadFailed(err); }
};

class SignallingHVSCMetadataService : public HVSCMetadataService
{
    Q_OBJECT

public:
    explicit SignallingHVSCMetadataService(IFileDownloader *s, IFileDownloader *b,
                                           QObject *parent = nullptr)
        : HVSCMetadataService(s, b, parent)
    {
    }
    void emitStilDownloadFailed(const QString &err) { emit stilDownloadFailed(err); }
    void emitBuglistDownloadFailed(const QString &err) { emit buglistDownloadFailed(err); }
};

class SignallingGameBase64Service : public GameBase64Service
{
    Q_OBJECT

public:
    explicit SignallingGameBase64Service(IFileDownloader *dl, QObject *parent = nullptr)
        : GameBase64Service(dl, parent)
    {
    }
    void emitDownloadFailed(const QString &err) { emit downloadFailed(err); }
};

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestErrorSourceConnector : public QObject
{
    Q_OBJECT

private:
    QWidget *parentWidget_ = nullptr;
    ErrorHandler *handler_ = nullptr;
    MockFileDownloader *downloader1_ = nullptr;
    MockFileDownloader *downloader2_ = nullptr;
    MockFileDownloader *downloader3_ = nullptr;

private slots:
    void init();
    void cleanup();

    void testNullSafety_AllNullSources();
    void testFtpClient_ErrorRoutedToDataError();
    void testRestClient_OperationFailedRoutedToOperationFailed();
    void testRemoteFileModel_ErrorOccurredRoutedToDataError();
    void testDeviceConnection_SkippedDueToCriticalDialog();
    void testFilePreviewService_PreviewFailedRoutedToOperationFailed();
    void testConfigFileLoader_LoadFailedRoutedToOperationFailed();
    void testTransferService_SkippedDueToHeavyDependencies();
    void testSonglengthsDatabase_DownloadFailedRoutedToDownloadError();
    void testHvscMetadataService_StilDownloadFailedRoutedToDownloadError();
    void testHvscMetadataService_BuglistDownloadFailedRoutedToDownloadError();
    void testGameBase64Service_DownloadFailedRoutedToDownloadError();
};

void TestErrorSourceConnector::init()
{
    parentWidget_ = new QWidget();
    handler_ = new ErrorHandler(parentWidget_);
    downloader1_ = new MockFileDownloader();
    downloader2_ = new MockFileDownloader();
    downloader3_ = new MockFileDownloader();
}

void TestErrorSourceConnector::cleanup()
{
    delete handler_;
    handler_ = nullptr;
    delete parentWidget_;
    parentWidget_ = nullptr;
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
                             nullptr, nullptr);
    // If we reach here without crashing, the null guard works
    QVERIFY(true);
}

void TestErrorSourceConnector::testFtpClient_ErrorRoutedToDataError()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    MinimalFtpSource ftpSource;
    handler_->connectSources(nullptr, nullptr, nullptr, &ftpSource, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr);

    ftpSource.emitError("FTP connection dropped");

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toString().contains("FTP connection dropped"));
}

void TestErrorSourceConnector::testRestClient_OperationFailedRoutedToOperationFailed()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    MockRestClient restClient;
    handler_->connectSources(nullptr, &restClient, nullptr, nullptr, nullptr, nullptr, nullptr,
                             nullptr, nullptr, nullptr);

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
                             nullptr, nullptr);

    model.emitError("Failed to list directory");

    QCOMPARE(spy.count(), 1);
    QVERIFY(spy.at(0).at(0).toString().contains("Failed to list directory"));
}

void TestErrorSourceConnector::testDeviceConnection_SkippedDueToCriticalDialog()
{
    QSKIP("DeviceConnection::connectionError routes to handleConnectionError which uses Critical "
          "severity and shows a blocking QMessageBox dialog");
}

void TestErrorSourceConnector::testFilePreviewService_PreviewFailedRoutedToOperationFailed()
{
    QSignalSpy spy(handler_, &ErrorHandler::statusMessage);

    MinimalFtpSource ftpSource;
    SignallingFilePreviewService fps(&ftpSource);
    handler_->connectSources(nullptr, nullptr, nullptr, nullptr, &fps, nullptr, nullptr, nullptr,
                             nullptr, nullptr);

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
                             nullptr, nullptr);

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
                             nullptr, nullptr);

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
                             &hvsc, nullptr);

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
                             &hvsc, nullptr);

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
                             nullptr, &gb64);

    gb64.emitDownloadFailed("Disk full");

    QCOMPARE(spy.count(), 1);
    QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    QCOMPARE(spy.at(0).at(1).value<ErrorSeverity>(), ErrorSeverity::Warning);
    QVERIFY(spy.at(0).at(2).toString().contains("GameBase64"));
    QCOMPARE(spy.at(0).at(3).toString(), QString("Disk full"));
}

QTEST_MAIN(TestErrorSourceConnector)
#include "test_errorsourceconnector.moc"
