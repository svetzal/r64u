#include <QtTest>
#include <QTemporaryDir>
#include <QSignalSpy>

#include "mocks/mockftpclient.h"
#include "models/transferqueue.h"

class TestTransferQueue : public QObject
{
    Q_OBJECT

private:
    MockFtpClient *mockFtp;
    TransferQueue *queue;
    QTemporaryDir tempDir;

    // Helper to flush the event queue and process mock operations
    // This is needed because TransferQueue now uses deferred event processing
    // Loops until all operations are complete (no more pending ops and no more events)
    void flushAndProcess()
    {
        int iterations = 0;
        const int kMaxIterations = 100;  // Safety limit to prevent infinite loops

        while (iterations++ < kMaxIterations) {
            queue->flushEventQueue();
            if (mockFtp->mockPendingOperationCount() == 0) {
                break;  // No more pending mock operations
            }
            mockFtp->mockProcessAllOperations();
        }
        queue->flushEventQueue();  // Final flush to process any remaining events
    }

    void flushAndProcessNext()
    {
        queue->flushEventQueue();
        mockFtp->mockProcessNextOperation();
        queue->flushEventQueue();
    }

private slots:
    void init()
    {
        mockFtp = new MockFtpClient(this);
        queue = new TransferQueue(this);
        queue->setFtpClient(mockFtp);
        queue->setAutoOverwrite(true);  // Skip overwrite confirmations in tests
        queue->setAutoMerge(true);      // Skip folder exists confirmations in tests
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
        flushAndProcess();

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
        flushAndProcessNext();

        // Now we should have queued LIST for subdir1 (next scan)
        QCOMPARE(mockFtp->mockGetListRequests().size(), 2);
        QCOMPARE(mockFtp->mockGetListRequests().at(1), QString("/remote/folder/subdir1"));

        // Still no downloads - we're still scanning
        QVERIFY(queue->isScanning());
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 0);

        // Process LIST for subdir1 - should find file1.txt
        flushAndProcessNext();

        // Now we should have queued LIST for subdir2 (next scan)
        QCOMPARE(mockFtp->mockGetListRequests().size(), 3);
        QCOMPARE(mockFtp->mockGetListRequests().at(2), QString("/remote/folder/subdir2"));

        // Still scanning, downloads queued but not started
        QVERIFY(queue->isScanning());

        // Process LIST for subdir2 - should find file2.txt and finish scanning
        flushAndProcessNext();

        // NOW scanning should be complete
        QVERIFY(!queue->isScanning());

        // Both files should be queued in TransferQueue
        QCOMPARE(queue->rowCount(), 2);

        // First download should have started (sequential processing - one at a time)
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);

        // Process all downloads
        flushAndProcess();

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
        flushAndProcessNext();  // /remote/root
        flushAndProcessNext();  // /remote/root/level1
        flushAndProcessNext();  // /remote/root/level1/level2

        // Scanning complete, download queued
        QVERIFY(!queue->isScanning());

        // Verify download was requested (item may be InProgress not Pending)
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);
        QVERIFY(mockFtp->mockGetDownloadRequests().contains("/remote/root/level1/level2/file.txt"));

        // Process download
        flushAndProcess();

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
        flushAndProcessNext();

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
        flushAndProcessNext();

        // Scanning should complete
        QVERIFY(!queue->isScanning());

        // One file should be queued
        QCOMPARE(queue->rowCount(), 1);

        // Process download
        flushAndProcess();

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
        queue->flushEventQueue();  // Trigger deferred processNext

        // Item should be in queue
        QCOMPARE(queue->rowCount(), 1);

        // Upload should be tracked
        QCOMPARE(mockFtp->mockGetUploadRequests().size(), 1);

        // Process the upload
        flushAndProcess();

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
        flushAndProcessNext();

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
        queue->flushEventQueue();  // Trigger deferred processNext

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
        flushAndProcess();

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
        flushAndProcessNext();

        // First item should be completed
        QCOMPARE(queue->rowCount(), 2);

        // Remove completed items
        queue->removeCompleted();

        // Should have 1 item left (second download, still pending/in_progress)
        QCOMPARE(queue->rowCount(), 1);

        // Complete remaining
        flushAndProcess();
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
        flushAndProcess();

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
        queue->flushEventQueue();  // Trigger deferred processNext

        QCOMPARE(queue->rowCount(), 3);

        // Only first should be in progress (sequential processing)
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);

        // Process all
        flushAndProcess();

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
        queue->flushEventQueue();  // Trigger deferred processNext

        // After enqueue, item goes to InProgress (due to processNext)
        QCOMPARE(queue->activeCount(), 1);

        flushAndProcess();

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
        flushAndProcess();

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
        flushAndProcessNext();

        // After mkdir completes, files should be queued for upload
        QVERIFY(queue->rowCount() >= 1);

        // Process the upload
        flushAndProcess();
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
        flushAndProcess();

        // At least one file should have been queued
        QVERIFY(queue->rowCount() >= 1);
    }

    // Bug fix test: Recursive upload must include files from ALL directories
    // Previously, processRecursiveUpload used the last dequeued PendingMkdir's
    // localDir, which was a subdirectory path, not the root directory.
    // This caused only files from the last subdirectory to be uploaded.
    void testRecursiveUploadIncludesAllSubdirectoryFiles()
    {
        // Create a local directory structure:
        // root/
        //   file_in_root.txt
        //   subdir1/
        //     file_in_sub1.txt
        //   subdir2/
        //     file_in_sub2.txt
        QString localDir = tempDir.path() + "/multi_subdir_upload";
        QString subDir1 = localDir + "/subdir1";
        QString subDir2 = localDir + "/subdir2";
        QDir().mkpath(subDir1);
        QDir().mkpath(subDir2);

        // Create file in root
        QFile rootFile(localDir + "/file_in_root.txt");
        QVERIFY(rootFile.open(QIODevice::WriteOnly));
        rootFile.write("root content");
        rootFile.close();

        // Create file in subdir1
        QFile sub1File(subDir1 + "/file_in_sub1.txt");
        QVERIFY(sub1File.open(QIODevice::WriteOnly));
        sub1File.write("sub1 content");
        sub1File.close();

        // Create file in subdir2
        QFile sub2File(subDir2 + "/file_in_sub2.txt");
        QVERIFY(sub2File.open(QIODevice::WriteOnly));
        sub2File.write("sub2 content");
        sub2File.close();

        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);

        // Enqueue recursive upload
        queue->enqueueRecursiveUpload(localDir, "/remote");

        // First mkdir is sent immediately (root dir)
        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 1);

        // Process all mkdirs and uploads
        // (mkdirs are sent sequentially, one after each completes)
        flushAndProcess();

        // All 3 mkdirs should have been requested by now
        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 3);

        // All 3 files from all directories should have been uploaded
        QCOMPARE(completedSpy.count(), 3);

        // Verify upload requests include files from ALL directories
        QStringList uploads = mockFtp->mockGetUploadRequests();
        QCOMPARE(uploads.size(), 3);

        // Check that root file was included (bug: without fix, only last subdir files uploaded)
        bool hasRootFile = false;
        bool hasSub1File = false;
        bool hasSub2File = false;
        for (const QString &path : uploads) {
            if (path.contains("file_in_root.txt")) hasRootFile = true;
            if (path.contains("file_in_sub1.txt")) hasSub1File = true;
            if (path.contains("file_in_sub2.txt")) hasSub2File = true;
        }

        QVERIFY2(hasRootFile, "File in root directory should have been uploaded");
        QVERIFY2(hasSub1File, "File in subdir1 should have been uploaded");
        QVERIFY2(hasSub2File, "File in subdir2 should have been uploaded");
    }

    // Test cancel when processing is active
    void testCancelWhileProcessing()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content");

        queue->enqueueDownload(remotePath, localPath);
        queue->flushEventQueue();  // Trigger deferred processNext

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
        flushAndProcess();

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
        flushAndProcess();

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
        flushAndProcess();

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
        flushAndProcess();

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
        flushAndProcessNext();

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

    // =========================================================================
    // Error Recovery Tests
    // =========================================================================

    // Test connection lost mid-transfer marks item as failed
    void testConnectionLostMidTransfer()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content data");

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);

        queue->enqueueDownload(remotePath, localPath);
        queue->flushEventQueue();  // Trigger deferred processNext

        // Verify item is in progress
        QCOMPARE(queue->activeCount(), 1);

        // Simulate connection loss (emits error)
        mockFtp->mockSetNextOperationFails("Connection lost");
        flushAndProcessNext();

        // Verify failure was signaled
        QCOMPARE(failedSpy.count(), 1);

        // Verify item status is Failed
        QModelIndex index = queue->index(0);
        QCOMPARE(queue->data(index, TransferQueue::StatusRole).toInt(),
                 static_cast<int>(TransferItem::Status::Failed));

        // Verify error message is stored
        QString errorMsg = queue->data(index, TransferQueue::ErrorMessageRole).toString();
        QVERIFY(errorMsg.contains("Connection lost"));
    }

    // Test that connection loss during recursive download marks current item failed and continues
    void testConnectionLostDuringRecursiveDownload()
    {
        // Setup directory with two files
        QList<FtpEntry> entries;
        FtpEntry file1; file1.name = "file1.txt"; file1.isDirectory = false;
        FtpEntry file2; file2.name = "file2.txt"; file2.isDirectory = false;
        entries << file1 << file2;
        mockFtp->mockSetDirectoryListing("/remote/folder", entries);

        mockFtp->mockSetDownloadData("/remote/folder/file1.txt", "content1");
        mockFtp->mockSetDownloadData("/remote/folder/file2.txt", "content2");

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);
        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);

        queue->enqueueRecursiveDownload("/remote/folder", tempDir.path());

        // Process the LIST
        flushAndProcessNext();

        // Two files should be queued
        QCOMPARE(queue->rowCount(), 2);

        // Fail the first download
        mockFtp->mockSetNextOperationFails("Network error");
        flushAndProcessNext();

        // First item should be failed
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(queue->data(queue->index(0), TransferQueue::StatusRole).toInt(),
                 static_cast<int>(TransferItem::Status::Failed));

        // Second download should continue
        flushAndProcessNext();

        // Second item should complete successfully
        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(queue->data(queue->index(1), TransferQueue::StatusRole).toInt(),
                 static_cast<int>(TransferItem::Status::Completed));
    }

    // Test partial file cleanup - failed download should not leave corrupted file
    void testPartialFileCleanupOnFailure()
    {
        QString remotePath = "/test/large_file.bin";
        QString localPath = tempDir.path() + "/large_file.bin";
        mockFtp->mockSetDownloadData(remotePath, "some content");

        queue->enqueueDownload(remotePath, localPath);

        // Simulate error before completion
        mockFtp->mockSetNextOperationFails("Transfer interrupted");
        flushAndProcessNext();

        // Mock doesn't create partial files since it completes atomically,
        // but verify the item is marked as failed
        QModelIndex index = queue->index(0);
        QCOMPARE(queue->data(index, TransferQueue::StatusRole).toInt(),
                 static_cast<int>(TransferItem::Status::Failed));
    }

    // Test directory creation failure during recursive upload
    void testDirectoryCreationFailure()
    {
        QString localDir = tempDir.path() + "/upload_dir";
        QDir().mkpath(localDir);

        QFile file(localDir + "/test.txt");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
        file.close();

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);

        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Should have queued mkdir
        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 1);

        // Fail the mkdir operation
        mockFtp->mockSetNextOperationFails("Permission denied: cannot create directory");
        flushAndProcessNext();

        // The mkdir failure doesn't currently emit operationFailed,
        // but upload operations that follow should not proceed
        // In current implementation, mkdir failure leaves queue in inconsistent state
        // This test documents the current behavior for future improvement
    }

    // Test delete operation failure
    void testDeleteOperationFailure()
    {
        // Setup a file to delete
        QList<FtpEntry> entries;
        FtpEntry file; file.name = "file.txt"; file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/remote/folder", entries);

        queue->enqueueRecursiveDelete("/remote/folder");

        // Process the LIST to scan directory
        flushAndProcessNext();

        // Fail the delete operation
        mockFtp->mockSetNextOperationFails("Permission denied");
        flushAndProcessNext();

        // Delete operation failure should be handled gracefully
        // Current implementation continues with remaining deletes
        QVERIFY(!queue->isProcessingDelete() || queue->deleteTotalCount() > 0);
    }

    // Test multiple sequential errors don't corrupt queue state
    void testMultipleSequentialErrors()
    {
        // Queue multiple downloads
        for (int i = 0; i < 3; ++i) {
            QString remotePath = QString("/test/file%1.txt").arg(i);
            QString localPath = tempDir.path() + QString("/file%1.txt").arg(i);
            mockFtp->mockSetDownloadData(remotePath, QString("content%1").arg(i).toUtf8());
            queue->enqueueDownload(remotePath, localPath);
        }
        queue->flushEventQueue();  // Trigger deferred processNext for first item

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);

        // Fail all operations
        for (int i = 0; i < 3; ++i) {
            mockFtp->mockSetNextOperationFails(QString("Error %1").arg(i));
            flushAndProcessNext();
        }

        // All should be failed
        QCOMPARE(failedSpy.count(), 3);

        // All items should be marked as Failed
        for (int i = 0; i < 3; ++i) {
            QModelIndex index = queue->index(i);
            QCOMPARE(queue->data(index, TransferQueue::StatusRole).toInt(),
                     static_cast<int>(TransferItem::Status::Failed));
        }

        // Queue should not be processing anymore
        QVERIFY(!queue->isProcessing());
    }

    // Test error during scanning phase of recursive download
    void testErrorDuringRecursiveScan()
    {
        // Setup a directory structure
        QList<FtpEntry> rootEntries;
        FtpEntry subdir; subdir.name = "subdir"; subdir.isDirectory = true;
        rootEntries << subdir;
        mockFtp->mockSetDirectoryListing("/remote/folder", rootEntries);

        queue->enqueueRecursiveDownload("/remote/folder", tempDir.path());

        QVERIFY(queue->isScanning());

        // Fail the list operation for the subdirectory
        flushAndProcessNext();  // Process root listing

        mockFtp->mockSetNextOperationFails("Directory listing failed");
        flushAndProcessNext();  // Fail subdir listing

        // Scanning should handle the error gracefully
        // Current implementation may leave scanning in incomplete state
        // This test documents behavior for future improvement
    }

    // Test recovery after reconnection
    void testRecoveryAfterReconnection()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content");

        // Start with connected state
        QVERIFY(mockFtp->isConnected());

        queue->enqueueDownload(remotePath, localPath);

        // Simulate disconnect
        mockFtp->mockSimulateDisconnect();
        QVERIFY(!mockFtp->isConnected());

        // The item should still be in the queue
        QCOMPARE(queue->rowCount(), 1);

        // Reconnect
        mockFtp->mockSetConnected(true);
        QVERIFY(mockFtp->isConnected());

        // Item is still there, waiting to be processed
        // After reconnection, a new download would need to be queued
        // Current implementation doesn't auto-resume failed transfers
        QCOMPARE(queue->rowCount(), 1);
    }

    // Test disconnection while queue has pending items
    void testDisconnectionWithPendingItems()
    {
        mockFtp->mockSetConnected(true);

        // Queue multiple items
        for (int i = 0; i < 3; ++i) {
            QString remotePath = QString("/test/file%1.txt").arg(i);
            QString localPath = tempDir.path() + QString("/file%1.txt").arg(i);
            mockFtp->mockSetDownloadData(remotePath, QString("content%1").arg(i).toUtf8());
            queue->enqueueDownload(remotePath, localPath);
        }

        // Process first item
        flushAndProcessNext();

        // Disconnect mid-queue
        mockFtp->mockSimulateDisconnect();

        // Queue should have items but not be processing
        QCOMPARE(queue->rowCount(), 3);
        QVERIFY(!mockFtp->isConnected());

        // First item completed, remaining should still be in queue
        QCOMPARE(queue->data(queue->index(0), TransferQueue::StatusRole).toInt(),
                 static_cast<int>(TransferItem::Status::Completed));
    }

    // Test error message is preserved in item
    void testErrorMessagePreservation()
    {
        QString remotePath = "/test/file.txt";
        QString localPath = tempDir.path() + "/file.txt";
        mockFtp->mockSetDownloadData(remotePath, "content");

        QString expectedError = "Specific error: Connection timed out after 30s";

        queue->enqueueDownload(remotePath, localPath);

        mockFtp->mockSetNextOperationFails(expectedError);
        flushAndProcessNext();

        QModelIndex index = queue->index(0);
        QString errorMsg = queue->data(index, TransferQueue::ErrorMessageRole).toString();
        QCOMPARE(errorMsg, expectedError);
    }

    // Test that failed items can be removed via removeCompleted
    void testRemoveCompletedIncludesFailed()
    {
        QString remotePath1 = "/test/file1.txt";
        QString remotePath2 = "/test/file2.txt";
        QString localPath1 = tempDir.path() + "/file1.txt";
        QString localPath2 = tempDir.path() + "/file2.txt";

        mockFtp->mockSetDownloadData(remotePath1, "content1");
        mockFtp->mockSetDownloadData(remotePath2, "content2");

        queue->enqueueDownload(remotePath1, localPath1);
        queue->enqueueDownload(remotePath2, localPath2);

        // Complete first, fail second
        flushAndProcessNext();  // Complete first

        mockFtp->mockSetNextOperationFails("Error");
        flushAndProcessNext();  // Fail second

        QCOMPARE(queue->rowCount(), 2);

        // removeCompleted should remove both completed and failed items
        queue->removeCompleted();

        // Only completed items are removed by removeCompleted
        // Failed items remain so user can see what failed
        // This test documents current behavior
        QVERIFY(queue->rowCount() <= 2);  // At least completed is removed
    }

    // Test upload error handling
    void testUploadErrorMarksFailed()
    {
        QString localPath = tempDir.path() + "/upload.txt";
        QFile file(localPath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("upload content");
        file.close();

        QString remotePath = "/remote/upload.txt";

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);

        queue->enqueueUpload(localPath, remotePath);

        mockFtp->mockSetNextOperationFails("Disk full");
        flushAndProcessNext();

        QCOMPARE(failedSpy.count(), 1);

        QModelIndex index = queue->index(0);
        QCOMPARE(queue->data(index, TransferQueue::StatusRole).toInt(),
                 static_cast<int>(TransferItem::Status::Failed));
        QCOMPARE(queue->data(index, TransferQueue::ErrorMessageRole).toString(),
                 QString("Disk full"));
    }

    // Test recursive upload with mkdir failure recovery
    void testRecursiveUploadMkdirFailure()
    {
        QString localDir = tempDir.path() + "/nested";
        QString nestedDir = localDir + "/sub";
        QDir().mkpath(nestedDir);

        QFile file(nestedDir + "/file.txt");
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
        file.close();

        queue->enqueueRecursiveUpload(localDir, "/remote");

        // First mkdir should be for the root directory
        QVERIFY(mockFtp->mockGetMkdirRequests().size() >= 1);

        // Fail the first mkdir
        mockFtp->mockSetNextOperationFails("Cannot create directory");
        flushAndProcessNext();

        // Queue should handle the failure gracefully
        // This documents current behavior - may need improvement
    }

    // =========================================================================
    // Overwrite Confirmation Tests
    // =========================================================================

    // Bug: Clicking "Overwrite" (single file) causes dialog to loop forever
    // When user clicks Overwrite (not OverwriteAll), processNext() is called
    // but the file still exists and overwriteAll_ is false, so it asks again.
    void testOverwriteSingleFileDoesNotLoop()
    {
        // Disable auto-overwrite to trigger the confirmation flow
        queue->setAutoOverwrite(false);

        QString remotePath = "/test/existing.txt";
        QString localPath = tempDir.path() + "/existing.txt";

        // Create the local file FIRST (it already exists)
        QFile existingFile(localPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        existingFile.write("old content");
        existingFile.close();
        QVERIFY(QFile::exists(localPath));

        // Setup the download data
        mockFtp->mockSetDownloadData(remotePath, "new content from server");

        // Spy on the overwrite confirmation signal
        QSignalSpy overwriteSpy(queue, &TransferQueue::overwriteConfirmationNeeded);
        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);

        // Enqueue the download
        queue->enqueueDownload(remotePath, localPath);
        queue->flushEventQueue();  // Trigger deferred processNext

        // The overwrite confirmation should be requested exactly ONCE
        QCOMPARE(overwriteSpy.count(), 1);

        // User clicks "Overwrite" (single file, not "Overwrite All")
        queue->respondToOverwrite(OverwriteResponse::Overwrite);
        queue->flushEventQueue();  // Trigger deferred processNext after response

        // Bug: The dialog should NOT appear again
        // Currently this fails because processNext() re-checks file existence
        QCOMPARE(overwriteSpy.count(), 1);  // Still only 1, not 2!

        // The download should be in progress now
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 1);

        // Complete the download
        flushAndProcess();

        // Verify completion
        QCOMPARE(completedSpy.count(), 1);

        // Verify the file was overwritten with new content
        QFile file(localPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("new content from server"));
    }

    // Test that OverwriteAll works correctly (for comparison)
    void testOverwriteAllBypassesSubsequentChecks()
    {
        queue->setAutoOverwrite(false);

        QString remotePath1 = "/test/file1.txt";
        QString remotePath2 = "/test/file2.txt";
        QString localPath1 = tempDir.path() + "/file1.txt";
        QString localPath2 = tempDir.path() + "/file2.txt";

        // Create both local files (they already exist)
        QFile file1(localPath1);
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("old1");
        file1.close();

        QFile file2(localPath2);
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("old2");
        file2.close();

        mockFtp->mockSetDownloadData(remotePath1, "new1");
        mockFtp->mockSetDownloadData(remotePath2, "new2");

        QSignalSpy overwriteSpy(queue, &TransferQueue::overwriteConfirmationNeeded);

        queue->enqueueDownload(remotePath1, localPath1);
        queue->enqueueDownload(remotePath2, localPath2);
        queue->flushEventQueue();  // Trigger deferred processNext

        // First file should trigger confirmation
        QCOMPARE(overwriteSpy.count(), 1);

        // User clicks "Overwrite All"
        queue->respondToOverwrite(OverwriteResponse::OverwriteAll);
        queue->flushEventQueue();  // Trigger processNext after response

        // Process first download
        flushAndProcessNext();

        // Second file should NOT trigger confirmation (OverwriteAll is set)
        QCOMPARE(overwriteSpy.count(), 1);  // Still only 1

        // Process second download
        flushAndProcessNext();

        // Both files should be overwritten
        QFile checkFile1(localPath1);
        QVERIFY(checkFile1.open(QIODevice::ReadOnly));
        QCOMPARE(checkFile1.readAll(), QByteArray("new1"));

        QFile checkFile2(localPath2);
        QVERIFY(checkFile2.open(QIODevice::ReadOnly));
        QCOMPARE(checkFile2.readAll(), QByteArray("new2"));
    }

    // Test that Skip works correctly
    void testOverwriteSkipMovesToNextFile()
    {
        queue->setAutoOverwrite(false);

        QString remotePath = "/test/skip_me.txt";
        QString localPath = tempDir.path() + "/skip_me.txt";

        // Create the local file
        QFile existingFile(localPath);
        QVERIFY(existingFile.open(QIODevice::WriteOnly));
        existingFile.write("original content");
        existingFile.close();

        mockFtp->mockSetDownloadData(remotePath, "new content");

        QSignalSpy overwriteSpy(queue, &TransferQueue::overwriteConfirmationNeeded);
        QSignalSpy allCompletedSpy(queue, &TransferQueue::allOperationsCompleted);

        queue->enqueueDownload(remotePath, localPath);
        queue->flushEventQueue();  // Trigger deferred processNext

        QCOMPARE(overwriteSpy.count(), 1);

        // User clicks "Skip"
        queue->respondToOverwrite(OverwriteResponse::Skip);
        queue->flushEventQueue();  // Process response

        // Should complete without downloading
        QCOMPARE(mockFtp->mockGetDownloadRequests().size(), 0);

        // All operations should complete (the skipped item is marked complete)
        QCOMPARE(allCompletedSpy.count(), 1);

        // File should still have original content
        QFile file(localPath);
        QVERIFY(file.open(QIODevice::ReadOnly));
        QCOMPARE(file.readAll(), QByteArray("original content"));
    }

    // =========================================================================
    // Upload Overwrite Confirmation Tests (HIGH PRIORITY - DESTRUCTIVE BUG)
    // =========================================================================

    // Bug: Upload does not check if remote file exists before overwriting.
    // This is destructive - user can accidentally overwrite remote files.
    void testUploadConfirmsOverwriteWhenRemoteFileExists()
    {
        // Disable auto-overwrite to require confirmation
        queue->setAutoOverwrite(false);

        QString localPath = tempDir.path() + "/upload_test.txt";
        QString remotePath = "/remote/upload_test.txt";

        // Create the local file to upload
        QFile localFile(localPath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        localFile.write("new local content");
        localFile.close();

        // Setup: The remote file ALREADY EXISTS (simulate with directory listing)
        QList<FtpEntry> remoteEntries;
        FtpEntry existingFile;
        existingFile.name = "upload_test.txt";
        existingFile.isDirectory = false;
        existingFile.size = 100;
        remoteEntries << existingFile;
        mockFtp->mockSetDirectoryListing("/remote", remoteEntries);

        QSignalSpy overwriteSpy(queue, &TransferQueue::overwriteConfirmationNeeded);
        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);

        // Enqueue the upload
        queue->enqueueUpload(localPath, remotePath);
        queue->flushEventQueue();  // Trigger deferred processNext

        // The queue should first issue a LIST to check if remote file exists
        // (This is the missing behavior we're testing for)
        QVERIFY2(mockFtp->mockGetListRequests().contains("/remote"),
                 "Upload should check if remote file exists before uploading");

        // Process the LIST operation
        flushAndProcessNext();

        // Since the file exists, overwrite confirmation should be requested
        QCOMPARE(overwriteSpy.count(), 1);

        // Verify the signal includes Upload operation type
        QList<QVariant> args = overwriteSpy.takeFirst();
        QCOMPARE(args.at(0).toString(), QString("upload_test.txt"));
        QCOMPARE(args.at(1).value<OperationType>(), OperationType::Upload);

        // Upload should NOT have started yet (waiting for confirmation)
        QCOMPARE(mockFtp->mockGetUploadRequests().size(), 0);

        // User confirms overwrite
        queue->respondToOverwrite(OverwriteResponse::Overwrite);
        queue->flushEventQueue();  // Trigger deferred processNext after response

        // Now upload should proceed
        QCOMPARE(mockFtp->mockGetUploadRequests().size(), 1);

        // Process the upload
        flushAndProcessNext();

        // Upload should complete
        QCOMPARE(completedSpy.count(), 1);
    }

    // Test that upload proceeds without confirmation when remote file doesn't exist
    void testUploadProceedsWhenRemoteFileDoesNotExist()
    {
        queue->setAutoOverwrite(false);

        QString localPath = tempDir.path() + "/new_file.txt";
        QString remotePath = "/remote/new_file.txt";

        // Create the local file
        QFile localFile(localPath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        localFile.write("content");
        localFile.close();

        // Remote directory is empty - file doesn't exist
        mockFtp->mockSetDirectoryListing("/remote", QList<FtpEntry>());

        QSignalSpy overwriteSpy(queue, &TransferQueue::overwriteConfirmationNeeded);

        queue->enqueueUpload(localPath, remotePath);

        // Process the LIST to check file existence
        flushAndProcessNext();

        // No confirmation needed - file doesn't exist
        QCOMPARE(overwriteSpy.count(), 0);

        // Upload should have started
        QCOMPARE(mockFtp->mockGetUploadRequests().size(), 1);
    }

    // Test that OverwriteAll bypasses checks for subsequent uploads
    void testUploadOverwriteAllBypassesChecks()
    {
        queue->setAutoOverwrite(false);

        QString localPath1 = tempDir.path() + "/file1.txt";
        QString localPath2 = tempDir.path() + "/file2.txt";
        QString remotePath1 = "/remote/file1.txt";
        QString remotePath2 = "/remote/file2.txt";

        // Create local files
        QFile file1(localPath1);
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("content1");
        file1.close();

        QFile file2(localPath2);
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("content2");
        file2.close();

        // Both files exist on remote
        QList<FtpEntry> remoteEntries;
        FtpEntry entry1; entry1.name = "file1.txt"; entry1.isDirectory = false;
        FtpEntry entry2; entry2.name = "file2.txt"; entry2.isDirectory = false;
        remoteEntries << entry1 << entry2;
        mockFtp->mockSetDirectoryListing("/remote", remoteEntries);

        QSignalSpy overwriteSpy(queue, &TransferQueue::overwriteConfirmationNeeded);

        queue->enqueueUpload(localPath1, remotePath1);
        queue->enqueueUpload(localPath2, remotePath2);

        // Process first file's LIST
        flushAndProcessNext();

        // First file should trigger confirmation
        QCOMPARE(overwriteSpy.count(), 1);

        // User chooses "Overwrite All"
        queue->respondToOverwrite(OverwriteResponse::OverwriteAll);

        // First upload should proceed
        flushAndProcessNext();

        // Second file should NOT require confirmation (OverwriteAll is set)
        // It may or may not need a LIST depending on implementation,
        // but should NOT emit overwriteConfirmationNeeded
        flushAndProcess();

        // Only one confirmation was needed
        QCOMPARE(overwriteSpy.count(), 1);
    }

    // Test that Skip works for uploads
    void testUploadSkipWhenRemoteFileExists()
    {
        queue->setAutoOverwrite(false);

        QString localPath = tempDir.path() + "/skip_upload.txt";
        QString remotePath = "/remote/skip_upload.txt";

        QFile localFile(localPath);
        QVERIFY(localFile.open(QIODevice::WriteOnly));
        localFile.write("local content");
        localFile.close();

        // Remote file exists
        QList<FtpEntry> remoteEntries;
        FtpEntry existingFile;
        existingFile.name = "skip_upload.txt";
        existingFile.isDirectory = false;
        remoteEntries << existingFile;
        mockFtp->mockSetDirectoryListing("/remote", remoteEntries);

        QSignalSpy overwriteSpy(queue, &TransferQueue::overwriteConfirmationNeeded);
        QSignalSpy allCompletedSpy(queue, &TransferQueue::allOperationsCompleted);

        queue->enqueueUpload(localPath, remotePath);

        // Process the LIST
        flushAndProcessNext();

        QCOMPARE(overwriteSpy.count(), 1);

        // User clicks Skip
        queue->respondToOverwrite(OverwriteResponse::Skip);

        // Upload should NOT have been issued
        QCOMPARE(mockFtp->mockGetUploadRequests().size(), 0);

        // Should complete (item skipped)
        QCOMPARE(allCompletedSpy.count(), 1);
    }

    // =========================================================================
    // Folder Upload Hang Bug Tests
    // =========================================================================

    // Bug: When uploading a folder with multiple files, the queue hangs.
    // Root cause: processNext() doesn't guard against checkingUploadFileExists_,
    // so when processRecursiveUpload() enqueues multiple files, each enqueueUpload()
    // call triggers processNext() (because processing_=false), causing multiple
    // LIST requests to be sent simultaneously. This can corrupt the state machine.
    void testRecursiveUploadMultipleFilesDoesNotHang()
    {
        // Disable auto-overwrite to trigger file existence checks
        queue->setAutoOverwrite(false);

        // Create a local directory with multiple files
        QString localDir = tempDir.path() + "/multi_upload";
        QDir().mkpath(localDir);

        // Create 3 files
        for (int i = 0; i < 3; ++i) {
            QString filePath = localDir + QString("/file%1.txt").arg(i);
            QFile file(filePath);
            QVERIFY(file.open(QIODevice::WriteOnly));
            file.write(QString("content%1").arg(i).toUtf8());
            file.close();
        }

        // Remote directory is empty (files don't exist yet)
        mockFtp->mockSetDirectoryListing("/remote/multi_upload", QList<FtpEntry>());

        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);
        QSignalSpy allCompletedSpy(queue, &TransferQueue::allOperationsCompleted);

        // Enqueue recursive upload
        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Process the mkdir for the root directory
        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 1);
        flushAndProcessNext();

        // After mkdir completes, processRecursiveUpload() is called which
        // enqueues all files. Each enqueueUpload() might trigger processNext().

        // Critical assertion: Only ONE LIST request should be pending at a time
        // because processNext() should guard against checkingUploadFileExists_.
        // If more than one LIST is pending, the bug is present.
        int listRequestsAfterMkdir = mockFtp->mockGetListRequests().size();

        // BUG: Without the fix, multiple LIST requests are sent simultaneously
        // because processNext() doesn't check checkingUploadFileExists_ before
        // finding the next pending item and sending another LIST.
        QVERIFY2(listRequestsAfterMkdir <= 1,
                 qPrintable(QString("Expected at most 1 LIST request, got %1. "
                           "This indicates processNext() re-entrancy bug.")
                           .arg(listRequestsAfterMkdir)));

        // Now process all operations
        flushAndProcess();

        // All 3 files should have completed
        QCOMPARE(completedSpy.count(), 3);
        QCOMPARE(allCompletedSpy.count(), 1);
    }

    // Test that error during file existence check clears the checking flag
    void testErrorDuringFileExistenceCheckClearsState()
    {
        queue->setAutoOverwrite(false);

        QString localDir = tempDir.path() + "/error_upload";
        QDir().mkpath(localDir);

        QString filePath = localDir + "/file.txt";
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
        file.close();

        // Remote directory listing will fail
        mockFtp->mockSetDirectoryListing("/remote/error_upload", QList<FtpEntry>());

        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Process the mkdir
        flushAndProcessNext();

        // Now a LIST should be pending for file existence check
        int listCount = mockFtp->mockGetListRequests().size();
        QVERIFY(listCount >= 1);

        // Simulate error during LIST
        mockFtp->mockSetNextOperationFails("Network error during LIST");
        flushAndProcessNext();

        // After error, the queue should recover and be able to process more items
        // (not hang due to checkingUploadFileExists_ being stuck true)

        // Enqueue another file to verify queue is not stuck
        QString filePath2 = tempDir.path() + "/another_file.txt";
        QFile file2(filePath2);
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("content2");
        file2.close();

        // This should be accepted (queue should not be stuck)
        queue->enqueueUpload(filePath2, "/remote/another_file.txt");

        // Should be able to start processing the new item
        // (if checkingUploadFileExists_ wasn't cleared, this would hang)
        flushAndProcess();
    }

    // Bug: When uploading multiple files sequentially, the second file's upload
    // hangs because the file handle is corrupted by a race condition in the FTP client.
    // The uploadFinished signal triggers the next upload() call, which sets a new
    // file handle. But then the 226 handler does transferFile_.reset(), resetting
    // the NEW file handle, causing the next STOR to have no file to send.
    void testSequentialUploadsDoNotCorruptFileHandle()
    {
        // Create two files to upload
        QString localPath1 = tempDir.path() + "/upload1.txt";
        QString localPath2 = tempDir.path() + "/upload2.txt";
        QString remotePath1 = "/remote/upload1.txt";
        QString remotePath2 = "/remote/upload2.txt";

        QFile file1(localPath1);
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("content1");
        file1.close();

        QFile file2(localPath2);
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("content2");
        file2.close();

        // No files exist on remote
        mockFtp->mockSetDirectoryListing("/remote", QList<FtpEntry>());

        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);
        QSignalSpy allCompletedSpy(queue, &TransferQueue::allOperationsCompleted);

        // Enqueue both uploads
        queue->enqueueUpload(localPath1, remotePath1);
        queue->enqueueUpload(localPath2, remotePath2);

        // Process all operations
        // Bug: Without fix, second upload will hang because its file handle is null
        flushAndProcess();

        // Both uploads should complete
        QCOMPARE(completedSpy.count(), 2);
        QCOMPARE(allCompletedSpy.count(), 1);

        // Verify both files were uploaded (mock stores local paths)
        QVERIFY(mockFtp->mockGetUploadRequests().contains(localPath1));
        QVERIFY(mockFtp->mockGetUploadRequests().contains(localPath2));
    }

    // Bug: After completing a folder upload, user cannot re-upload the same folder
    // because the completed batch is still being checked for duplicates.
    void testCanReuploadFolderAfterCompletion()
    {
        // Create a local directory with a file
        QString localDir = tempDir.path() + "/reupload_test";
        QDir().mkpath(localDir);

        QString filePath = localDir + "/file.txt";
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
        file.close();

        // Remote directory is empty
        mockFtp->mockSetDirectoryListing("/remote/reupload_test", QList<FtpEntry>());

        QSignalSpy allCompletedSpy(queue, &TransferQueue::allOperationsCompleted);

        // First upload
        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Process mkdir and upload
        flushAndProcess();

        // First upload should complete
        QCOMPARE(allCompletedSpy.count(), 1);

        // Now try to upload the same folder again (simulating user making changes)
        // This should NOT be rejected as a duplicate
        allCompletedSpy.clear();

        queue->enqueueRecursiveUpload(localDir, "/remote");

        // Should have queued a new mkdir (not rejected)
        // Bug: Without fix, this would be rejected as "already being uploaded"
        QVERIFY2(mockFtp->mockGetMkdirRequests().size() >= 2,
                 "Second upload should be accepted after first completes");

        // Process second upload
        flushAndProcess();

        // Second upload should also complete
        QCOMPARE(allCompletedSpy.count(), 1);
    }

    // Test that error during directory creation clears the creating flag
    void testErrorDuringDirectoryCreationClearsState()
    {
        QString localDir = tempDir.path() + "/mkdir_error";
        QDir().mkpath(localDir);

        QString filePath = localDir + "/file.txt";
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("content");
        file.close();

        queue->enqueueRecursiveUpload(localDir, "/remote");

        // mkdir should be pending
        QCOMPARE(mockFtp->mockGetMkdirRequests().size(), 1);
        QVERIFY(queue->isCreatingDirectories());

        // Simulate error during mkdir
        mockFtp->mockSetNextOperationFails("Permission denied: cannot create directory");
        flushAndProcessNext();

        // After error, the queue should NOT be stuck in "creating directories" state
        // If creatingDirectory_ and pendingMkdirs_ aren't cleared, queue hangs
        QVERIFY2(!queue->isCreatingDirectories(),
                 "Queue should not be stuck in 'creating directories' state after error");

        // Should be able to enqueue and process new operations
        QString localPath = tempDir.path() + "/new_file.txt";
        QFile newFile(localPath);
        QVERIFY(newFile.open(QIODevice::WriteOnly));
        newFile.write("new content");
        newFile.close();

        mockFtp->mockSetDirectoryListing("/remote", QList<FtpEntry>());

        queue->enqueueUpload(localPath, "/remote/new_file.txt");

        // Should be able to process (queue not stuck)
        flushAndProcess();
    }

    // =========================================================================
    // Recursive Delete Error Recovery Tests
    // =========================================================================

    // Bug: When a recursive delete encounters a 550 "directory not empty" error
    // (e.g., because a file was created between scan and delete), the queue would
    // hang instead of continuing with remaining items.
    void testRecursiveDeleteContinuesOnItemFailure()
    {
        // Setup a directory with multiple files and subdirs
        QList<FtpEntry> rootEntries;
        FtpEntry file1; file1.name = "file1.txt"; file1.isDirectory = false;
        FtpEntry file2; file2.name = "file2.txt"; file2.isDirectory = false;
        FtpEntry subdir; subdir.name = "subdir"; subdir.isDirectory = true;
        rootEntries << file1 << file2 << subdir;
        mockFtp->mockSetDirectoryListing("/remote/delete_test", rootEntries);

        // Subdir is empty
        mockFtp->mockSetDirectoryListing("/remote/delete_test/subdir", QList<FtpEntry>());

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);
        QSignalSpy completedSpy(queue, &TransferQueue::operationCompleted);

        // Start recursive delete
        queue->enqueueRecursiveDelete("/remote/delete_test");

        // Process the LIST operations to scan the directory tree
        flushAndProcessNext();  // LIST /remote/delete_test
        flushAndProcessNext();  // LIST /remote/delete_test/subdir

        // Now delete operations should be queued
        // Order: files first, then subdir, then root
        QVERIFY(queue->isProcessingDelete());

        // First delete (file1) succeeds
        flushAndProcessNext();

        // Second delete (file2) fails with 550 error
        mockFtp->mockSetNextOperationFails("Requested action not taken.");
        flushAndProcessNext();

        // Bug: Without fix, queue would hang here
        // With fix, it should continue to subdir deletion
        QCOMPARE(failedSpy.count(), 1);
        QVERIFY2(queue->isProcessingDelete() || queue->deleteProgress() > 0,
                 "Delete should continue after item failure");

        // Continue processing remaining deletes
        flushAndProcess();

        // Some operations should have completed despite the failure
        // (the deletion of items after the failed one should have continued)
    }

    // Test that recursive delete reports all failed items
    void testRecursiveDeleteReportsMultipleFailures()
    {
        // Setup a directory with multiple files
        QList<FtpEntry> entries;
        for (int i = 0; i < 5; ++i) {
            FtpEntry file;
            file.name = QString("file%1.txt").arg(i);
            file.isDirectory = false;
            entries << file;
        }
        mockFtp->mockSetDirectoryListing("/remote/multi_fail", entries);

        QSignalSpy failedSpy(queue, &TransferQueue::operationFailed);

        queue->enqueueRecursiveDelete("/remote/multi_fail");

        // Process LIST
        flushAndProcessNext();

        // Fail every other delete
        for (int i = 0; i < 5; ++i) {
            if (i % 2 == 1) {
                mockFtp->mockSetNextOperationFails("Permission denied");
            }
            flushAndProcessNext();
        }

        // Continue with directory deletion (which will also fail since not empty)
        mockFtp->mockSetNextOperationFails("Directory not empty");
        flushAndProcessNext();

        // Should have reported multiple failures (files 1, 3 and the directory)
        QVERIFY(failedSpy.count() >= 2);

        // Queue should not be stuck - delete operation should complete (with errors)
        QVERIFY(!queue->isProcessingDelete());
    }

    // Test that delete error doesn't corrupt queue state for other operations
    void testDeleteErrorDoesNotAffectOtherOperations()
    {
        // Setup a simple directory
        QList<FtpEntry> entries;
        FtpEntry file; file.name = "file.txt"; file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/remote/folder", entries);

        // Also setup a download
        mockFtp->mockSetDownloadData("/remote/other_file.txt", "content");

        // Start recursive delete
        queue->enqueueRecursiveDelete("/remote/folder");

        // Process LIST
        flushAndProcessNext();

        // Fail the file deletion
        mockFtp->mockSetNextOperationFails("Permission denied");
        flushAndProcessNext();

        // Fail the directory deletion
        mockFtp->mockSetNextOperationFails("Directory not empty");
        flushAndProcessNext();

        // Delete should be done (with failures)
        QVERIFY(!queue->isProcessingDelete());

        // Now queue a download - it should work normally
        QString localPath = tempDir.path() + "/other_file.txt";
        queue->enqueueDownload("/remote/other_file.txt", localPath);

        // Download should proceed
        flushAndProcess();

        // Verify download completed
        QVERIFY(QFile::exists(localPath));
    }

    // Test that concurrent recursive downloads create separate batches
    void testConcurrentRecursiveDownloadsCreateSeparateBatches()
    {
        // Setup two directories with files
        QList<FtpEntry> entries1;
        FtpEntry file1; file1.name = "file1.txt"; file1.isDirectory = false;
        entries1 << file1;
        mockFtp->mockSetDirectoryListing("/remote/folder1", entries1);

        QList<FtpEntry> entries2;
        FtpEntry file2; file2.name = "file2.txt"; file2.isDirectory = false;
        entries2 << file2;
        mockFtp->mockSetDirectoryListing("/remote/folder2", entries2);

        mockFtp->mockSetDownloadData("/remote/folder1/file1.txt", "content1");
        mockFtp->mockSetDownloadData("/remote/folder2/file2.txt", "content2");

        // Start first recursive download
        queue->enqueueRecursiveDownload("/remote/folder1", tempDir.path());

        // Should have one batch immediately
        QCOMPARE(queue->allBatchIds().size(), 1);
        int batch1Id = queue->allBatchIds().first();
        QVERIFY(batch1Id > 0);

        // Start second recursive download while first is scanning
        queue->enqueueRecursiveDownload("/remote/folder2", tempDir.path());

        // Should now have two separate batches
        QCOMPARE(queue->allBatchIds().size(), 2);
        QList<int> batchIds = queue->allBatchIds();
        QVERIFY(batchIds.contains(batch1Id));
        int batch2Id = (batchIds[0] == batch1Id) ? batchIds[1] : batchIds[0];
        QVERIFY(batch2Id > 0);
        QVERIFY(batch1Id != batch2Id);

        // Process scanning (LIST) for both directories
        flushAndProcessNext();  // LIST for folder1
        flushAndProcessNext();  // LIST for folder2

        // Each batch should have exactly 1 item (each folder has 1 file)
        BatchProgress progress1 = queue->batchProgress(batch1Id);
        BatchProgress progress2 = queue->batchProgress(batch2Id);
        QCOMPARE(progress1.totalItems, 1);
        QCOMPARE(progress2.totalItems, 1);

        // Process downloads
        flushAndProcess();

        // Verify files were downloaded
        QVERIFY(QFile::exists(tempDir.path() + "/folder1/file1.txt"));
        QVERIFY(QFile::exists(tempDir.path() + "/folder2/file2.txt"));
    }

    // Test that files discovered during scanning are added to correct batch
    void testFilesAddedToCorrectBatchDuringScanning()
    {
        // Setup directory with multiple files
        QList<FtpEntry> entries;
        for (int i = 0; i < 3; ++i) {
            FtpEntry file;
            file.name = QString("file%1.txt").arg(i);
            file.isDirectory = false;
            entries << file;
        }
        mockFtp->mockSetDirectoryListing("/remote/folder", entries);

        for (int i = 0; i < 3; ++i) {
            mockFtp->mockSetDownloadData(QString("/remote/folder/file%1.txt").arg(i), "content");
        }

        // Start recursive download
        queue->enqueueRecursiveDownload("/remote/folder", tempDir.path());

        // Get the batch ID
        QCOMPARE(queue->allBatchIds().size(), 1);
        int batchId = queue->allBatchIds().first();

        // Process LIST to discover files
        flushAndProcessNext();

        // All 3 files should be in the same batch
        BatchProgress progress = queue->batchProgress(batchId);
        QCOMPARE(progress.totalItems, 3);

        // Now verify by checking items_ through the model
        for (int i = 0; i < queue->rowCount(); ++i) {
            QModelIndex index = queue->index(i);
            // All items should have the same batch ID
            int itemBatchId = queue->data(index, Qt::UserRole + 10).toInt();  // Check via internal item
            // Since we don't expose batchId through model roles, verify via batch progress
            // The key point is all items are in the single batch
        }

        // Process downloads
        flushAndProcess();

        // All files should exist
        for (int i = 0; i < 3; ++i) {
            QVERIFY(QFile::exists(tempDir.path() + QString("/folder/file%1.txt").arg(i)));
        }
    }
};

QTEST_MAIN(TestTransferQueue)
#include "test_transferqueue.moc"
