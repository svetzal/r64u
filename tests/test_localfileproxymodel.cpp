#include <QtTest>
#include <QFileSystemModel>
#include <QTemporaryDir>
#include <QFile>
#include <QSignalSpy>

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

        // Wait for async operations
        QSignalSpy spy(fsModel, &QAbstractItemModel::layoutChanged);
        QTest::qWait(100);

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
        QTest::qWait(100);

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
