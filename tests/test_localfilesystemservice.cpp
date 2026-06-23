/**
 * @file test_localfilesystemservice.cpp
 * @brief Unit tests for LocalFileSystemService via the ILocalFileSystemService contract.
 *
 * Tests operate on a QTemporaryDir so disk state is isolated and cleaned
 * up automatically.
 */

#include "services/ilocalfilesystemservice.h"
#include "services/localfilesystemservice.h"

#include <QFile>
#include <QTemporaryDir>
#include <QtTest>

class TestLocalFileSystemService : public QObject
{
    Q_OBJECT

private:
    ILocalFileSystemService *fs_ = nullptr;
    QTemporaryDir tempDir_;

private slots:
    void init()
    {
        fs_ = new LocalFileSystemService(this);
        QVERIFY(tempDir_.isValid());
    }

    void cleanup()
    {
        delete fs_;
        fs_ = nullptr;
    }

    // -----------------------------------------------------------------------
    // directoryExists
    // -----------------------------------------------------------------------

    void testDirectoryExists_existingDir_returnsTrue()
    {
        QVERIFY(fs_->directoryExists(tempDir_.path()));
    }

    void testDirectoryExists_nonExistingDir_returnsFalse()
    {
        QVERIFY(!fs_->directoryExists(tempDir_.filePath("does_not_exist")));
    }

    // -----------------------------------------------------------------------
    // createDirectoryPath
    // -----------------------------------------------------------------------

    void testCreateDirectoryPath_newPath_createsDir()
    {
        QString newPath = tempDir_.filePath("a/b/c");
        QVERIFY(fs_->createDirectoryPath(newPath));
        QVERIFY(QDir(newPath).exists());
    }

    void testCreateDirectoryPath_existingPath_returnsTrue()
    {
        QVERIFY(fs_->createDirectoryPath(tempDir_.path()));
    }

    // -----------------------------------------------------------------------
    // removeDirectoryRecursively
    // -----------------------------------------------------------------------

    void testRemoveDirectoryRecursively_existing_removesDir()
    {
        QString subDir = tempDir_.filePath("to_remove");
        QVERIFY(QDir().mkpath(subDir));
        QVERIFY(fs_->removeDirectoryRecursively(subDir));
        QVERIFY(!QDir(subDir).exists());
    }

    void testRemoveDirectoryRecursively_nonExisting_returnsTrue()
    {
        // QDir::removeRecursively() returns true when the directory does not exist
        // (treats "already absent" as success).
        QVERIFY(fs_->removeDirectoryRecursively(tempDir_.filePath("ghost")));
    }

    // -----------------------------------------------------------------------
    // listSubdirectoriesRecursively
    // -----------------------------------------------------------------------

    void testListSubdirectoriesRecursively_twoSubdirs_returnsBoth()
    {
        QString a = tempDir_.filePath("sub_a");
        QString b = tempDir_.filePath("sub_a/sub_b");
        QDir().mkpath(a);
        QDir().mkpath(b);

        QStringList result = fs_->listSubdirectoriesRecursively(tempDir_.path());
        QVERIFY(result.contains(a));
        QVERIFY(result.contains(b));
    }

    void testListSubdirectoriesRecursively_emptyDir_returnsEmpty()
    {
        QString empty = tempDir_.filePath("empty_dir");
        QDir().mkpath(empty);
        QVERIFY(fs_->listSubdirectoriesRecursively(empty).isEmpty());
    }

    // -----------------------------------------------------------------------
    // listFilesRecursively
    // -----------------------------------------------------------------------

    void testListFilesRecursively_twoFiles_returnsBoth()
    {
        QString fileA = tempDir_.filePath("file_a.txt");
        QString fileB = tempDir_.filePath("nested/file_b.txt");
        QDir().mkpath(tempDir_.filePath("nested"));
        {
            QFile f(fileA);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }
        {
            QFile f(fileB);
            QVERIFY(f.open(QIODevice::WriteOnly));
        }

        QStringList result = fs_->listFilesRecursively(tempDir_.path());
        QVERIFY(result.contains(fileA));
        QVERIFY(result.contains(fileB));
    }

    void testListFilesRecursively_emptyDir_returnsEmpty()
    {
        QString empty = tempDir_.filePath("scan_empty");
        QDir().mkpath(empty);
        QVERIFY(fs_->listFilesRecursively(empty).isEmpty());
    }

    // -----------------------------------------------------------------------
    // fileExists
    // -----------------------------------------------------------------------

    void testFileExists_existingFile_returnsTrue()
    {
        QString path = tempDir_.filePath("exists.txt");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QVERIFY(fs_->fileExists(path));
    }

    void testFileExists_nonExistingFile_returnsFalse()
    {
        QVERIFY(!fs_->fileExists(tempDir_.filePath("absent.txt")));
    }

    // -----------------------------------------------------------------------
    // fileSize
    // -----------------------------------------------------------------------

    void testFileSize_knownContent_returnsCorrectSize()
    {
        QString path = tempDir_.filePath("sized.txt");
        QFile f(path);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.write("hello");
        f.close();
        QCOMPARE(fs_->fileSize(path), 5LL);
    }

    void testFileSize_nonExistingFile_returnsZero()
    {
        QCOMPARE(fs_->fileSize(tempDir_.filePath("nope.txt")), 0LL);
    }

    // -----------------------------------------------------------------------
    // fileName
    // -----------------------------------------------------------------------

    void testFileName_absolutePath_returnsLastComponent()
    {
        QCOMPARE(fs_->fileName("/home/user/doc/file.txt"), QString("file.txt"));
    }

    // -----------------------------------------------------------------------
    // relativePath
    // -----------------------------------------------------------------------

    void testRelativePath_fileInsideBase_returnsRelative()
    {
        QCOMPARE(fs_->relativePath("/home/user", "/home/user/docs/a.txt"), QString("docs/a.txt"));
    }
};

QTEST_MAIN(TestLocalFileSystemService)
#include "test_localfilesystemservice.moc"
