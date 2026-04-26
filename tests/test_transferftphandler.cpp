#include "ftp/recursivescancoordinator.h"
#include "ftp/remotedirectorycreator.h"
#include "mocks/mockftpclient.h"
#include "mocks/mocklocalfilesystem.h"
#include "models/transferftphandler.h"
#include "services/transfercore.h"

#include <QSignalSpy>
#include <QtTest>

class TestTransferFtpHandler : public QObject
{
    Q_OBJECT

private:
    transfer::State state_;
    MockFtpClient *mockFtp = nullptr;
    MockLocalFileSystem *mockFs = nullptr;
    TransferFtpHandler *handler = nullptr;

private slots:
    void init()
    {
        state_ = transfer::State();
        mockFtp = new MockFtpClient(this);
        mockFs = new MockLocalFileSystem(this);
        handler = new TransferFtpHandler(state_, this);
        handler->setFtpClient(mockFtp);
    }

    void cleanup()
    {
        delete handler;
        delete mockFs;
        delete mockFtp;
        handler = nullptr;
        mockFs = nullptr;
        mockFtp = nullptr;
        state_ = transfer::State();
    }

    void testUploadProgressUpdatesBytesAndEmitsDataChanged()
    {
        transfer::TransferItem item;
        item.localPath = "/local/file.txt";
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Upload;
        item.status = transfer::TransferItem::Status::InProgress;
        state_.items.append(item);
        state_.currentIndex = 0;

        QSignalSpy spy(handler, &TransferFtpHandler::itemDataChanged);
        emit mockFtp->uploadProgress("/local/file.txt", 512, 1024);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(state_.items[0].bytesTransferred, qint64(512));
        QCOMPARE(state_.items[0].totalBytes, qint64(1024));
    }

    void testUploadFinishedMarksItemCompleted()
    {
        transfer::TransferItem item;
        item.localPath = "/local/file.txt";
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Upload;
        item.status = transfer::TransferItem::Status::InProgress;
        item.batchId = -1;
        state_.items.append(item);
        state_.currentIndex = 0;
        state_.queueState = transfer::QueueState::Transferring;

        QSignalSpy completedSpy(handler, &TransferFtpHandler::operationCompleted);
        QSignalSpy queueSpy(handler, &TransferFtpHandler::queueChanged);
        QSignalSpy schedSpy(handler, &TransferFtpHandler::scheduleProcessNextRequested);

        emit mockFtp->uploadFinished("/local/file.txt", "/remote/file.txt");

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toString(), QString("file.txt"));
        QVERIFY(queueSpy.count() >= 1);
        QVERIFY(schedSpy.count() >= 1);
        QCOMPARE(state_.items[0].status, transfer::TransferItem::Status::Completed);
    }

    void testDownloadProgressUpdatesBytesAndEmitsDataChanged()
    {
        transfer::TransferItem item;
        item.localPath = "/local/file.txt";
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Download;
        item.status = transfer::TransferItem::Status::InProgress;
        state_.items.append(item);
        state_.currentIndex = 0;

        QSignalSpy spy(handler, &TransferFtpHandler::itemDataChanged);
        emit mockFtp->downloadProgress("/remote/file.txt", 256, 2048);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 0);
        QCOMPARE(state_.items[0].bytesTransferred, qint64(256));
        QCOMPARE(state_.items[0].totalBytes, qint64(2048));
    }

    void testDownloadFinishedMarksItemCompleted()
    {
        transfer::TransferItem item;
        item.localPath = "/local/file.txt";
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Download;
        item.status = transfer::TransferItem::Status::InProgress;
        item.batchId = -1;
        state_.items.append(item);
        state_.currentIndex = 0;
        state_.queueState = transfer::QueueState::Transferring;

        QSignalSpy completedSpy(handler, &TransferFtpHandler::operationCompleted);
        QSignalSpy queueSpy(handler, &TransferFtpHandler::queueChanged);
        QSignalSpy schedSpy(handler, &TransferFtpHandler::scheduleProcessNextRequested);

        emit mockFtp->downloadFinished("/remote/file.txt", "/local/file.txt");

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(completedSpy.at(0).at(0).toString(), QString("file.txt"));
        QVERIFY(queueSpy.count() >= 1);
        QVERIFY(schedSpy.count() >= 1);
        QCOMPARE(state_.items[0].status, transfer::TransferItem::Status::Completed);
    }

    void testFtpErrorWithTransferItem_emitsOperationFailed()
    {
        transfer::TransferItem item;
        item.localPath = "/local/file.txt";
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Upload;
        item.status = transfer::TransferItem::Status::InProgress;
        item.batchId = -1;
        state_.items.append(item);
        state_.currentIndex = 0;
        state_.queueState = transfer::QueueState::Transferring;

        QSignalSpy dataChangedSpy(handler, &TransferFtpHandler::itemDataChanged);
        QSignalSpy failedSpy(handler, &TransferFtpHandler::operationFailed);
        QSignalSpy queueSpy(handler, &TransferFtpHandler::queueChanged);

        emit mockFtp->error("Connection reset");

        QCOMPARE(dataChangedSpy.count(), 1);
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(queueSpy.count(), 1);
    }

    void testFtpErrorWithNoCurrentItem_emitsOnlyQueueChanged()
    {
        state_.currentIndex = -1;
        state_.queueState = transfer::QueueState::Idle;

        QSignalSpy failedSpy(handler, &TransferFtpHandler::operationFailed);
        QSignalSpy queueSpy(handler, &TransferFtpHandler::queueChanged);

        emit mockFtp->error("Connection error");

        QCOMPARE(failedSpy.count(), 0);
        QCOMPARE(queueSpy.count(), 1);
    }

    void testFileRemovedForSingleFileDelete()
    {
        transfer::TransferItem item;
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Delete;
        item.status = transfer::TransferItem::Status::InProgress;
        item.batchId = -1;
        state_.items.append(item);
        state_.currentIndex = 0;
        state_.queueState = transfer::QueueState::Deleting;

        QSignalSpy completedSpy(handler, &TransferFtpHandler::operationCompleted);
        QSignalSpy queueSpy(handler, &TransferFtpHandler::queueChanged);
        QSignalSpy schedSpy(handler, &TransferFtpHandler::scheduleProcessNextRequested);

        emit mockFtp->fileRemoved("/remote/file.txt");

        QVERIFY(completedSpy.count() >= 1);
        QVERIFY(queueSpy.count() >= 1);
        QVERIFY(schedSpy.count() >= 1);
    }

    void testDirectoryCreatedDelegatesToDirCreator()
    {
        state_.queueState = transfer::QueueState::CreatingDirectories;
        state_.totalDirectoriesToCreate = 1;
        state_.directoriesCreated = 0;

        transfer::PendingMkdir mkdir;
        mkdir.remotePath = "/remote/newdir";
        state_.pendingMkdirs.enqueue(mkdir);

        RemoteDirectoryCreator creator(state_, mockFtp, mockFs, this);
        handler->setDirCreator(&creator);

        QSignalSpy spy(&creator, &RemoteDirectoryCreator::allDirectoriesCreated);

        emit mockFtp->directoryCreated("/remote/newdir");

        QCOMPARE(spy.count(), 1);
    }

    void testDirectoryListedRoutedToScanCoordinator()
    {
        state_.requestedListings.insert("/remote/dir");

        RecursiveScanCoordinator coordinator(state_, mockFtp, mockFs, this);
        handler->setScanCoordinator(&coordinator);

        emit mockFtp->directoryListed("/remote/dir", {});

        QVERIFY(!state_.requestedListings.contains("/remote/dir"));
    }

    void testDirectoryListedIgnoredWhenNotHandled()
    {
        RecursiveScanCoordinator coordinator(state_, mockFtp, mockFs, this);
        handler->setScanCoordinator(&coordinator);

        QSignalSpy queueSpy(handler, &TransferFtpHandler::queueChanged);

        emit mockFtp->directoryListed("/remote/unknown", {});

        QCOMPARE(queueSpy.count(), 0);
    }

    // -------------------------------------------------------------------------
    // RemoteDirectoryCreator: null FTP client / null localFs guard tests
    // -------------------------------------------------------------------------

    void testCreateNextDirectory_nullFtpClient_emitsError()
    {
        // Construct creator with null FTP client
        RemoteDirectoryCreator creator(state_, nullptr, mockFs, this);

        transfer::PendingMkdir mkdir;
        mkdir.remotePath = "/remote/newdir";
        state_.pendingMkdirs.enqueue(mkdir);

        QSignalSpy errorSpy(&creator, &RemoteDirectoryCreator::error);

        creator.createNextDirectory();

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(!errorSpy.at(0).at(0).toString().isEmpty());
    }

    void testQueueDirectoriesForUpload_nullLocalFs_onlyQueuesRoot()
    {
        // Construct creator with null localFs — only root should be queued
        RemoteDirectoryCreator creator(state_, mockFtp, nullptr, this);

        creator.queueDirectoriesForUpload("/local/dir", "/remote/dir");

        // Root directory is always queued regardless of localFs being null
        QCOMPARE(state_.pendingMkdirs.size(), 1);
        QCOMPARE(state_.pendingMkdirs.head().remotePath, QString("/remote/dir"));
    }

    // -------------------------------------------------------------------------
    // TransferFtpHandler: null timeoutManager_ guard tests
    // -------------------------------------------------------------------------

    void testStartTimeout_nullTimeoutManager_doesNotCrash()
    {
        // handler has no timeout manager set
        TransferFtpHandler handlerNoTimeout(state_, this);
        handlerNoTimeout.setFtpClient(mockFtp);
        // No setTimeoutManager() call — timeoutManager_ stays null

        // Trigger startTimeout indirectly via uploadProgress
        transfer::TransferItem item;
        item.localPath = "/local/file.txt";
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Upload;
        item.status = transfer::TransferItem::Status::InProgress;
        state_.items.append(item);
        state_.currentIndex = 0;

        // Should not crash even though timeoutManager_ is null
        emit mockFtp->uploadProgress("/local/file.txt", 100, 200);

        QCOMPARE(state_.items[0].bytesTransferred, qint64(100));
    }

    void testStopTimeout_nullTimeoutManager_doesNotCrash()
    {
        // handler has no timeout manager set
        TransferFtpHandler handlerNoTimeout(state_, this);
        handlerNoTimeout.setFtpClient(mockFtp);
        // No setTimeoutManager() call — timeoutManager_ stays null

        transfer::TransferItem item;
        item.localPath = "/local/file.txt";
        item.remotePath = "/remote/file.txt";
        item.operationType = transfer::OperationType::Upload;
        item.status = transfer::TransferItem::Status::InProgress;
        item.batchId = -1;
        state_.items.append(item);
        state_.currentIndex = 0;
        state_.queueState = transfer::QueueState::Transferring;

        QSignalSpy completedSpy(&handlerNoTimeout, &TransferFtpHandler::operationCompleted);

        // uploadFinished calls stopTimeout() — should not crash with null manager
        emit mockFtp->uploadFinished("/local/file.txt", "/remote/file.txt");

        QCOMPARE(completedSpy.count(), 1);
    }
};

QTEST_MAIN(TestTransferFtpHandler)
#include "test_transferftphandler.moc"
