#include <QtTest>
#include <QSignalSpy>

#include "mocks/mockftpclient.h"
#include "models/remotefilemodel.h"

class TestRemoteFileModel : public QObject
{
    Q_OBJECT

private:
    MockFtpClient *mockFtp;
    RemoteFileModel *model;

private slots:
    void init()
    {
        mockFtp = new MockFtpClient(this);
        model = new RemoteFileModel(this);
        model->setFtpClient(mockFtp);
        mockFtp->mockSetConnected(true);
    }

    void cleanup()
    {
        delete model;
        delete mockFtp;
        model = nullptr;
        mockFtp = nullptr;
    }

    // === Basic Model Structure Tests ===

    void testInitialState()
    {
        // Model should start with root path "/"
        QCOMPARE(model->rootPath(), QString("/"));

        // Root has no visible children initially
        QCOMPARE(model->rowCount(), 0);

        // 3 columns: Name, Size, Type
        QCOMPARE(model->columnCount(), 3);

        // Invalid index returns root node
        QVERIFY(!model->index(0, 0).isValid());
    }

    void testHeaderData()
    {
        QCOMPARE(model->headerData(0, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Name"));
        QCOMPARE(model->headerData(1, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Size"));
        QCOMPARE(model->headerData(2, Qt::Horizontal, Qt::DisplayRole).toString(), QString("Type"));

        // Vertical orientation returns nothing
        QVERIFY(!model->headerData(0, Qt::Vertical, Qt::DisplayRole).isValid());

        // Non-display role returns nothing
        QVERIFY(!model->headerData(0, Qt::Horizontal, Qt::EditRole).isValid());
    }

    void testSetRootPath()
    {
        QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);

        model->setRootPath("/Usb0");

        QCOMPARE(model->rootPath(), QString("/Usb0"));
        QCOMPARE(resetSpy.count(), 1);
    }

    // === Lazy Fetching Tests ===

    void testCanFetchMoreBeforeFetch()
    {
        // Root is a directory that hasn't been fetched yet
        QVERIFY(model->canFetchMore(QModelIndex()));
    }

    void testFetchMoreTriggersListing()
    {
        QSignalSpy loadingSpy(model, &RemoteFileModel::loadingStarted);

        model->fetchMore(QModelIndex());

        // Should have requested listing for root
        QCOMPARE(mockFtp->mockGetListRequests().size(), 1);
        QCOMPARE(mockFtp->mockGetListRequests().first(), QString("/"));

        // Loading signal should have been emitted
        QCOMPARE(loadingSpy.count(), 1);
        QCOMPARE(loadingSpy.first().first().toString(), QString("/"));
    }

    void testCanFetchMoreWhileFetching()
    {
        model->fetchMore(QModelIndex());

        // While fetching, canFetchMore should return false
        QVERIFY(!model->canFetchMore(QModelIndex()));
    }

    void testCanFetchMoreAfterFetch()
    {
        // Setup mock listing
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        file.size = 1024;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // After fetching, canFetchMore should return false
        QVERIFY(!model->canFetchMore(QModelIndex()));
    }

    // === Directory Listing Population Tests ===

    void testDirectoryListingPopulatesModel()
    {
        QList<FtpEntry> entries;

        FtpEntry dir;
        dir.name = "folder1";
        dir.isDirectory = true;
        entries << dir;

        FtpEntry file;
        file.name = "game.prg";
        file.isDirectory = false;
        file.size = 16384;
        entries << file;

        mockFtp->mockSetDirectoryListing("/", entries);

        QSignalSpy rowsInsertedSpy(model, &QAbstractItemModel::rowsInserted);
        QSignalSpy loadingFinishedSpy(model, &RemoteFileModel::loadingFinished);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Should have 2 children
        QCOMPARE(model->rowCount(), 2);

        // Rows inserted signal
        QCOMPARE(rowsInsertedSpy.count(), 1);

        // Loading finished signal
        QCOMPARE(loadingFinishedSpy.count(), 1);
    }

    void testNestedDirectoryFetching()
    {
        // Root listing
        QList<FtpEntry> rootEntries;
        FtpEntry dir;
        dir.name = "Games";
        dir.isDirectory = true;
        rootEntries << dir;
        mockFtp->mockSetDirectoryListing("/", rootEntries);

        // Subdir listing
        QList<FtpEntry> subEntries;
        FtpEntry file;
        file.name = "tetris.prg";
        file.isDirectory = false;
        file.size = 4096;
        subEntries << file;
        mockFtp->mockSetDirectoryListing("/Games", subEntries);

        // Fetch root
        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QCOMPARE(model->rowCount(), 1);

        // Get Games directory index
        QModelIndex gamesIndex = model->index(0, 0);
        QVERIFY(gamesIndex.isValid());
        QCOMPARE(model->data(gamesIndex, Qt::DisplayRole).toString(), QString("Games"));

        // Games should be fetchable
        QVERIFY(model->canFetchMore(gamesIndex));

        // Fetch Games
        model->fetchMore(gamesIndex);
        mockFtp->mockProcessAllOperations();

        // Games should now have 1 child
        QCOMPARE(model->rowCount(gamesIndex), 1);

        // Get tetris file index
        QModelIndex tetrisIndex = model->index(0, 0, gamesIndex);
        QVERIFY(tetrisIndex.isValid());
        QCOMPARE(model->data(tetrisIndex, Qt::DisplayRole).toString(), QString("tetris.prg"));
    }

    // === Data Roles Tests ===

    void testDisplayRole()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "music.sid";
        file.isDirectory = false;
        file.size = 2048;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->data(idx, Qt::DisplayRole).toString(), QString("music.sid"));

        // Column 1 is size (non-dir only)
        QModelIndex sizeIdx = model->index(0, 1);
        QCOMPARE(model->data(sizeIdx, Qt::DisplayRole).toString(), QString("2048"));

        // Column 2 is type
        QModelIndex typeIdx = model->index(0, 2);
        QCOMPARE(model->data(typeIdx, Qt::DisplayRole).toString(), QString("SID Music"));
    }

    void testFilePathRole()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "game.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->data(idx, RemoteFileModel::FilePathRole).toString(), QString("/game.prg"));
    }

    void testIsDirectoryRole()
    {
        QList<FtpEntry> entries;

        FtpEntry dir;
        dir.name = "folder";
        dir.isDirectory = true;
        entries << dir;

        FtpEntry file;
        file.name = "file.txt";
        file.isDirectory = false;
        entries << file;

        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex dirIdx = model->index(0, 0);
        QModelIndex fileIdx = model->index(1, 0);

        QVERIFY(model->data(dirIdx, RemoteFileModel::IsDirectoryRole).toBool());
        QVERIFY(!model->data(fileIdx, RemoteFileModel::IsDirectoryRole).toBool());
    }

    void testFileSizeRole()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "large.d64";
        file.isDirectory = false;
        file.size = 174848;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->data(idx, RemoteFileModel::FileSizeRole).toLongLong(), 174848LL);
    }

    void testFileTypeRole()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "cartridge.crt";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->data(idx, RemoteFileModel::FileTypeRole).toInt(),
                 static_cast<int>(RemoteFileModel::FileType::Cartridge));
    }

    // === Custom Accessor Methods ===

    void testFilePath()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->filePath(idx), QString("/test.prg"));

        // Invalid index returns root path (nodeFromIndex returns rootNode_)
        QCOMPARE(model->filePath(QModelIndex()), QString("/"));
    }

    void testIsDirectory()
    {
        QList<FtpEntry> entries;
        FtpEntry dir;
        dir.name = "subdir";
        dir.isDirectory = true;
        entries << dir;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QVERIFY(model->isDirectory(idx));

        // Invalid index returns root node, which IS a directory
        QVERIFY(model->isDirectory(QModelIndex()));
    }

    void testFileType()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "game.d64";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->fileType(idx), RemoteFileModel::FileType::DiskImage);

        // Invalid index returns root node, which has type Directory
        QCOMPARE(model->fileType(QModelIndex()), RemoteFileModel::FileType::Directory);
    }

    void testFileSize()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "big.bin";
        file.isDirectory = false;
        file.size = 65536;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->fileSize(idx), 65536LL);

        // Invalid index returns 0
        QCOMPARE(model->fileSize(QModelIndex()), 0LL);
    }

    // === File Type Detection Tests ===

    void testDetectFileType()
    {
        QCOMPARE(RemoteFileModel::detectFileType("music.sid"), RemoteFileModel::FileType::SidMusic);
        QCOMPARE(RemoteFileModel::detectFileType("music.SID"), RemoteFileModel::FileType::SidMusic);
        QCOMPARE(RemoteFileModel::detectFileType("game.psid"), RemoteFileModel::FileType::SidMusic);
        QCOMPARE(RemoteFileModel::detectFileType("game.rsid"), RemoteFileModel::FileType::SidMusic);

        QCOMPARE(RemoteFileModel::detectFileType("tune.mod"), RemoteFileModel::FileType::ModMusic);
        QCOMPARE(RemoteFileModel::detectFileType("tune.xm"), RemoteFileModel::FileType::ModMusic);
        QCOMPARE(RemoteFileModel::detectFileType("tune.s3m"), RemoteFileModel::FileType::ModMusic);
        QCOMPARE(RemoteFileModel::detectFileType("tune.it"), RemoteFileModel::FileType::ModMusic);

        QCOMPARE(RemoteFileModel::detectFileType("game.prg"), RemoteFileModel::FileType::Program);
        QCOMPARE(RemoteFileModel::detectFileType("game.p00"), RemoteFileModel::FileType::Program);

        QCOMPARE(RemoteFileModel::detectFileType("cart.crt"), RemoteFileModel::FileType::Cartridge);

        QCOMPARE(RemoteFileModel::detectFileType("disk.d64"), RemoteFileModel::FileType::DiskImage);
        QCOMPARE(RemoteFileModel::detectFileType("disk.d71"), RemoteFileModel::FileType::DiskImage);
        QCOMPARE(RemoteFileModel::detectFileType("disk.d81"), RemoteFileModel::FileType::DiskImage);
        QCOMPARE(RemoteFileModel::detectFileType("disk.g64"), RemoteFileModel::FileType::DiskImage);
        QCOMPARE(RemoteFileModel::detectFileType("disk.g71"), RemoteFileModel::FileType::DiskImage);

        QCOMPARE(RemoteFileModel::detectFileType("tape.tap"), RemoteFileModel::FileType::TapeImage);
        QCOMPARE(RemoteFileModel::detectFileType("tape.t64"), RemoteFileModel::FileType::TapeImage);

        QCOMPARE(RemoteFileModel::detectFileType("kernal.rom"), RemoteFileModel::FileType::Rom);
        QCOMPARE(RemoteFileModel::detectFileType("kernal.bin"), RemoteFileModel::FileType::Rom);

        QCOMPARE(RemoteFileModel::detectFileType("settings.cfg"), RemoteFileModel::FileType::Config);

        QCOMPARE(RemoteFileModel::detectFileType("readme.txt"), RemoteFileModel::FileType::Unknown);
        QCOMPARE(RemoteFileModel::detectFileType("noextension"), RemoteFileModel::FileType::Unknown);
    }

    void testFileTypeString()
    {
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::Directory), QString("Folder"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::SidMusic), QString("SID Music"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::ModMusic), QString("MOD Music"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::Program), QString("Program"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::Cartridge), QString("Cartridge"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::DiskImage), QString("Disk Image"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::TapeImage), QString("Tape Image"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::Rom), QString("ROM"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::Config), QString("Configuration"));
        QCOMPARE(RemoteFileModel::fileTypeString(RemoteFileModel::FileType::Unknown), QString("File"));
    }

    // === Model Reset and Refresh Tests ===

    void testRefreshResetsModel()
    {
        // Populate model first
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "old.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QCOMPARE(model->rowCount(), 1);

        // Refresh should reset
        QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);
        model->refresh();

        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(model->rowCount(), 0);
        QVERIFY(model->canFetchMore(QModelIndex()));
    }

    void testRefreshSpecificIndex()
    {
        // Setup root with subdir
        QList<FtpEntry> rootEntries;
        FtpEntry dir;
        dir.name = "subdir";
        dir.isDirectory = true;
        rootEntries << dir;
        mockFtp->mockSetDirectoryListing("/", rootEntries);

        // Subdir content
        QList<FtpEntry> subEntries;
        FtpEntry file;
        file.name = "file.prg";
        file.isDirectory = false;
        subEntries << file;
        mockFtp->mockSetDirectoryListing("/subdir", subEntries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex subdirIdx = model->index(0, 0);
        model->fetchMore(subdirIdx);
        mockFtp->mockProcessAllOperations();

        QCOMPARE(model->rowCount(subdirIdx), 1);

        // Refresh subdir - this clears children and immediately triggers fetchMore
        QSignalSpy rowsRemovedSpy(model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy loadingStartedSpy(model, &RemoteFileModel::loadingStarted);
        model->refresh(subdirIdx);

        // Children should be removed
        QCOMPARE(rowsRemovedSpy.count(), 1);
        QCOMPARE(model->rowCount(subdirIdx), 0);

        // refresh() triggers fetchMore internally, so it's already fetching
        // Verify a new list request was issued
        QCOMPARE(loadingStartedSpy.count(), 1);
        QCOMPARE(loadingStartedSpy.first().first().toString(), QString("/subdir"));

        // canFetchMore returns false because it's actively fetching
        QVERIFY(!model->canFetchMore(subdirIdx));

        // After processing, canFetchMore should be false (already fetched)
        mockFtp->mockProcessAllOperations();
        QVERIFY(!model->canFetchMore(subdirIdx));
    }

    void testClear()
    {
        // Populate model
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QCOMPARE(model->rowCount(), 1);

        QSignalSpy resetSpy(model, &QAbstractItemModel::modelReset);
        model->clear();

        QCOMPARE(resetSpy.count(), 1);
        QCOMPARE(model->rowCount(), 0);
        QVERIFY(model->canFetchMore(QModelIndex()));
    }

    // === Tree Hierarchy Tests ===

    void testParentIndex()
    {
        // Setup nested structure
        QList<FtpEntry> rootEntries;
        FtpEntry dir;
        dir.name = "parent";
        dir.isDirectory = true;
        rootEntries << dir;
        mockFtp->mockSetDirectoryListing("/", rootEntries);

        QList<FtpEntry> subEntries;
        FtpEntry file;
        file.name = "child.prg";
        file.isDirectory = false;
        subEntries << file;
        mockFtp->mockSetDirectoryListing("/parent", subEntries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex parentIdx = model->index(0, 0);
        model->fetchMore(parentIdx);
        mockFtp->mockProcessAllOperations();

        QModelIndex childIdx = model->index(0, 0, parentIdx);
        QVERIFY(childIdx.isValid());

        // Child's parent should be parentIdx
        QModelIndex computedParent = model->parent(childIdx);
        QCOMPARE(computedParent, parentIdx);

        // Parent's parent should be invalid (root)
        QVERIFY(!model->parent(parentIdx).isValid());
    }

    void testHasChildren()
    {
        // Unfetched directory should report hasChildren true
        QVERIFY(model->hasChildren(QModelIndex()));

        // After fetching with empty result
        mockFtp->mockSetDirectoryListing("/", QList<FtpEntry>());
        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Empty directory should report no children
        QVERIFY(!model->hasChildren(QModelIndex()));
    }

    void testHasChildrenWithContent()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "file.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Should have children after fetching
        QVERIFY(model->hasChildren(QModelIndex()));
    }

    // === Deduplication Tests ===

    void testIgnoresUnrequestedListings()
    {
        // Fetch the root
        model->fetchMore(QModelIndex());

        // Simulate a listing from TransferQueue (different path)
        // Manually emit directoryListed for a path we didn't request
        QList<FtpEntry> foreignEntries;
        FtpEntry foreign;
        foreign.name = "foreign.prg";
        foreign.isDirectory = false;
        foreignEntries << foreign;

        // This should be ignored - we didn't request /other
        emit mockFtp->directoryListed("/other", foreignEntries);

        // Model should still have 0 rows
        QCOMPARE(model->rowCount(), 0);

        // Now process the actual root listing
        mockFtp->mockSetDirectoryListing("/", QList<FtpEntry>());
        mockFtp->mockProcessAllOperations();

        // Still 0 after processing our empty listing
        QCOMPARE(model->rowCount(), 0);
    }

    // === Error Handling Tests ===

    void testFtpErrorClearsPendingFetches()
    {
        QSignalSpy errorSpy(model, &RemoteFileModel::errorOccurred);

        model->fetchMore(QModelIndex());

        // Simulate error
        emit mockFtp->error("Connection lost");

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(errorSpy.first().first().toString(), QString("Connection lost"));

        // Should be able to fetch again after error
        QVERIFY(model->canFetchMore(QModelIndex()));
    }

    // === Invalid Index Handling ===

    void testDataWithInvalidIndex()
    {
        QVariant result = model->data(QModelIndex(), Qt::DisplayRole);
        QVERIFY(!result.isValid());
    }

    void testFlagsWithInvalidIndex()
    {
        QCOMPARE(model->flags(QModelIndex()), Qt::NoItemFlags);
    }

    void testFlagsWithValidIndex()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        Qt::ItemFlags flags = model->flags(idx);

        QVERIFY(flags.testFlag(Qt::ItemIsEnabled));
        QVERIFY(flags.testFlag(Qt::ItemIsSelectable));
    }

    // === Directory Size Display Test ===

    void testDirectorySizeNotDisplayed()
    {
        QList<FtpEntry> entries;
        FtpEntry dir;
        dir.name = "folder";
        dir.isDirectory = true;
        dir.size = 4096;  // Some directories report size
        entries << dir;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Size column for directory should be empty
        QModelIndex sizeIdx = model->index(0, 1);
        QVERIFY(!model->data(sizeIdx, Qt::DisplayRole).isValid());
    }

    // === Full Path Construction Test ===

    void testFullPathConstruction()
    {
        // Test with custom root path
        model->setRootPath("/Usb0");

        QList<FtpEntry> entries;
        FtpEntry dir;
        dir.name = "Games";
        dir.isDirectory = true;
        entries << dir;
        mockFtp->mockSetDirectoryListing("/Usb0", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QModelIndex idx = model->index(0, 0);
        QCOMPARE(model->filePath(idx), QString("/Usb0/Games"));
    }

    // === Text Alignment Test ===

    void testTextAlignment()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        file.size = 1024;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Size column should be right-aligned
        QModelIndex sizeIdx = model->index(0, 1);
        QVariant alignment = model->data(sizeIdx, Qt::TextAlignmentRole);
        QCOMPARE(alignment.toInt(), static_cast<int>(Qt::AlignRight));

        // Name column should not have explicit alignment
        QModelIndex nameIdx = model->index(0, 0);
        QVERIFY(!model->data(nameIdx, Qt::TextAlignmentRole).isValid());
    }

    // === Cache TTL and Invalidation Tests ===

    void testCacheTtlConfiguration()
    {
        // Default TTL is 30 seconds
        QCOMPARE(model->cacheTtl(), 30);

        // Set custom TTL
        model->setCacheTtl(60);
        QCOMPARE(model->cacheTtl(), 60);

        // Disable TTL
        model->setCacheTtl(0);
        QCOMPARE(model->cacheTtl(), 0);
    }

    void testIsStaleBeforeFetch()
    {
        // Unfetched directory is not stale (it's just unfetched)
        QVERIFY(!model->isStale(QModelIndex()));
    }

    void testIsStaleImmediatelyAfterFetch()
    {
        // Set a long TTL so data is fresh
        model->setCacheTtl(300);

        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Immediately after fetch, should not be stale
        QVERIFY(!model->isStale(QModelIndex()));
    }

    void testIsStaleWithZeroTtl()
    {
        // Disable TTL - data should never be considered stale
        model->setCacheTtl(0);

        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // With TTL disabled, should never be stale
        QVERIFY(!model->isStale(QModelIndex()));
    }

    void testCanFetchMoreWhenStaleEnabled()
    {
        // Set very short TTL (1 second)
        model->setCacheTtl(1);

        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Immediately after fetch, should not be able to fetch more
        QVERIFY(!model->canFetchMore(QModelIndex()));

        // Wait for TTL to expire
        QTest::qWait(1100);  // Wait 1.1 seconds

        // Now should be able to fetch more (stale data)
        QVERIFY(model->canFetchMore(QModelIndex()));
    }

    void testInvalidateCache()
    {
        // Setup with nested structure
        QList<FtpEntry> rootEntries;
        FtpEntry dir;
        dir.name = "Games";
        dir.isDirectory = true;
        rootEntries << dir;
        mockFtp->mockSetDirectoryListing("/", rootEntries);

        QList<FtpEntry> subEntries;
        FtpEntry file;
        file.name = "tetris.prg";
        file.isDirectory = false;
        subEntries << file;
        mockFtp->mockSetDirectoryListing("/Games", subEntries);

        // Fetch root
        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Fetch subdir
        QModelIndex gamesIndex = model->index(0, 0);
        model->fetchMore(gamesIndex);
        mockFtp->mockProcessAllOperations();

        // Both should be fetched (not fetchable)
        QVERIFY(!model->canFetchMore(QModelIndex()));
        QVERIFY(!model->canFetchMore(gamesIndex));

        // Invalidate entire cache
        model->invalidateCache();

        // Both should now be fetchable again
        QVERIFY(model->canFetchMore(QModelIndex()));
        QVERIFY(model->canFetchMore(gamesIndex));
    }

    void testInvalidatePath()
    {
        // Setup with nested structure
        QList<FtpEntry> rootEntries;
        FtpEntry dir;
        dir.name = "Games";
        dir.isDirectory = true;
        rootEntries << dir;
        mockFtp->mockSetDirectoryListing("/", rootEntries);

        QList<FtpEntry> subEntries;
        FtpEntry file;
        file.name = "tetris.prg";
        file.isDirectory = false;
        subEntries << file;
        mockFtp->mockSetDirectoryListing("/Games", subEntries);

        // Fetch root
        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Fetch subdir
        QModelIndex gamesIndex = model->index(0, 0);
        model->fetchMore(gamesIndex);
        mockFtp->mockProcessAllOperations();

        // Both should be fetched
        QVERIFY(!model->canFetchMore(QModelIndex()));
        QVERIFY(!model->canFetchMore(gamesIndex));

        // Invalidate only Games directory
        model->invalidatePath("/Games");

        // Root should still be fetched
        QVERIFY(!model->canFetchMore(QModelIndex()));

        // Games should now be fetchable
        QVERIFY(model->canFetchMore(gamesIndex));
    }

    void testInvalidatePathNonexistent()
    {
        // Invalidating a non-existent path should not crash
        model->invalidatePath("/NonExistent/Path");

        // Model should still work
        QVERIFY(model->canFetchMore(QModelIndex()));
    }

    void testRefreshIfStaleWhenFresh()
    {
        // Set long TTL
        model->setCacheTtl(300);

        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        int listRequestsBefore = mockFtp->mockGetListRequests().size();

        // refreshIfStale should not trigger a refresh when data is fresh
        model->refreshIfStale();

        int listRequestsAfter = mockFtp->mockGetListRequests().size();

        // No new list requests should have been made
        QCOMPARE(listRequestsAfter, listRequestsBefore);
    }

    void testRefreshIfStaleWhenStale()
    {
        // Set very short TTL
        model->setCacheTtl(1);

        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // Wait for TTL to expire
        QTest::qWait(1100);

        int listRequestsBefore = mockFtp->mockGetListRequests().size();

        // refreshIfStale should trigger a refresh when data is stale
        model->refreshIfStale();

        int listRequestsAfter = mockFtp->mockGetListRequests().size();

        // Should not have made additional list request because refresh() does a full reset
        // Actually, refresh() calls setRootPath() which resets the model, not lists
        // So the list count should be the same until fetchMore is called again
        QCOMPARE(listRequestsAfter, listRequestsBefore);

        // But the model should now be fetchable again
        QVERIFY(model->canFetchMore(QModelIndex()));
    }

    void testFetchMoreClearsStaleChildren()
    {
        // Set short TTL
        model->setCacheTtl(1);

        // First fetch with old data
        QList<FtpEntry> oldEntries;
        FtpEntry oldFile;
        oldFile.name = "old.prg";
        oldFile.isDirectory = false;
        oldEntries << oldFile;
        mockFtp->mockSetDirectoryListing("/", oldEntries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->data(model->index(0, 0), Qt::DisplayRole).toString(), QString("old.prg"));

        // Wait for TTL to expire
        QTest::qWait(1100);

        // Update mock with new data
        QList<FtpEntry> newEntries;
        FtpEntry newFile;
        newFile.name = "new.prg";
        newFile.isDirectory = false;
        newEntries << newFile;
        mockFtp->mockSetDirectoryListing("/", newEntries);

        // fetchMore should clear old children and get new ones
        QSignalSpy rowsRemovedSpy(model, &QAbstractItemModel::rowsRemoved);
        QSignalSpy rowsInsertedSpy(model, &QAbstractItemModel::rowsInserted);

        model->fetchMore(QModelIndex());

        // Old children should be removed first
        QCOMPARE(rowsRemovedSpy.count(), 1);

        mockFtp->mockProcessAllOperations();

        // New children should be inserted
        QCOMPARE(rowsInsertedSpy.count(), 1);

        // Model should now have new data
        QCOMPARE(model->rowCount(), 1);
        QCOMPARE(model->data(model->index(0, 0), Qt::DisplayRole).toString(), QString("new.prg"));
    }

    void testClearResetsTimestamp()
    {
        QList<FtpEntry> entries;
        FtpEntry file;
        file.name = "test.prg";
        file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->fetchMore(QModelIndex());
        mockFtp->mockProcessAllOperations();

        // After clear, should be fetchable again
        model->clear();
        QVERIFY(model->canFetchMore(QModelIndex()));

        // And should not be stale (not fetched)
        QVERIFY(!model->isStale(QModelIndex()));
    }
};

QTEST_MAIN(TestRemoteFileModel)
#include "test_remotefilemodel.moc"
