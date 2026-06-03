#include "ftp/recursivescancoordinator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystemservice.h"
#include "ftp/ftpentry.h"
#include "core/transfercore.h"

#include <QSignalSpy>
#include <QtTest>

class TestRecursiveScanCoordinator : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystemService *mockFs = nullptr;
    RecursiveScanCoordinator *scanner = nullptr;

    static FtpEntry makeDir(const QString &name)
    {
        FtpEntry e;
        e.name = name;
        e.isDirectory = true;
        return e;
    }

    static FtpEntry makeFile(const QString &name, qint64 size = 0)
    {
        FtpEntry e;
        e.name = name;
        e.isDirectory = false;
        e.size = size;
        return e;
    }

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFs = new MockLocalFileSystemService(this);
        scanner = new RecursiveScanCoordinator(state_, mockFtp, mockFs, this);
        mockFtp->mockSetConnected(true);
        connect(mockFtp, &IFtpClient::directoryListed, scanner,
                &RecursiveScanCoordinator::onDirectoryListed);
    }

    void cleanup()
    {
        delete scanner;
        delete mockFs;
        delete mockFtp;
        scanner = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
    }

    void testHandlesListing_trueForDownloadPath()
    {
        scanner->startDownloadScan("/remote/dir", "/local", "/remote/dir", 1);
        QVERIFY(scanner->handlesListing("/remote/dir"));
    }

    void testHandlesListing_trueForDeletePath()
    {
        scanner->startDeleteScan("/remote/dir");
        QVERIFY(scanner->handlesListing("/remote/dir"));
    }

    void testHandlesListing_falseForUnknownPath()
    {
        QVERIFY(!scanner->handlesListing("/unknown/path"));
    }

    void testStartDownloadScan_resetsCounters()
    {
        state_.directoriesScanned = 5;
        state_.filesDiscovered = 10;

        scanner->startDownloadScan("/remote/Games", "/local/downloads", "/remote/Games", 1);

        QCOMPARE(state_.directoriesScanned, 0);
        QCOMPARE(state_.filesDiscovered, 0);
    }

    void testStartDownloadScan_emitsScanningStartedWithDownload()
    {
        QSignalSpy spy(scanner, &RecursiveScanCoordinator::scanningStarted);

        scanner->startDownloadScan("/remote/Games", "/local/downloads", "/remote/Games", 1);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).value<transfer::OperationType>(),
                 transfer::OperationType::Download);
    }

    void testStartDownloadScan_issuesListCallOnFtp()
    {
        scanner->startDownloadScan("/remote/dir", "/local/dir", "/remote/dir", 42);

        QCOMPARE(mockFtp->mockGetListRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetListRequests().first(), QString("/remote/dir"));
    }

    void testOnDirectoryListed_download_emitsDownloadFileDiscoveredForFiles()
    {
        scanner->startDownloadScan("/remote/dir", "/local/base", "/remote/dir", 1);

        QSignalSpy spy(scanner, &RecursiveScanCoordinator::downloadFileDiscovered);

        QList<FtpEntry> entries = {makeFile("game.prg", 4096)};
        mockFtp->mockSetDirectoryListing("/remote/dir", entries);
        mockFtp->mockProcessNextOperation();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/remote/dir/game.prg"));
        QCOMPARE(spy.at(0).at(2).toInt(), 1);
    }

    void testOnDirectoryListed_download_createsLocalDirForSubdirs()
    {
        scanner->startDownloadScan("/remote/root", "/local/base", "/remote/root", 1);

        QList<FtpEntry> entries = {makeDir("sub")};
        mockFtp->mockSetDirectoryListing("/remote/root", entries);
        mockFtp->mockProcessNextOperation();

        QVERIFY(mockFs->mockGetMkpathRequests().contains("/local/base/sub"));
    }

    void testStartDeleteScan_initializesState()
    {
        scanner->startDeleteScan("/remote/Demos");

        QCOMPARE(state_.scanningFolderName, QString("Demos"));
        QCOMPARE(state_.directoriesScanned, 0);
        QVERIFY(state_.requestedDeleteListings.contains("/remote/Demos"));
    }

    void testStartDeleteScan_emitsScanningStartedWithDelete()
    {
        QSignalSpy spy(scanner, &RecursiveScanCoordinator::scanningStarted);

        scanner->startDeleteScan("/remote/Demos");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(1).value<transfer::OperationType>(), transfer::OperationType::Delete);
    }

    void testOnDirectoryListed_delete_emitsDeleteScanCompleteWhenDone()
    {
        scanner->startDeleteScan("/remote/dir");

        QSignalSpy spy(scanner, &RecursiveScanCoordinator::deleteScanComplete);

        QList<FtpEntry> entries = {makeFile("file.prg")};
        mockFtp->mockSetDirectoryListing("/remote/dir", entries);
        mockFtp->mockProcessNextOperation();

        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestRecursiveScanCoordinator)
#include "test_recursivescancoordinator.moc"
