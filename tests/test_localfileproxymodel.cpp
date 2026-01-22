#include <QtTest>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileSystemModel>
#include <QSignalSpy>
#include <QTemporaryDir>

#include "models/localfileproxymodel.h"

class TestLocalFileProxyModel : public QObject
{
    Q_OBJECT

private:
    QTemporaryDir *tempDir;
    QFileSystemModel *fsModel;
    LocalFileProxyModel *proxyModel;

private slots:
    void init()
    {
        tempDir = new QTemporaryDir();
        QVERIFY(tempDir->isValid());

        fsModel = new QFileSystemModel(this);
        fsModel->setRootPath(tempDir->path());

        proxyModel = new LocalFileProxyModel(this);
        proxyModel->setSourceModel(fsModel);
    }

    void cleanup()
    {
        delete proxyModel;
        proxyModel = nullptr;
        delete fsModel;
        fsModel = nullptr;
        delete tempDir;
        tempDir = nullptr;
    }

    // ========== Basic functionality ==========

    void testConstructor()
    {
        LocalFileProxyModel model;
        QVERIFY(model.sourceModel() == nullptr);
    }

    void testSetSourceModel()
    {
        QCOMPARE(proxyModel->sourceModel(), fsModel);
    }

    // ========== data() for file size column ==========

    void testDataFileSizeColumn()
    {
        // Create a test file with known size
        QString filePath = tempDir->filePath("testfile.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QByteArray content(1234, 'X');
        file.write(content);
        file.close();

        // Wait for the file system model to update
        QModelIndex rootIdx = fsModel->index(tempDir->path());
        QVERIFY(rootIdx.isValid());

        // Force the model to fetch children
        fsModel->fetchMore(rootIdx);

        // Wait for async operations - QFileSystemModel can be slow
        QSignalSpy spy(fsModel, &QAbstractItemModel::layoutChanged);
        QTest::qWait(500);

        // Find our test file
        for (int i = 0; i < fsModel->rowCount(rootIdx); i++) {
            QModelIndex nameIdx = fsModel->index(i, 0, rootIdx);
            if (fsModel->fileName(nameIdx) == "testfile.txt") {
                // Get the proxy index for the size column
                QModelIndex proxyNameIdx = proxyModel->mapFromSource(nameIdx);
                QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

                // Check that data returns the byte count as string
                QVariant data = proxyModel->data(proxySizeIdx, Qt::DisplayRole);
                QCOMPARE(data.toString(), QString("1234"));
                return;
            }
        }

        QFAIL("Test file not found in model");
    }

    void testDataDirectoryReturnsEmpty()
    {
        // Create a test directory
        QString dirPath = tempDir->filePath("testdir");
        QDir().mkpath(dirPath);

        QModelIndex rootIdx = fsModel->index(tempDir->path());
        fsModel->fetchMore(rootIdx);
        QTest::qWait(100);

        // Find our test directory
        for (int i = 0; i < fsModel->rowCount(rootIdx); i++) {
            QModelIndex nameIdx = fsModel->index(i, 0, rootIdx);
            if (fsModel->fileName(nameIdx) == "testdir") {
                QModelIndex proxyNameIdx = proxyModel->mapFromSource(nameIdx);
                QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

                // Directories should return empty variant
                QVariant data = proxyModel->data(proxySizeIdx, Qt::DisplayRole);
                QVERIFY(!data.isValid() || data.toString().isEmpty());
                return;
            }
        }

        QFAIL("Test directory not found in model");
    }

    void testDataOtherColumnsPassthrough()
    {
        // Create a test file
        QString filePath = tempDir->filePath("passthrough.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("test");
        file.close();

        QModelIndex rootIdx = fsModel->index(tempDir->path());
        fsModel->fetchMore(rootIdx);
        QTest::qWait(100);

        // Find our test file
        for (int i = 0; i < fsModel->rowCount(rootIdx); i++) {
            QModelIndex nameIdx = fsModel->index(i, 0, rootIdx);
            if (fsModel->fileName(nameIdx) == "passthrough.txt") {
                QModelIndex proxyNameIdx = proxyModel->mapFromSource(nameIdx);

                // Column 0 (name) should pass through unchanged
                QVariant nameData = proxyModel->data(proxyNameIdx, Qt::DisplayRole);
                QCOMPARE(nameData.toString(), QString("passthrough.txt"));
                return;
            }
        }

        QFAIL("Test file not found in model");
    }

    void testDataOtherRolesPassthrough()
    {
        // Create a test file
        QString filePath = tempDir->filePath("roles.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(QByteArray(5000, 'X'));
        file.close();

        QModelIndex rootIdx = fsModel->index(tempDir->path());
        fsModel->fetchMore(rootIdx);
        QTest::qWait(100);

        // Find our test file
        for (int i = 0; i < fsModel->rowCount(rootIdx); i++) {
            QModelIndex nameIdx = fsModel->index(i, 0, rootIdx);
            if (fsModel->fileName(nameIdx) == "roles.txt") {
                QModelIndex proxyNameIdx = proxyModel->mapFromSource(nameIdx);
                QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

                // Non-DisplayRole on size column should pass through
                // (e.g., tooltip, decoration, etc.)
                QVariant editData = proxyModel->data(proxySizeIdx, Qt::EditRole);
                // Should be same as source model's data for this role
                QModelIndex sourceSizeIdx = proxyModel->mapToSource(proxySizeIdx);
                QVariant sourceEditData = fsModel->data(sourceSizeIdx, Qt::EditRole);
                QCOMPARE(editData, sourceEditData);
                return;
            }
        }

        QFAIL("Test file not found in model");
    }

    void testDataZeroSizeFile()
    {
        // Create an empty file
        QString filePath = tempDir->filePath("empty.txt");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.close();

        QModelIndex rootIdx = fsModel->index(tempDir->path());
        fsModel->fetchMore(rootIdx);
        QTest::qWait(100);

        for (int i = 0; i < fsModel->rowCount(rootIdx); i++) {
            QModelIndex nameIdx = fsModel->index(i, 0, rootIdx);
            if (fsModel->fileName(nameIdx) == "empty.txt") {
                QModelIndex proxyNameIdx = proxyModel->mapFromSource(nameIdx);
                QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

                QVariant data = proxyModel->data(proxySizeIdx, Qt::DisplayRole);
                QCOMPARE(data.toString(), QString("0"));
                return;
            }
        }

        QFAIL("Test file not found in model");
    }

    void testDataLargeFile()
    {
        // Create a larger file (100KB)
        QString filePath = tempDir->filePath("large.bin");
        QFile file(filePath);
        QVERIFY(file.open(QIODevice::WriteOnly));
        QByteArray content(102400, 'X'); // 100KB
        file.write(content);
        file.close();

        QModelIndex rootIdx = fsModel->index(tempDir->path());
        fsModel->fetchMore(rootIdx);
        QTest::qWait(200);  // Wait for async loading

        for (int i = 0; i < fsModel->rowCount(rootIdx); i++) {
            QModelIndex nameIdx = fsModel->index(i, 0, rootIdx);
            if (fsModel->fileName(nameIdx) == "large.bin") {
                QModelIndex proxyNameIdx = proxyModel->mapFromSource(nameIdx);
                QModelIndex proxySizeIdx = proxyNameIdx.sibling(proxyNameIdx.row(), 1);

                QVariant data = proxyModel->data(proxySizeIdx, Qt::DisplayRole);
                QCOMPARE(data.toString(), QString("102400"));
                return;
            }
        }

        QFAIL("Test file not found in model");
    }

    // ========== Sorting ==========

    void testSortingDirectoriesFirst()
    {
        // Create test files and directories with names that would sort
        // alphabetically interleaved if not for folder-first sorting
        QDir().mkpath(tempDir->filePath("b_dir"));
        QFile file1(tempDir->filePath("a_file.txt"));
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("test");
        file1.close();
        QDir().mkpath(tempDir->filePath("c_dir"));
        QFile file2(tempDir->filePath("d_file.txt"));
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("test");
        file2.close();

        QModelIndex rootIdx = fsModel->index(tempDir->path());
        fsModel->fetchMore(rootIdx);
        QTest::qWait(200);  // Wait for async loading

        // Set up sorting on the proxy
        proxyModel->sort(0, Qt::AscendingOrder);
        QTest::qWait(50);  // Wait for sort

        QModelIndex proxyRoot = proxyModel->mapFromSource(rootIdx);
        int rowCount = proxyModel->rowCount(proxyRoot);

        if (rowCount < 4) {
            QSKIP("Files not yet loaded by QFileSystemModel");
        }

        // Collect all items in sorted order
        QStringList sortedNames;
        QList<bool> isDir;
        for (int i = 0; i < rowCount; i++) {
            QModelIndex idx = proxyModel->index(i, 0, proxyRoot);
            sortedNames << proxyModel->data(idx, Qt::DisplayRole).toString();
            QModelIndex sourceIdx = proxyModel->mapToSource(idx);
            isDir << fsModel->isDir(sourceIdx);
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
        QFile file1(tempDir->filePath("bravo.txt"));
        QVERIFY(file1.open(QIODevice::WriteOnly));
        file1.write("test");
        file1.close();
        QFile file2(tempDir->filePath("Alfa.txt"));
        QVERIFY(file2.open(QIODevice::WriteOnly));
        file2.write("test");
        file2.close();
        QFile file3(tempDir->filePath("delta.txt"));
        QVERIFY(file3.open(QIODevice::WriteOnly));
        file3.write("test");
        file3.close();
        QFile file4(tempDir->filePath("Charlie.txt"));
        QVERIFY(file4.open(QIODevice::WriteOnly));
        file4.write("test");
        file4.close();

        // Verify files were created
        QDir testDir(tempDir->path());
        QCOMPARE(testDir.entryList(QDir::Files).size(), 4);

        QModelIndex rootIdx = fsModel->index(tempDir->path());
        QVERIFY(rootIdx.isValid());

        // Force the model to watch this directory and load contents
        fsModel->fetchMore(rootIdx);

        // Wait for async loading by polling row count with longer timeout
        for (int attempts = 0; attempts < 50; ++attempts) {
            QCoreApplication::processEvents();
            QTest::qWait(50);
            if (fsModel->rowCount(rootIdx) >= 4) {
                break;
            }
            // Re-fetch periodically to ensure model is loading
            if (attempts % 10 == 9) {
                fsModel->fetchMore(rootIdx);
            }
        }

        proxyModel->sort(0, Qt::AscendingOrder);
        QTest::qWait(100);

        QModelIndex proxyRoot = proxyModel->mapFromSource(rootIdx);
        int rowCount = proxyModel->rowCount(proxyRoot);

        if (rowCount < 4) {
            QSKIP("Files not yet loaded by QFileSystemModel");
        }

        // Collect all items in sorted order
        QStringList sortedNames;
        for (int i = 0; i < rowCount; i++) {
            QModelIndex idx = proxyModel->index(i, 0, proxyRoot);
            sortedNames << proxyModel->data(idx, Qt::DisplayRole).toString();
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
        QVariant data = proxyModel->data(invalidIdx, Qt::DisplayRole);
        QVERIFY(!data.isValid());
    }
};

QTEST_MAIN(TestLocalFileProxyModel)
#include "test_localfileproxymodel.moc"
