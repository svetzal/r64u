#include "core/transferlistingcore.h"

#include <QTest>

namespace {

FtpEntry makeFile(const QString &name, qint64 size = 0)
{
    FtpEntry e;
    e.name = name;
    e.isDirectory = false;
    e.size = size;
    return e;
}

FtpEntry makeDir(const QString &name)
{
    FtpEntry e;
    e.name = name;
    e.isDirectory = true;
    return e;
}

}  // namespace

class TestTransferListingCore : public QObject
{
    Q_OBJECT

private slots:
    void testSortDeleteQueue_filesBeforeDirectories()
    {
        QList<transfer::DeleteItem> queue;
        transfer::DeleteItem dir;
        dir.path = "/remote/dir";
        dir.isDirectory = true;
        transfer::DeleteItem file;
        file.path = "/remote/dir/file.prg";
        file.isDirectory = false;
        queue.append(dir);
        queue.append(file);

        auto sorted = transfer::sortDeleteQueue(queue);

        QVERIFY(!sorted[0].isDirectory);
        QVERIFY(sorted[1].isDirectory);
    }

    void testSortDeleteQueue_deeperDirFirst()
    {
        QList<transfer::DeleteItem> queue;
        transfer::DeleteItem shallow;
        shallow.path = "/a/b";
        shallow.isDirectory = true;
        transfer::DeleteItem deep;
        deep.path = "/a/b/c";
        deep.isDirectory = true;
        queue.append(shallow);
        queue.append(deep);

        auto sorted = transfer::sortDeleteQueue(queue);

        QCOMPARE(sorted[0].path, QString("/a/b/c"));
        QCOMPARE(sorted[1].path, QString("/a/b"));
    }

    void testSortDeleteQueue_emptyReturnsEmpty()
    {
        QVERIFY(transfer::sortDeleteQueue({}).isEmpty());
    }

    void testProcessDirectoryListingForDownload_fileEntryCreatesDownloadPair()
    {
        transfer::PendingScan scan;
        scan.remotePath = "/remote/dir";
        scan.localBasePath = "/local/base";
        scan.remoteBasePath = "/remote/dir";
        scan.batchId = 1;

        QList<FtpEntry> entries = {makeFile("game.prg", 4096)};
        auto result = transfer::processDirectoryListingForDownload(scan, entries);

        QCOMPARE(result.newFileDownloads.size(), 1);
        QCOMPARE(result.newFileDownloads[0].first, QString("/remote/dir/game.prg"));
        QCOMPARE(result.newFileDownloads[0].second, QString("/local/base/game.prg"));
    }

    void testProcessDirectoryListingForDownload_dirEntryCreatesSubScan()
    {
        transfer::PendingScan scan;
        scan.remotePath = "/remote/dir";
        scan.localBasePath = "/local/base";
        scan.remoteBasePath = "/remote/dir";
        scan.batchId = 1;

        QList<FtpEntry> entries = {makeDir("sub")};
        auto result = transfer::processDirectoryListingForDownload(scan, entries);

        QCOMPARE(result.newSubScans.size(), 1);
        QCOMPARE(result.newSubScans[0].remotePath, QString("/remote/dir/sub"));
        QVERIFY(result.newFileDownloads.isEmpty());
    }

    void testProcessDirectoryListingForDownload_nestedPathConstruction()
    {
        transfer::PendingScan scan;
        scan.remotePath = "/remote/dir/sub";
        scan.localBasePath = "/local/base";
        scan.remoteBasePath = "/remote/dir";
        scan.batchId = 1;

        QList<FtpEntry> entries = {makeFile("nested.prg")};
        auto result = transfer::processDirectoryListingForDownload(scan, entries);

        QCOMPARE(result.newFileDownloads[0].second, QString("/local/base/sub/nested.prg"));
    }

    void testProcessDirectoryListingForDelete_fileEntriesProduceFileItems()
    {
        QList<FtpEntry> entries = {makeFile("file.prg")};
        auto result = transfer::processDirectoryListingForDelete("/remote/dir", entries);

        QCOMPARE(result.fileItems.size(), 1);
        QCOMPARE(result.fileItems[0].path, QString("/remote/dir/file.prg"));
        QVERIFY(!result.fileItems[0].isDirectory);
    }

    void testProcessDirectoryListingForDelete_dirEntriesProduceSubScans()
    {
        QList<FtpEntry> entries = {makeDir("sub")};
        auto result = transfer::processDirectoryListingForDelete("/remote/dir", entries);

        QCOMPARE(result.newSubScans.size(), 1);
        QCOMPARE(result.newSubScans[0].remotePath, QString("/remote/dir/sub"));
    }

    void testProcessDirectoryListingForDelete_alwaysIncludesDirectoryItem()
    {
        auto result = transfer::processDirectoryListingForDelete("/remote/dir", {});

        QCOMPARE(result.directoryItem.path, QString("/remote/dir"));
        QVERIFY(result.directoryItem.isDirectory);
    }

    void testUpdateFolderExistence_marksDestExistsTrueWhenDirFound()
    {
        transfer::State state;
        transfer::PendingFolderOp op;
        op.destPath = "/remote";
        op.targetPath = "/remote/Games";
        op.destExists = false;
        state.pendingFolderOps.enqueue(op);

        QList<FtpEntry> entries = {makeDir("Games")};
        auto newState = transfer::updateFolderExistence(state, "/remote", entries);

        QVERIFY(newState.pendingFolderOps.head().destExists);
    }

    void testUpdateFolderExistence_marksDestExistsFalseWhenDirNotFound()
    {
        transfer::State state;
        transfer::PendingFolderOp op;
        op.destPath = "/remote";
        op.targetPath = "/remote/Games";
        op.destExists = false;
        state.pendingFolderOps.enqueue(op);

        QList<FtpEntry> entries = {makeDir("Other")};
        auto newState = transfer::updateFolderExistence(state, "/remote", entries);

        QVERIFY(!newState.pendingFolderOps.head().destExists);
    }

    void testCheckUploadFileExists_fileFoundTransitionsToAwaitingConfirm()
    {
        transfer::State state;
        state.currentIndex = 0;
        transfer::TransferItem item;
        item.remotePath = "/remote/dir/file.prg";
        item.operationType = transfer::OperationType::Upload;
        state.items.append(item);

        QList<FtpEntry> entries = {makeFile("file.prg")};
        auto result = transfer::checkUploadFileExists(state, entries);

        QVERIFY(result.fileExists);
        QCOMPARE(result.newState.queueState, transfer::QueueState::AwaitingFileConfirm);
    }

    void testCheckUploadFileExists_fileNotFoundConfirmsItem()
    {
        transfer::State state;
        state.currentIndex = 0;
        transfer::TransferItem item;
        item.remotePath = "/remote/dir/file.prg";
        item.operationType = transfer::OperationType::Upload;
        state.items.append(item);

        QList<FtpEntry> entries = {makeFile("other.prg")};
        auto result = transfer::checkUploadFileExists(state, entries);

        QVERIFY(!result.fileExists);
        QVERIFY(result.newState.items[0].confirmed);
    }

    void testCheckUploadFileExists_outOfRangeCurrentIndex_noOp()
    {
        transfer::State state;
        state.currentIndex = -1;

        auto result = transfer::checkUploadFileExists(state, {});

        QVERIFY(!result.fileExists);
        QCOMPARE(result.newState.queueState, transfer::QueueState::Idle);
    }
};

QTEST_MAIN(TestTransferListingCore)
#include "test_transferlistingcore.moc"
