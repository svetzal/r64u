/**
 * @file test_ftpcommandqueue.cpp
 * @brief Tests for FtpCommandQueue.
 */

#include "services/ftpcommandqueue.h"

#include <QFile>
#include <QTemporaryFile>
#include <QtTest/QtTest>

class TestFtpCommandQueue : public QObject
{
    Q_OBJECT

private slots:
    void enqueueAndDequeue_preservesFifoOrder()
    {
        FtpCommandQueue queue;
        queue.enqueue(FtpCommandQueue::Command::Pwd);
        queue.enqueue(FtpCommandQueue::Command::Cwd, "/tmp");
        queue.enqueue(FtpCommandQueue::Command::List);

        auto first = queue.dequeueNext();
        QCOMPARE(first.cmd, FtpCommandQueue::Command::Pwd);

        auto second = queue.dequeueNext();
        QCOMPARE(second.cmd, FtpCommandQueue::Command::Cwd);
        QCOMPARE(second.arg, QString("/tmp"));

        auto third = queue.dequeueNext();
        QCOMPARE(third.cmd, FtpCommandQueue::Command::List);
    }

    void isEmpty_trueWhenEmpty()
    {
        FtpCommandQueue queue;
        QVERIFY(queue.isEmpty());
    }

    void isEmpty_falseAfterEnqueue()
    {
        FtpCommandQueue queue;
        queue.enqueue(FtpCommandQueue::Command::Pwd);
        QVERIFY(!queue.isEmpty());
    }

    void size_correctBeforeAndAfterOperations()
    {
        FtpCommandQueue queue;
        QCOMPARE(queue.size(), 0);

        queue.enqueue(FtpCommandQueue::Command::Pwd);
        QCOMPARE(queue.size(), 1);

        queue.enqueue(FtpCommandQueue::Command::List);
        QCOMPARE(queue.size(), 2);

        queue.dequeueNext();
        QCOMPARE(queue.size(), 1);

        queue.dequeueNext();
        QCOMPARE(queue.size(), 0);
    }

    void drain_onNonEmptyQueue_closesFilesAndEmptiesQueue()
    {
        FtpCommandQueue queue;

        // Create a real temp file so we can verify it's closed
        auto file = std::make_shared<QTemporaryFile>();
        QVERIFY(file->open());
        QString filePath = file->fileName();

        queue.enqueueRetr("/remote/file.bin", filePath, file, false);
        queue.enqueue(FtpCommandQueue::Command::Pwd);

        QCOMPARE(queue.size(), 2);
        queue.drain();

        QVERIFY(queue.isEmpty());
        // File should be closed after drain
        QVERIFY(!file->isOpen());
    }

    void drain_onEmptyQueue_doesNotCrash()
    {
        FtpCommandQueue queue;
        // Must not throw or crash
        queue.drain();
        QVERIFY(queue.isEmpty());
    }

    void enqueueRetr_storesFileHandleInPendingCommand()
    {
        FtpCommandQueue queue;
        auto file = std::make_shared<QTemporaryFile>();
        QVERIFY(file->open());

        queue.enqueueRetr("/remote/file.bin", "/local/file.bin", file, false);

        auto cmd = queue.dequeueNext();
        QCOMPARE(cmd.cmd, FtpCommandQueue::Command::Retr);
        QCOMPARE(cmd.arg, QString("/remote/file.bin"));
        QCOMPARE(cmd.localPath, QString("/local/file.bin"));
        QVERIFY(cmd.transferFile != nullptr);
        QCOMPARE(cmd.isMemoryDownload, false);

        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
    }

    void enqueueRetr_memoryDownload_setsIsMemoryFlag()
    {
        FtpCommandQueue queue;
        queue.enqueueRetr("/remote/file.sid", QString(), nullptr, true);

        auto cmd = queue.dequeueNext();
        QCOMPARE(cmd.cmd, FtpCommandQueue::Command::Retr);
        QVERIFY(cmd.isMemoryDownload);
        QVERIFY(cmd.transferFile == nullptr);
    }

    void enqueueStor_storesFileHandleInPendingCommand()
    {
        FtpCommandQueue queue;
        auto file = std::make_shared<QTemporaryFile>();
        QVERIFY(file->open());

        queue.enqueueStor("/remote/dest.prg", "/local/src.prg", file);

        auto cmd = queue.dequeueNext();
        QCOMPARE(cmd.cmd, FtpCommandQueue::Command::Stor);
        QCOMPARE(cmd.arg, QString("/remote/dest.prg"));
        QCOMPARE(cmd.localPath, QString("/local/src.prg"));
        QVERIFY(cmd.transferFile != nullptr);

        if (cmd.transferFile) {
            cmd.transferFile->close();
        }
    }

    void enqueue_preservesArgAndLocalPath()
    {
        FtpCommandQueue queue;
        queue.enqueue(FtpCommandQueue::Command::RnFr, "/old/path", "/stored/local");

        auto cmd = queue.dequeueNext();
        QCOMPARE(cmd.cmd, FtpCommandQueue::Command::RnFr);
        QCOMPARE(cmd.arg, QString("/old/path"));
        QCOMPARE(cmd.localPath, QString("/stored/local"));
    }
};

QTEST_MAIN(TestFtpCommandQueue)
#include "test_ftpcommandqueue.moc"
