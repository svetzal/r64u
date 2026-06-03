#include "core/transfercore.h"
#include "ftp/remotedirectorycoordinator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystemservice.h"

#include <QSignalSpy>
#include <QtTest>

class TestRemoteDirectoryCoordinator : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystemService *mockFs = nullptr;
    RemoteDirectoryCoordinator *creator = nullptr;

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFs = new MockLocalFileSystemService(this);
        creator = new RemoteDirectoryCoordinator(state_, mockFtp, mockFs, this);
        mockFtp->mockSetConnected(true);
    }

    void cleanup()
    {
        delete creator;
        delete mockFs;
        delete mockFtp;
        creator = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
    }

    void testQueueDirectoriesForUpload_noSubdirs_queuesSingleRoot()
    {
        mockFs->mockSetSubdirectories("/local/mydir", QStringList{});

        creator->queueDirectoriesForUpload("/local/mydir", "/remote/mydir");

        QCOMPARE(state_.pendingMkdirs.size(), 1);
        QCOMPARE(state_.totalDirectoriesToCreate, 1);
    }

    void testQueueDirectoriesForUpload_withSubdirs_queuesAllWithRelativePaths()
    {
        mockFs->mockSetSubdirectories("/local/root", {"/local/root/subA", "/local/root/subA/deep"});

        creator->queueDirectoriesForUpload("/local/root", "/remote/root");

        QCOMPARE(state_.pendingMkdirs.size(), 3);
        QCOMPARE(state_.totalDirectoriesToCreate, 3);
        QCOMPARE(state_.pendingMkdirs.dequeue().remotePath, QString("/remote/root"));
        QCOMPARE(state_.pendingMkdirs.dequeue().remotePath, QString("/remote/root/subA"));
        QCOMPARE(state_.pendingMkdirs.dequeue().remotePath, QString("/remote/root/subA/deep"));
    }

    void testQueueDirectoriesForUpload_resetsDirCreatedCounter()
    {
        state_.directoriesCreated = 99;
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});

        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");

        QCOMPARE(state_.directoriesCreated, 0);
    }

    void testQueueDirectoriesForUpload_emitsProgressSignal()
    {
        QSignalSpy spy(creator, &RemoteDirectoryCoordinator::directoryCreationProgress);
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});

        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(spy.at(0).at(1).toInt(), 1);
    }

    void testCreateNextDirectory_issuesMkdirForHead()
    {
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");

        creator->createNextDirectory();

        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetMkdirRequests().first(), QString("/remote/dir"));
    }

    void testCreateNextDirectory_emptyQueue_emitsAllDirectoriesCreated()
    {
        QSignalSpy spy(creator, &RemoteDirectoryCoordinator::allDirectoriesCreated);

        creator->createNextDirectory();

        QCOMPARE(spy.count(), 1);
        QVERIFY(mockFtp->mockGetMkdirRequests().isEmpty());
    }

    void testOnDirectoryCreated_incrementsCounterAndDequeues()
    {
        mockFs->mockSetSubdirectories("/local/dir", {"/local/dir/sub"});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");
        state_.queueState = transfer::QueueState::CreatingDirectories;

        creator->onDirectoryCreated("/remote/dir");

        QCOMPARE(state_.directoriesCreated, 1);
        QCOMPARE(state_.pendingMkdirs.size(), 1);
    }

    void testOnDirectoryCreated_lastDir_emitsAllDirectoriesCreated()
    {
        QSignalSpy spy(creator, &RemoteDirectoryCoordinator::allDirectoriesCreated);
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");
        state_.queueState = transfer::QueueState::CreatingDirectories;

        creator->onDirectoryCreated("/remote/dir");

        QCOMPARE(spy.count(), 1);
    }

    void testOnDirectoryCreated_wrongState_ignored()
    {
        mockFs->mockSetSubdirectories("/local/dir", QStringList{});
        creator->queueDirectoriesForUpload("/local/dir", "/remote/dir");
        state_.queueState = transfer::QueueState::Idle;

        creator->onDirectoryCreated("/remote/dir");

        QCOMPARE(state_.directoriesCreated, 0);
    }
};

QTEST_MAIN(TestRemoteDirectoryCoordinator)
#include "test_remotedirectorycoordinator.moc"
