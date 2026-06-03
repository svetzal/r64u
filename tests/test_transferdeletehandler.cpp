#include "ftp/recursivescancoordinator.h"
#include "ftp/remotedirectorycoordinator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystemservice.h"
#include "models/transferdeletehandler.h"
#include "core/transfercore.h"

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

    void testEnqueueRecursiveDelete_notConnected_emitsOperationFailed()
    {
        mockFtp->mockSetConnected(false);

        QSignalSpy spy(handler, &TransferDeleteHandler::operationFailed);

        handler->enqueueRecursiveDelete("/remote/Games");

        QCOMPARE(spy.count(), 1);
    }

    void testEnqueueRecursiveDelete_connected_transitionsToScanning()
    {
        QSignalSpy spy(handler, &TransferDeleteHandler::transitionToRequested);

        handler->enqueueRecursiveDelete("/remote/Games");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy[0][0].value<transfer::QueueState>(), transfer::QueueState::Scanning);
    }

    void testProcessNextDelete_notConnected_transitionsToIdleAndEmitsFailed()
    {
        mockFtp->mockSetConnected(false);

        QSignalSpy failedSpy(handler, &TransferDeleteHandler::operationFailed);
        QSignalSpy transitionSpy(handler, &TransferDeleteHandler::transitionToRequested);

        handler->processNextDelete();

        QCOMPARE(transitionSpy.count(), 1);
        QCOMPARE(transitionSpy[0][0].value<transfer::QueueState>(), transfer::QueueState::Idle);
        QCOMPARE(failedSpy.count(), 1);
    }

    void testProcessNextDelete_emptyDeleteQueue_emitsOperationCompleted()
    {
        QSignalSpy spy(handler, &TransferDeleteHandler::operationCompleted);

        handler->processNextDelete();

        QCOMPARE(spy.count(), 1);
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
