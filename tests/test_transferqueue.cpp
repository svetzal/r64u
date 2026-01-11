#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "services/c64uftpclient.h"
#include "models/transferqueue.h"

class TestTransferQueue : public QObject
{
    Q_OBJECT

private:
    C64UFtpClient *mockFtp;
    TransferQueue *queue;
    QTemporaryDir tempDir;

private slots:
    void init()
    {
        mockFtp = new C64UFtpClient(this);
        queue = new TransferQueue(this);
        queue->setFtpClient(mockFtp);
        queue->setAutoOverwrite(true);  // Skip overwrite confirmations in tests
        mockFtp->mockSetConnected(true);
    }

    void cleanup()
    {
        delete queue;
        delete mockFtp;
        queue = nullptr;
        mockFtp = nullptr;
    }

    // Verify mock is properly connected
    void testMockIsConnected()
    {
        // Verify mockFtp is connected
        QVERIFY2(mockFtp->isConnected(), "Mock FTP should be connected after init()");

        // Verify we can call list
        mockFtp->list("/test");
        QCOMPARE(mockFtp->mockGetListRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetListRequests().first(), QString("/test"));
    }

    // Test single file download
    void testSingleFileDownload()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        QByteArray content = "Hello World";

        mockFtp->mockSetDownloadData(remotePath, content);

        queue->enqueueDownload(remotePath, localPath);

        // Item is immediately InProgress (not Pending) because processNext() is called
        QCOMPARE(queue->rowCount(), 1);

        // Process the download
        mockFtp->mockProcessAllOperations();

        // Item is now Completed
        QCOMPARE(queue->rowCount(), 1);  // Still there but completed

        // Verify file was created
        QFile file(localPath);
        QVERIFY(file.exists());
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), content);
    }

    // Test that recursive download scans ALL directories before starting downloads
    void testRecursiveDownloadScansAllDirectoriesFirst()
    {
        // Setup directory structure:
        // /remote/folder/
        //   subdir1/
        //     file1.txt
        //   subdir2/
        //     file2.txt

        QList<FtpEntry> rootEntries;
        FtpEntry subdir1; subdir1.name = "subdir1"; subdir1.isDirectory = true;
        FtpEntry subdir2; subdir2.name = "subdir2"; subdir2.isDirectory = true;
        rootEntries << subdir1 << subdir2;
        mockFtp->mockSetDirectoryListing("/remote/folder", rootEntries);

        QList<FtpEntry> subdir1Entries;
        FtpEntry file1; file1.name = "file1.txt"; file1.isDirectory = false; file1.size = 100;
        subdir1Entries << file1;
        mockFtp->mockSetDirectoryListing("/remote/folder/subdir1", subdir1Entries);

        QList<FtpEntry> subdir2Entries;
        FtpEntry file2; file2.name = "file2.txt"; file2.isDirectory = false; file2.size = 200;
        subdir2Entries << file2;
        mockFtp->mockSetDirectoryListing("/remote/folder/subdir2", subdir2Entries);

        mockFtp->mockSetDownloadData("/remote/folder/subdir1/file1.txt", "content1");
        mockFtp->mockSetDownloadData("/remote/folder/subdir2/file2.txt", "content2");

        // Start recursive download
        queue->enqueueRecursiveDownload("/remote/folder", tempDir.path());

        // At this point, only LIST for /remote/folder should be queued
        QCOMPARE(mockFtp->mockGetListRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetListRequests().first(), QString("/remote/folder"));

        // No downloads should be queued yet - we're in scanning mode
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 0);

        // Process first LIST - should discover 2 subdirs
        mockFtp->mockProcessNextOperation();

        // Now we should have queued LIST for subdir1 (next scan)
        QCOMPARE(mockFtp->mockGetListRequests().size(), 2);
        QCOMPARE(mockFtp->mockGetListRequests().at(1), QString("/remote/folder/subdir1"));

        // Still no downloads - we're still scanning
        QVERIFY(queue->isScanning());
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 0);

        // Process LIST for subdir1 - should find file1.txt
        mockFtp->mockProcessNextOperation();

        // Now we should have queued LIST for subdir2 (next scan)
        QCOMPARE(mockFtp->mockGetListRequests().size(), 3);
        QCOMPARE(mockFtp->mockGetListRequests().at(2), QString("/remote/folder/subdir2"));

        // Still scanning, downloads queued but not started
        QVERIFY(queue->isScanning());

        // Process LIST for subdir2 - should find file2.txt and finish scanning
        mockFtp->mockProcessNextOperation();

        // NOW scanning should be complete
        QVERIFY(!queue->isScanning());

        // Both files should be queued in TransferQueue (downloads happen sequentially)
        QCOMPARE(queue->rowCount(), 2);

        // First download should have started (processNext called after scanning)
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);

        // Process all downloads
        mockFtp->mockProcessAllOperations();

        // Verify files were created in correct locations (this is the real success criterion)
        QString file1Path = tempDir.path() + "/folder/subdir1/file1.txt";
        QString file2Path = tempDir.path() + "/folder/subdir2/file2.txt";

        QVERIFY2(QFile::exists(file1Path), qPrintable("file1.txt should exist at " + file1Path));
        QVERIFY2(QFile::exists(file2Path), qPrintable("file2.txt should exist at " + file2Path));

        QFile f1(file1Path);
        f1.open(QIODevice::ReadOnly);
        QCOMPARE(f1.readAll(), QByteArray("content1"));

        QFile f2(file2Path);
        f2.open(QIODevice::ReadOnly);
        QCOMPARE(f2.readAll(), QByteArray("content2"));
    }

    // Test with deeper nesting - 3 levels
    void testRecursiveDownloadDeepNesting()
    {
        // /remote/root/
        //   level1/
        //     level2/
        //       file.txt

        QList<FtpEntry> rootEntries;
        FtpEntry level1; level1.name = "level1"; level1.isDirectory = true;
        rootEntries << level1;
        mockFtp->mockSetDirectoryListing("/remote/root", rootEntries);

        QList<FtpEntry> level1Entries;
        FtpEntry level2; level2.name = "level2"; level2.isDirectory = true;
        level1Entries << level2;
        mockFtp->mockSetDirectoryListing("/remote/root/level1", level1Entries);

        QList<FtpEntry> level2Entries;
        FtpEntry file; file.name = "file.txt"; file.isDirectory = false;
        level2Entries << file;
        mockFtp->mockSetDirectoryListing("/remote/root/level1/level2", level2Entries);

        mockFtp->mockSetDownloadData("/remote/root/level1/level2/file.txt", "deep content");

        queue->enqueueRecursiveDownload("/remote/root", tempDir.path());

        // Process all LIST operations
        mockFtp->mockProcessNextOperation();  // /remote/root
        mockFtp->mockProcessNextOperation();  // /remote/root/level1
        mockFtp->mockProcessNextOperation();  // /remote/root/level1/level2

        // Scanning complete, download queued
        QVERIFY(!queue->isScanning());

        // Verify download was requested (item may be InProgress not Pending)
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);
        QVERIFY(mockFtp->mockGetDownloadRequests().contains("/remote/root/level1/level2/file.txt"));

        // Process download
        mockFtp->mockProcessAllOperations();

        QString filePath = tempDir.path() + "/root/level1/level2/file.txt";
        QVERIFY2(QFile::exists(filePath), qPrintable("File should exist at " + filePath));

        QFile f(filePath);
        f.open(QIODevice::ReadOnly);
        QCOMPARE(f.readAll(), QByteArray("deep content"));
    }

    // Test cancellation during scanning
    void testCancelDuringScanning()
    {
        QList<FtpEntry> entries;
        FtpEntry subdir; subdir.name = "subdir"; subdir.isDirectory = true;
        entries << subdir;
        mockFtp->mockSetDirectoryListing("/remote/folder", entries);

        queue->enqueueRecursiveDownload("/remote/folder", tempDir.path());

        QVERIFY(queue->isScanning());

        queue->cancelAll();

        QVERIFY(!queue->isScanning());
        QCOMPARE(queue->pendingCount(), 0);
    }

    // Test clear during scanning
    void testClearDuringScanning()
    {
        QList<FtpEntry> entries;
        FtpEntry subdir; subdir.name = "subdir"; subdir.isDirectory = true;
        entries << subdir;
        mockFtp->mockSetDirectoryListing("/remote/folder", entries);

        queue->enqueueRecursiveDownload("/remote/folder", tempDir.path());

        QVERIFY(queue->isScanning());

        queue->clear();

        QVERIFY(!queue->isScanning());
        QCOMPARE(queue->rowCount(), 0);
    }

    // Test empty directory
    void testRecursiveDownloadEmptyDirectory()
    {
        mockFtp->mockSetDirectoryListing("/remote/empty", QList<FtpEntry>());

        queue->enqueueRecursiveDownload("/remote/empty", tempDir.path());

        // Process the LIST
        mockFtp->mockProcessNextOperation();

        // Should complete with no downloads
        QVERIFY(!queue->isScanning());
        QCOMPARE(queue->pendingCount(), 0);

        // Directory should have been created
        QVERIFY(QDir(tempDir.path() + "/empty").exists());
    }

    // Test that trailing slashes in remote path are handled correctly
    void testRecursiveDownloadTrailingSlash()
    {
        // Setup directory with trailing slash in path
        QList<FtpEntry> entries;
        FtpEntry file; file.name = "test.txt"; file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/remote/folder", entries);  // Server returns without trailing slash
        mockFtp->mockSetDownloadData("/remote/folder/test.txt", "test content");

        // Request with trailing slash
        queue->enqueueRecursiveDownload("/remote/folder/", tempDir.path());

        // Process the LIST
        mockFtp->mockProcessNextOperation();

        // Scanning should complete
        QVERIFY(!queue->isScanning());

        // One file should be queued
        QCOMPARE(queue->rowCount(), 1);

        // Process download
        mockFtp->mockProcessAllOperations();

        // File should be in correct location (folder/test.txt, not just test.txt)
        QString filePath = tempDir.path() + "/folder/test.txt";
        QVERIFY2(QFile::exists(filePath), qPrintable("File should exist at " + filePath));

        QFile f(filePath);
        QVERIFY(f.open(QIODevice::ReadOnly));
        QCOMPARE(f.readAll(), QByteArray("test content"));
    }

    // Test single file upload
    void testSingleFileUpload()
    {
        // Create a local file to upload
        QString localPath = tempDir.path() + "/upload_test.txt";
        QFile file(localPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("Upload content");
        file.close();

        QString remotePath = "/remote/upload_test.txt";

        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);
        QSignalSpy startedSpy(queue, &TransferQueue::operationStarted);

        queue->enqueueUpload(localPath, remotePath);

        // Item should be in queue
        QCOMPARE(queue->rowCount(), 1);

        // Upload should be tracked
        QCOMPARE(mockFtp->mockGetUploadRequests().size(), 1);

        // Process the upload
        mockFtp->mockProcessAllOperations();

        // Verify signals
        QCOMPARE(startedSpy.count(), 1);
        QCOMPARE(completedSpy.count(), 1);
    }

    // Test error handling during download
    void testDownloadError()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);

        queue->enqueueDownload(remotePath, localPath);

        // Simulate error on next operation
        mockFtp->mockSetNextOperationFails("Network error");
        mockFtp->mockProcessNextOperation();

        // Verify failure signal was emitted
        QCOMPARE(failedSpy.count(), 1);
    }

    // Test model data() function
    void testModelData()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "test");

        queue->enqueueDownload(remotePath, localPath);

        QModelIndex index = queue->index(0);
        QVERIFY(index.isValid());

        // Test FileNameRole
        QVariant fileName = queue->data(index, TransferQueue::FileNameRole);
        QCOMPARE(fileName.toString(), QString("file.txt"));

        // Test LocalPathRole
        QVariant local = queue->data(index, TransferQueue::LocalPathRole);
        QCOMPARE(local.toString(), localPath);

        // Test RemotePathRole
        QVariant remote = queue->data(index, TransferQueue::RemotePathRole);
        QCOMPARE(remote.toString(), remotePath);

        // Test DirectionRole
        QVariant dir = queue->data(index, TransferQueue::OperationTypeRole);
        QCOMPARE(dir.toInt(), static_cast<int>(OperationType::Download));

        // Test StatusRole (should be InProgress since processNext was called)
        QVariant status = queue->data(index, TransferQueue::StatusRole);
        QCOMPARE(status.toInt(), static_cast<int>(TransferItem::Status::InProgress));

        // Process the download
        mockFtp->mockProcessAllOperations();

        // Test StatusRole after completion
        status = queue->data(index, TransferQueue::StatusRole);
        QCOMPARE(status.toInt(), static_cast<int>(TransferItem::Status::Completed));
    }

    // Test removeCompleted()
    void testRemoveCompleted()
    {
        QString remotePath1 = "/test/file1.txt";
        QString remotePath2 = "/test/file2.txt";
        QString localPath1 = tempDir.path() + "/file1.txt";
        QString localPath2 = tempDir.path() + "/file2.txt";

        mockFtp->mockSetDownloadData(remotePath1, "content1");
        mockFtp->mockSetDownloadData(remotePath2, "content2");

        queue->enqueueDownload(remotePath1, localPath1);
        queue->enqueueDownload(remotePath2, localPath2);

        QCOMPARE(queue->rowCount(), 2);

        // Process first download only
        mockFtp->mockProcessNextOperation();

        // First item should be completed
        QCOMPARE(queue->rowCount(), 2);

        // Remove completed items
        queue->removeCompleted();

        // Should have 1 item left (second download, still pending/in_progress)
        QCOMPARE(queue->rowCount(), 1);

        // Complete remaining
        mockFtp->mockProcessAllOperations();
        queue->removeCompleted();

        // All completed items should be removed
        QCOMPARE(queue->rowCount(), 0);
    }

    // Test allTransfersCompleted signal
    void testAllTransfersCompletedSignal()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content");

        QSignalSpy allCompletedSpy(queue, &TransferQueue::allOperationsCompleted);

        queue->enqueueDownload(remotePath, localPath);
        mockFtp->mockProcessAllOperations();

        // Signal should be emitted when last transfer completes
        QCOMPARE(allCompletedSpy.count(), 1);
    }

    // Test multiple sequential downloads
    void testMultipleSequentialDownloads()
    {
        for (int i = 0; i < 3; ++i) {
            QString remotePath = QString("/test/file%1.txt").arg(i);
            QString localPath = tempDir.path() + QString("/file%1.txt").arg(i);
            mockFtp->mockSetDownloadData(remotePath, QString("content%1").arg(i).toUtf8());
            queue->enqueueDownload(remotePath, localPath);
        }

        QCOMPARE(queue->rowCount(), 3);

        // Only first should be in progress (sequential processing)
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);

        // Process all
        mockFtp->mockProcessAllOperations();

        // All should have been requested
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 3);

        // All files should exist
        for (int i = 0; i < 3; ++i) {
            QString localPath = tempDir.path() + QString("/file%1.txt").arg(i);
            QVERIFY2(QFile::exists(localPath), qPrintable("File should exist: " + localPath));
        }
    }

    // Test pendingCount() and activeCount()
    void testCountMethods()
    {
        QCOMPARE(queue->pendingCount(), 0);
        QCOMPARE(queue->activeCount(), 0);

        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content");

        queue->enqueueDownload(remotePath, localPath);

        // After enqueue, item goes to InProgress (due to processNext)
        QCOMPARE(queue->activeCount(), 1);

        mockFtp->mockProcessAllOperations();

        // After completion
        QCOMPARE(queue->pendingCount(), 0);
        QCOMPARE(queue->activeCount(), 0);
    }

    // Test roleNames()
    void testRoleNames()
    {
        QHash<int, QByteArray> roles = queue->roleNames();

        QVERIFY(roles.contains(TransferQueue::LocalPathRole));
        QVERIFY(roles.contains(TransferQueue::RemotePathRole));
        QVERIFY(roles.contains(TransferQueue::OperationTypeRole));
        QVERIFY(roles.contains(TransferQueue::StatusRole));
        QVERIFY(roles.contains(TransferQueue::ProgressRole));
        QVERIFY(roles.contains(TransferQueue::FileNameRole));
    }

    // Test data() with invalid index
    void testDataInvalidIndex()
    {
        QModelIndex invalid;
        QVariant result = queue->data(invalid, TransferQueue::FileNameRole);
        QVERIFY(!result.isValid());
    }

    // Test progress reporting
    void testProgressReporting()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content data for progress test");

        QSignalSpy dataChangedSpy(queue, &TransferQueue::dataChanged);

        queue->enqueueDownload(remotePath, localPath);
        mockFtp->mockProcessAllOperations();

        // dataChanged signal should have been emitted for progress updates
        QVERIFY(dataChangedSpy.count() >= 1);
    }

    // Test disconnected state
    void testDisconnectedState()
    {
        mockFtp->mockSetConnected(false);

        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";

        queue->enqueueDownload(remotePath, localPath);

        // Item should be queued but not processing
        QCOMPARE(queue->rowCount(), 1);
        QVERIFY(!queue->isProcessing());
    }

    // Test setFtpClient with null
    void testSetFtpClientNull()
    {
        queue->setFtpClient(nullptr);

        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";

        queue->enqueueDownload(remotePath, localPath);

        // Should handle gracefully
        QCOMPARE(queue->rowCount(), 1);
    }

    // Test recursive upload with directory creation
    void testRecursiveUpload()
    {
        // Create a local directory structure to upload
        QString localDir = tempDir.path() + "/upload_dir";
        QDir().mkpath(localDir);

        // Create a file in the directory
        QString filePath = localDir + "/testfile.txt";
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("upload content");
        file.close();

        // Enqueue recursive upload
        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Should have queued mkdir for root directory
        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetMkdirRequests().first(), QString("/remote/upload_dir"));

        // Process the mkdir - this should trigger onDirectoryCreated
        mockFtp->mockProcessNextOperation();

        // After mkdir completes, files should be queued for upload
        QVERIFY(queue->rowCount() >= 1);

        // Process the upload
        mockFtp->mockProcessAllOperations();
    }

    // Test recursive upload with nested subdirectories
    void testRecursiveUploadWithSubdir()
    {
        // Create a local directory with subdirectory
        QString localDir = tempDir.path() + "/nested_upload";
        QString subDir = localDir + "/subdir";
        QDir().mkpath(subDir);

        // Create a file in the subdirectory
        QString subFilePath = subDir + "/subfile.txt";
        QFile subFile(subFilePath);
        QVERIFY(subFile.open(QIODevice::WriteOnly));
        subFile.write("sub content");
        subFile.close();

        // Enqueue recursive upload
        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Should have queued mkdir for root directory first
        QVERIFY(mockFtp->mockGetMkdirRequests().size() >= 1);

        // Process all mkdirs and uploads
        mockFtp->mockProcessAllOperations();

        // At least one file should have been queued
        QVERIFY(queue->rowCount() >= 1);
    }

    // Test cancel when processing is active
    void testCancelWhileProcessing()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content");

        queue->enqueueDownload(remotePath, localPath);

        // Item should be in progress (processing started)
        QVERIFY(queue->isProcessing());

        // Cancel while processing
        queue->cancelAll();

        // Processing should stop
        QVERIFY(!queue->isProcessing());

        // Item should be marked as failed
        QModelIndex index = queue->index(0);
        QVariant status = queue->data(index, TransferQueue::StatusRole);
        QCOMPARE(status.toInt(), static_cast<int>(TransferItem::Status::Failed));
    }

    // Test removeCompleted when current item is removed
    void testRemoveCompletedCurrentItem()
    {
        QString remotePath1 = "/test/file1.txt";
        QString localPath1 = tempDir.path() + "/file1.txt";
        mockFtp->mockSetDownloadData(remotePath1, "content1");

        queue->enqueueDownload(remotePath1, localPath1);

        // Process to completion
        mockFtp->mockProcessAllOperations();

        // Verify item is completed
        QModelIndex index = queue->index(0);
        QVariant status = queue->data(index, TransferQueue::StatusRole);
        QCOMPARE(status.toInt(), static_cast<int>(TransferItem::Status::Completed));

        // Remove completed - this tests the currentIndex_ == i branch
        queue->removeCompleted();

        // Queue should be empty
        QCOMPARE(queue->rowCount(), 0);
    }

    // Test upload progress reporting
    void testUploadProgress()
    {
        QString localPath = tempDir.path() + "/upload_progress.txt";
        QFile file(localPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content for progress");
        file.close();

        QString remotePath = "/remote/upload_progress.txt";

        QSignalSpy dataChangedSpy(queue, &TransferQueue::dataChanged);

        queue->enqueueUpload(localPath, remotePath);

        // Process the upload
        mockFtp->mockProcessAllOperations();

        // dataChanged should have been emitted for progress
        QVERIFY(dataChangedSpy.count() >= 1);
    }

    // Test recursive upload when not connected
    void testRecursiveUploadNotConnected()
    {
        mockFtp->mockSetConnected(false);

        QString localDir = tempDir.path() + "/upload_dir";
        QDir().mkpath(localDir);

        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Should not queue any operations
        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 0);
    }

    // Test recursive download when not connected
    void testRecursiveDownloadNotConnected()
    {
        mockFtp->mockSetConnected(false);

        queue->enqueueRecursiveDownload("/remote/folder", tempDir.path());

        // Should not start scanning
        QVERIFY(!queue->isScanning());
        QCOMPARE(mockFtp->mockGetListRequests().size(), 0);
    }

    // Test TotalBytesRole
    void testTotalBytesRole()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "test content here");

        queue->enqueueDownload(remotePath, localPath);
        mockFtp->mockProcessAllOperations();

        QModelIndex index = queue->index(0);
        QVariant totalBytes = queue->data(index, TransferQueue::TotalBytesRole);
        QVERIFY(totalBytes.toLongLong() > 0);
    }

    // Test BytesTransferredRole
    void testBytesTransferredRole()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "test content here");

        queue->enqueueDownload(remotePath, localPath);
        mockFtp->mockProcessAllOperations();

        QModelIndex index = queue->index(0);
        QVariant bytesTransferred = queue->data(index, TransferQueue::BytesTransferredRole);
        QVERIFY(bytesTransferred.toLongLong() > 0);
    }

    // Test upload failure
    void testUploadError()
    {
        QString localPath = tempDir.path() + "/upload_error.txt";
        QFile file(localPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("error test");
        file.close();

        QString remotePath = "/remote/upload_error.txt";

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);

        queue->enqueueUpload(localPath, remotePath);

        // Simulate error on upload
        mockFtp->mockSetNextOperationFails("Upload failed");
        mockFtp->mockProcessNextOperation();

        // Verify failure signal
        QCOMPARE(failedSpy.count(), 1);
    }

    // Test that data returns empty for invalid role
    void testDataInvalidRole()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content");

        queue->enqueueDownload(remotePath, localPath);

        QModelIndex index = queue->index(0);
        QVariant result = queue->data(index, Qt::UserRole + 999);  // Invalid role
        QVERIFY(!result.isValid());
    }
};

QTEST_MAIN(TestTransferQueue)
#include "test_transferqueue.moc"
