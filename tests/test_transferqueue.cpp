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
};

QTEST_MAIN(TestTransferQueue)
#include "test_transferqueue.moc"
