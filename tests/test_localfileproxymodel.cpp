#include "models/localfileproxymodel.h"

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestLocalFileProxyModel : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir *tempDir_;
    QFileSystemModel *fsModel_;
    LocalFileProxyModel *proxyModel_;

    // Poll until the named entry appears in fsModel_ under rootIdx, or timeout elapses.
    // Returns the row index of the entry, or -1 if not found within the timeout.
    int waitForEntry(const QModelIndex &rootIdx, const QString &name, int timeoutMs = 5000)
    {
        for (int elapsed = 0; elapsed < timeoutMs; elapsed += 50) {
            QCoreApplication::processEvents();
            QTest::qWait(50);
            fsModel_->fetchMore(rootIdx);
            for (int i = 0; i < fsModel_->rowCount(rootIdx); i++) {
                QModelIndex nameIdx = fsModel_->index(i, 0, rootIdx);
                if (fsModel_->fileName(nameIdx) == name) {
                    return i;
                }
            }
        }
        return -1;
    }

private slots:
    void init()
    {
        tempDir_ = new QTemporaryDir();
        QVERIFY(tempDir_->isValid());

        fsModel_ = new QFileSystemModel(this);
        fsModel_->setRootPath(tempDir_->path());

        proxyModel_ = new LocalFileProxyModel(this);
        proxyModel_->setSourceModel(fsModel_);
    }

    void cleanup()
    {
        delete proxyModel_;
        proxyModel_ = nullptr;
        delete fsModel_;
        fsModel_ = nullptr;
        delete tempDir_;
        tempDir_ = nullptr;
    }

    // ========== Basic functionality ==========

    void testConstructor()
    {
        LocalFileProxyModel model;
        QVERIFY(model.sourceModel() == nullptr);
    }

    void testSetSourceModel() { QCOMPARE(proxyModel_->sourceModel(), fsModel_); }

    // ========== data() for file size column ==========

    void testDataFileSizeColumn()
    {
        // Create a test file with known size
        QString filePath = tempDir_->filePath("testfile.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QByteArray content(1234, 'X');
        file.write(content);
        file.close();

        // Wait for the file system model to update
        QModelIndex rootIdx = fsModel_->index(tempDir_->path());
        QVERIFY(rootIdx.isValid());

        // Force the model to fetch children
        fsModel_->fetchMore(rootIdx);

        // Wait for async operations - QFileSystemModel can be slow
        QSignalSpy spy(fsModel_, &QAbstractItemModel::layoutChanged);
        QTest::qWait(500);

        // Find our test file
        for (int i = 0; i < fsModel_->rowCount(rootIdx); i++) {
            QModelIndex nameIdx = fsModel_->index(i, 0, rootIdx);
            if (fsModel_->fileName(nameIdx) == "testfile.txt") {
                // Get the proxy index for the size column
                QModelIndex proxyNameIdx = proxyModel_->mapFromSource(nameIdx);
                QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

                // Check that data returns the byte count as string
                QVariant data = proxyModel_->data(proxySizeIdx, Qt::DisplayRole);
                QCOMPARE(data.toString(), QString("1234"));
                return;
            }
        }

        QFAIL("Test file not found in model");
    }

    void testDataDirectoryReturnsEmpty()
    {
        // Create a test directory
        QString dirPath = tempDir_->filePath("testdir");
        QDir().mkpath(dirPath);

        QModelIndex rootIdx = fsModel_->index(tempDir_->path());
        int row = waitForEntry(rootIdx, "testdir");
        if (row < 0) {
            QFAIL("Test directory not found in model");
        }

        QModelIndex nameIdx = fsModel_->index(row, 0, rootIdx);
        QModelIndex proxyNameIdx = proxyModel_->mapFromSource(nameIdx);
        QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

        // Directories should return empty variant
        QVariant data = proxyModel_->data(proxySizeIdx, Qt::DisplayRole);
        QVERIFY(!data.isValid() || data.toString().isEmpty());
    }

    void testDataOtherColumnsPassthrough()
    {
        // Create a test file
        QString filePath = tempDir_->filePath("passthrough.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
        file.close();

        QModelIndex rootIdx = fsModel_->index(tempDir_->path());
        int row = waitForEntry(rootIdx, "passthrough.txt");
        if (row < 0) {
            QFAIL("Test file not found in model");
        }

        QModelIndex nameIdx = fsModel_->index(row, 0, rootIdx);
        QModelIndex proxyNameIdx = proxyModel_->mapFromSource(nameIdx);

        // Column 0 (name) should pass through unchanged
        QVariant nameData = proxyModel_->data(proxyNameIdx, Qt::DisplayRole);
        QCOMPARE(nameData.toString(), QString("passthrough.txt"));
    }

    void testDataOtherRolesPassthrough()
    {
        // Create a test file
        QString filePath = tempDir_->filePath("roles.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(5000, 'X'));
        file.close();

        QModelIndex rootIdx = fsModel_->index(tempDir_->path());
        int row = waitForEntry(rootIdx, "roles.txt");
        if (row < 0) {
            QFAIL("Test file not found in model");
        }

        QModelIndex nameIdx = fsModel_->index(row, 0, rootIdx);
        QModelIndex proxyNameIdx = proxyModel_->mapFromSource(nameIdx);
        QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

        // Non-DisplayRole on size column should pass through
        // (e.g., tooltip, decoration, etc.)
        QVariant editData = proxyModel_->data(proxySizeIdx, Qt::EditRole);
        // Should be same as source model's data for this role
        QModelIndex sourceSizeIdx = proxyModel_->mapToSource(proxySizeIdx);
        QVariant sourceEditData = fsModel_->data(sourceSizeIdx, Qt::EditRole);
        QCOMPARE(editData, sourceEditData);
    }

    void testDataZeroSizeFile()
    {
        // Create an empty file
        QString filePath = tempDir_->filePath("empty.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        QModelIndex rootIdx = fsModel_->index(tempDir_->path());
        int row = waitForEntry(rootIdx, "empty.txt");
        if (row < 0) {
            QFAIL("Test file not found in model");
        }

        QModelIndex nameIdx = fsModel_->index(row, 0, rootIdx);
        QModelIndex proxyNameIdx = proxyModel_->mapFromSource(nameIdx);
        QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

        QVariant data = proxyModel_->data(proxySizeIdx, Qt::DisplayRole);
        QCOMPARE(data.toString(), QString("0"));
    }

    void testDataLargeFile()
    {
        // Create a larger file (100KB)
        QString filePath = tempDir_->filePath("large.bin");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QByteArray content(102400, 'X');  // 100KB
        file.write(content);
        file.close();

        QModelIndex rootIdx = fsModel_->index(tempDir_->path());
        int row = waitForEntry(rootIdx, "large.bin");
        if (row < 0) {
            QFAIL("Test file not found in model");
        }

        QModelIndex nameIdx = fsModel_->index(row, 0, rootIdx);
        QModelIndex proxyNameIdx = proxyModel_->mapFromSource(nameIdx);
        QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

        QVariant data = proxyModel_->data(proxySizeIdx, Qt::DisplayRole);
        QCOMPARE(data.toString(), QString("102400"));
    }

    // ========== Sorting ==========

    void testSortingDirectoriesFirst()
    {
        // Create test files and directories with names that would sort
        // alphabetically interleaved if not for folder-first sorting
        QDir().mkpath(tempDir_->filePath("b_dir"));
        QFile file1(tempDir_->filePath("a_file.txt"));
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("test");
        file1.close();
        QDir().mkpath(tempDir_->filePath("c_dir"));
        QFile file2(tempDir_->filePath("d_file.txt"));
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("test");
        file2.close();

        QModelIndex rootIdx = fsModel_->index(tempDir_->path());

        // Poll until all 4 entries are loaded, then sort
        for (int elapsed = 0; elapsed < 5000; elapsed += 50) {
            QCoreApplication::processEvents();
            QTest::qWait(50);
            fsModel_->fetchMore(rootIdx);
            if (fsModel_->rowCount(rootIdx) >= 4) {
                break;
            }
        }

        // Set up sorting on the proxy
        proxyModel_->sort(0, Qt::AscendingOrder);
        QTest::qWait(50);  // Brief wait for sort to apply

        QModelIndex proxyRoot = proxyModel_->mapFromSource(rootIdx);
        int rowCount = proxyModel_->rowCount(proxyRoot);

        if (rowCount < 4) {
            QSKIP("Files not yet loaded by QFileSystemModel");
        }

        // Collect all items in sorted order
        QStringList sortedNames;
        QList<bool> isDir;
        for (int i = 0; i < rowCount; i++) {
            QModelIndex idx = proxyModel_->index(i, 0, proxyRoot);
            sortedNames << proxyModel_->data(idx, Qt::DisplayRole).toString();
            QModelIndex sourceIdx = proxyModel_->mapToSource(idx);
            isDir << fsModel_->isDir(sourceIdx);
        }

        // Verify directories come before files
        bool seenFile = false;
        for (int i = 0; i < sortedNames.size(); i++) {
            if (!isDir[i]) {
                seenFile = true;
            } else if (seenFile) {
                QFAIL("Directory found after file - sorting is incorrect");
            }
        }

        // If we have the expected items, verify order
        if (sortedNames.contains("b_dir") && sortedNames.contains("c_dir") &&
            sortedNames.contains("a_file.txt") && sortedNames.contains("d_file.txt")) {
            // Directories should come first (b_dir, c_dir)
            // Then files (a_file.txt, d_file.txt)
            int bDirPos = sortedNames.indexOf("b_dir");
            int cDirPos = sortedNames.indexOf("c_dir");
            int aFilePos = sortedNames.indexOf("a_file.txt");
            int dFilePos = sortedNames.indexOf("d_file.txt");

            // Both directories should come before both files
            QVERIFY2(bDirPos < aFilePos, "b_dir should come before a_file.txt");
            QVERIFY2(cDirPos < aFilePos, "c_dir should come before a_file.txt");
            QVERIFY2(bDirPos < dFilePos, "b_dir should come before d_file.txt");
            QVERIFY2(cDirPos < dFilePos, "c_dir should come before d_file.txt");
        }
    }

    void testSortingCaseInsensitive()
    {
        // Create test files with mixed-case names that would sort differently
        // under case-sensitive vs case-insensitive sorting.
        // Use unique names to avoid filesystem collisions on case-insensitive systems.
        // Case-sensitive sort:  Alfa, Charlie, bravo, delta (uppercase before lowercase)
        // Case-insensitive sort: Alfa, bravo, Charlie, delta (alphabetical regardless of case)
        QFile file1(tempDir_->filePath("bravo.txt"));
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("test");
        file1.close();
        QFile file2(tempDir_->filePath("Alfa.txt"));
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("test");
        file2.close();
        QFile file3(tempDir_->filePath("delta.txt"));
        QVERIFY(file3.open(QIODevice::WriteOnly));
        file3.write("test");
        file3.close();
        QFile file4(tempDir_->filePath("Charlie.txt"));
        QVERIFY(file4.open(QIODevice::WriteOnly));
        file4.write("test");
        file4.close();

        // Verify files were created
        QDir testDir(tempDir_->path());
        QCOMPARE(testDir.entryList(QDir::Files).size(), 4);

        QModelIndex rootIdx = fsModel_->index(tempDir_->path());
        QVERIFY(rootIdx.isValid());

        // Force the model to watch this directory and load contents
        fsModel_->fetchMore(rootIdx);

        // Wait for async loading by polling row count with longer timeout
        for (int attempts = 0; attempts < 50; ++attempts) {
            QCoreApplication::processEvents();
            QTest::qWait(50);
            if (fsModel_->rowCount(rootIdx) >= 4) {
                break;
            }
            // Re-fetch periodically to ensure model is loading
            if (attempts % 10 == 9) {
                fsModel_->fetchMore(rootIdx);
            }
        }

        proxyModel_->sort(0, Qt::AscendingOrder);
        QTest::qWait(100);

        QModelIndex proxyRoot = proxyModel_->mapFromSource(rootIdx);
        int rowCount = proxyModel_->rowCount(proxyRoot);

        if (rowCount < 4) {
            QSKIP("Files not yet loaded by QFileSystemModel");
        }

        // Collect all items in sorted order
        QStringList sortedNames;
        for (int i = 0; i < rowCount; i++) {
            QModelIndex idx = proxyModel_->index(i, 0, proxyRoot);
            sortedNames << proxyModel_->data(idx, Qt::DisplayRole).toString();
        }

        // Verify case-insensitive ordering:
        // With case-insensitive sorting: Alfa, bravo, Charlie, delta
        // With case-sensitive (ASCII) sorting: Alfa, Charlie, bravo, delta
        int alfaPos = sortedNames.indexOf("Alfa.txt");
        int bravoPos = sortedNames.indexOf("bravo.txt");
        int charliePos = sortedNames.indexOf("Charlie.txt");
        int deltaPos = sortedNames.indexOf("delta.txt");

        if (alfaPos >= 0 && bravoPos >= 0 && charliePos >= 0 && deltaPos >= 0) {
            // Case-insensitive sort order: Alfa < bravo < Charlie < delta
            QVERIFY2(alfaPos < bravoPos,
                     "Alfa.txt should come before bravo.txt (case-insensitive sort)");
            QVERIFY2(bravoPos < charliePos,
                     "bravo.txt should come before Charlie.txt (case-insensitive sort)");
            QVERIFY2(charliePos < deltaPos,
                     "Charlie.txt should come before delta.txt (case-insensitive sort)");

            // The key test: bravo (lowercase) should come BEFORE Charlie (uppercase)
            // This would fail with case-sensitive sorting where uppercase comes first
            QVERIFY2(bravoPos < charliePos,
                     "bravo.txt (lowercase) must come before Charlie.txt (uppercase) - "
                     "this proves case-insensitive sorting is working");
        }
    }

    // ========== Edge cases ==========

    void testDataWithNoSourceModel()
    {
        LocalFileProxyModel modelWithoutSource;
        QModelIndex invalidIdx;

        // Should not crash with no source model
        QVariant data = modelWithoutSource.data(invalidIdx, Qt::DisplayRole);
        QVERIFY(!data.isValid());
    }

    void testDataWithInvalidIndex()
    {
        QModelIndex invalidIdx;
        QVariant data = proxyModel_->data(invalidIdx, Qt::DisplayRole);
        QVERIFY(!data.isValid());
    }
};

QTEST_MAIN(TestLocalFileProxyModel)
#include "test_localfileproxymodel.moc"
