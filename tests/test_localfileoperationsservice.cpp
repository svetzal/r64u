/**
 * @file test_localfileoperationsservice.cpp
 * @brief Unit tests for LocalFileOperationsService.
 *
 * Uses QTemporaryDir for real filesystem isolation — LocalFileOperationsService
 * operates on QDir/QFile directly, so no mock is needed.  Tests verify signal
 * emissions on success and failure without inspecting the underlying Qt
 * internals.
 */

#include "services/ierroremitter.h"
#include "services/localfileoperationsservice.h"

#include <QFile>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class TestLocalFileOperationsService : public QObject
{
    Q_OBJECT

private:
    LocalFileOperationsService *service = nullptr;
    QTemporaryDir *tempDir = nullptr;

private slots:
    void init()
    {
        service = new LocalFileOperationsService(this);
        tempDir = new QTemporaryDir();
        QVERIFY(tempDir->isValid());
    }

    void cleanup()
    {
        delete tempDir;
        tempDir = nullptr;
    }

    // -----------------------------------------------------------------------
    // createFolder tests
    // -----------------------------------------------------------------------

    void testCreateFolder_success_emitsFolderCreated()
    {
        QSignalSpy spy(service, &LocalFileOperationsService::folderCreated);

        service->createFolder(tempDir->path(), "new_folder");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().endsWith("new_folder"));
    }

    void testCreateFolder_success_emitsStatusMessage()
    {
        QSignalSpy spy(service, &LocalFileOperationsService::statusMessage);

        service->createFolder(tempDir->path(), "my_folder");

        QCOMPARE(spy.count(), 1);
        QVERIFY(!spy.at(0).at(0).toString().isEmpty());
    }

    void testCreateFolder_failure_emitsErrorReported()
    {
        QSignalSpy spy(service, &IErrorEmitter::errorReported);

        // Parent path does not exist — mkdir without -p will fail
        service->createFolder("/nonexistent/path/that/does/not/exist", "folder");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    }

    void testCreateFolder_failure_doesNotEmitFolderCreated()
    {
        QSignalSpy spy(service, &LocalFileOperationsService::folderCreated);

        service->createFolder("/nonexistent/path/that/does/not/exist", "folder");

        QCOMPARE(spy.count(), 0);
    }

    // -----------------------------------------------------------------------
    // renameItem tests
    // -----------------------------------------------------------------------

    void testRenameItem_success_emitsItemRenamed()
    {
        QString originalPath = tempDir->path() + "/original.txt";
        QFile f(originalPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QSignalSpy spy(service, &LocalFileOperationsService::itemRenamed);
        service->renameItem(originalPath, "renamed.txt");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), originalPath);
        QVERIFY(spy.at(0).at(1).toString().endsWith("renamed.txt"));
    }

    void testRenameItem_success_emitsStatusMessage()
    {
        QString originalPath = tempDir->path() + "/toRename.txt";
        QFile f(originalPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QSignalSpy spy(service, &LocalFileOperationsService::statusMessage);
        service->renameItem(originalPath, "done.txt");

        QCOMPARE(spy.count(), 1);
    }

    void testRenameItem_failure_notExists_emitsErrorReported()
    {
        QSignalSpy spy(service, &IErrorEmitter::errorReported);

        service->renameItem("/nonexistent/file.txt", "new.txt");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    }

    void testRenameItem_failure_notExists_doesNotEmitItemRenamed()
    {
        QSignalSpy spy(service, &LocalFileOperationsService::itemRenamed);

        service->renameItem("/nonexistent/file.txt", "new.txt");

        QCOMPARE(spy.count(), 0);
    }

    // -----------------------------------------------------------------------
    // deleteItems tests
    // -----------------------------------------------------------------------

    void testDeleteItems_nonExistentPath_emitsErrorReported()
    {
        QSignalSpy spy(service, &IErrorEmitter::errorReported);

        service->deleteItems({"/nonexistent/path/file.txt"});

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).value<ErrorCategory>(), ErrorCategory::FileOperation);
    }

    void testDeleteItems_nonExistentPath_doesNotEmitItemsDeleted()
    {
        QSignalSpy spy(service, &LocalFileOperationsService::itemsDeleted);

        service->deleteItems({"/nonexistent/path/file.txt"});

        QCOMPARE(spy.count(), 0);
    }

    void testDeleteItems_nonExistentPath_emitsStatusMessage()
    {
        QSignalSpy spy(service, &LocalFileOperationsService::statusMessage);

        service->deleteItems({"/nonexistent/path/file.txt"});

        QCOMPARE(spy.count(), 1);
    }

    void testDeleteItems_singleSuccess_emitsItemsDeleted()
    {
        QString filePath = tempDir->path() + "/to_delete.txt";
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        QVERIFY(QFile::exists(filePath));

        QSignalSpy spy(service, &LocalFileOperationsService::itemsDeleted);
        service->deleteItems({filePath});

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 1);
    }

    void testDeleteItems_singleSuccess_emitsStatusMessage()
    {
        QString filePath = tempDir->path() + "/to_delete2.txt";
        QFile f(filePath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();

        QSignalSpy spy(service, &LocalFileOperationsService::statusMessage);
        service->deleteItems({filePath});

        QCOMPARE(spy.count(), 1);
    }

    void testDeleteItems_multipleSuccess_emitsItemsDeletedWithCount()
    {
        QString path1 = tempDir->path() + "/delete_a.txt";
        QString path2 = tempDir->path() + "/delete_b.txt";
        for (const auto &p : {path1, path2}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.close();
        }

        QSignalSpy spy(service, &LocalFileOperationsService::itemsDeleted);
        service->deleteItems({path1, path2});

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
    }

    void testDeleteItems_multipleSuccess_emitsAllGoodStatusMessage()
    {
        QString path1 = tempDir->path() + "/del_c.txt";
        QString path2 = tempDir->path() + "/del_d.txt";
        for (const auto &p : {path1, path2}) {
            QFile f(p);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.close();
        }

        QSignalSpy spy(service, &LocalFileOperationsService::statusMessage);
        service->deleteItems({path1, path2});

        // One statusMessage summarising the batch
        QCOMPARE(spy.count(), 1);
    }

    void testDeleteItems_partialFailure_emitsItemsDeletedAndOperationFailed()
    {
        QString goodPath = tempDir->path() + "/good_file.txt";
        QFile f(goodPath);
        QVERIFY(f.open(QIODevice::WriteOnly));
        f.close();
        const QString badPath = "/nonexistent/bad_file.txt";

        QSignalSpy deletedSpy(service, &LocalFileOperationsService::itemsDeleted);
        QSignalSpy failedSpy(service, &IErrorEmitter::errorReported);

        service->deleteItems({goodPath, badPath});

        QCOMPARE(deletedSpy.count(), 1);
        QCOMPARE(deletedSpy.at(0).at(0).toInt(), 1);
        QCOMPARE(failedSpy.count(), 1);
    }
};

QTEST_GUILESS_MAIN(TestLocalFileOperationsService)
#include "test_localfileoperationsservice.moc"
