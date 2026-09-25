#include "core/transfercore.h"
#include "ftp/recursivescancoordinator.h"
#include "ftp/remotedirectorycoordinator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystemservice.h"
#include "services/transferdeletehandler.h"

#include <QSignalSpy>
#include <QtTest>

class TestTransferDeleteHandler : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystemService *mockFs = nullptr;
    RecursiveScanCoordinator *scanCoordinator = nullptr;
    RemoteDirectoryCoordinator *dirCreator = nullptr;
    TransferDeleteHandler *handler = nullptr;
    void setupCallbacks()
    {
        handler->setCreateBatchCallback([this](transfer::OperationType type, const QString &desc,
                                               const QString &folder, const QString &src) -> int {
            auto r = transfer::createBatch(state_, type, desc, folder, src);
            state_ = r.newState;
            return r.batchId;
        });
    }

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFs = new MockLocalFileSystemService(this);
        scanCoordinator = new RecursiveScanCoordinator(state_, mockFtp, mockFs, this);
        dirCreator = new RemoteDirectoryCoordinator(state_, mockFtp, mockFs, this);
        handler = new TransferDeleteHandler(state_, this);
        handler->setFtpClient(mockFtp);
        handler->setScanCoordinator(scanCoordinator);
        handler->setDirCreator(dirCreator);
        setupCallbacks();
        mockFtp->mockSetConnected(true);
    }

    void cleanup()
    {
        delete handler;
        delete dirCreator;
        delete scanCoordinator;
        delete mockFs;
        delete mockFtp;
        handler = nullptr;
        dirCreator = nullptr;
        scanCoordinator = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
    }

    void testEnqueueDelete_addsItemToState()
    {
        handler->enqueueDelete("/remote/file.prg", false);

        QCOMPARE(state_.items.size(), 1);
        QCOMPARE(state_.items[0].remotePath, QString("/remote/file.prg"));
        QCOMPARE(state_.items[0].operationType, transfer::OperationType::Delete);
    }

    void testEnqueueDelete_emitsRowsInsertedSignal()
    {
        QSignalSpy insertedSpy(handler, &TransferDeleteHandler::rowsInserted);
        QSignalSpy aboutToInsertSpy(handler, &TransferDeleteHandler::rowsAboutToBeInserted);

        handler->enqueueDelete("/remote/file.prg", false);

        QCOMPARE(aboutToInsertSpy.count(), 1);
        QCOMPARE(insertedSpy.count(), 1);
        // Signal carries row range 0..0 for first item
        QCOMPARE(aboutToInsertSpy[0][0].toInt(), 0);
        QCOMPARE(aboutToInsertSpy[0][1].toInt(), 0);
    }

    void testEnqueueDelete_idleState_schedulesProcessNext()
    {
        state_.queueState = transfer::QueueState::Idle;

        QSignalSpy spy(handler, &TransferDeleteHandler::scheduleProcessNextRequested);

        handler->enqueueDelete("/remote/file.prg", false);

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueDelete_emitsQueueChanged()
    {
        QSignalSpy spy(handler, &TransferDeleteHandler::queueChanged);

        handler->enqueueDelete("/remote/file.prg", false);

        QCOMPARE(spy.count(), 1);
    }

    void testStartRecursiveDelete_notConnected_requestsAbandoningTheFolderOperation()
    {
        mockFtp->mockSetConnected(false);

        QSignalSpy abandonSpy(handler, &TransferDeleteHandler::abandonFolderOperationRequested);
        QSignalSpy failedSpy(handler, &TransferDeleteHandler::operationFailed);

        handler->startRecursiveDelete("/remote/Games");

        QCOMPARE(abandonSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 0);  // reported once, by whoever fails the operation
        QVERIFY(mockFtp->mockGetListRequests().isEmpty());
    }

    void testStartRecursiveDelete_connected_transitionsToScanning()
    {
        QSignalSpy spy(handler, &TransferDeleteHandler::transitionToRequested);

        handler->startRecursiveDelete("/remote/Games");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<transfer::QueueState>(), transfer::QueueState::Scanning);
        QCOMPARE(mockFtp->mockGetListRequests(), QStringList{"/remote/Games"});
    }

    void testProcessNextDelete_notConnected_requestsAbandoningTheFolderOperation()
    {
        mockFtp->mockSetConnected(false);

        QSignalSpy abandonSpy(handler, &TransferDeleteHandler::abandonFolderOperationRequested);

        handler->processNextDelete();

        QCOMPARE(abandonSpy.count(), 1);
    }

    void testProcessNextDelete_emptyDeleteQueue_reportsTheDeleteFinished()
    {
        QSignalSpy completedSpy(handler, &TransferDeleteHandler::operationCompleted);
        QSignalSpy finishedSpy(handler, &TransferDeleteHandler::recursiveDeleteFinished);

        handler->processNextDelete();

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 1);
    }

    void testProcessNextDelete_pendingFileInQueue_callsFtpRemove()
    {
        transfer::DeleteItem di;
        di.path = "/remote/file.prg";
        di.isDirectory = false;
        state_.deleteQueue.append(di);
        state_.deletedCount = 0;

        handler->processNextDelete();

        QCOMPARE(mockFtp->mockGetDeleteRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetDeleteRequests().first(), QString("/remote/file.prg"));
    }
};

QTEST_MAIN(TestTransferDeleteHandler)
#include "test_transferdeletehandler.moc"
