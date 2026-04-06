/**
 * @file test_ftptransferstate.cpp
 * @brief Tests for FtpTransferState.
 */

#include "services/ftptransferstate.h"

#include <QTemporaryFile>
#include <QtTest/QtTest>

class TestFtpTransferState : public QObject
{
    Q_OBJECT

private slots:
    // --- Initial state ---

    void initialState_buffersAreEmpty()
    {
        FtpTransferState state;
        QVERIFY(state.listBuffer().isEmpty());
        QVERIFY(state.retrBuffer().isEmpty());
    }

    void initialState_noPendingState()
    {
        FtpTransferState state;
        QVERIFY(!state.hasPendingList());
        QVERIFY(!state.hasPendingRetr());
    }

    void initialState_noCurrentFiles()
    {
        FtpTransferState state;
        QVERIFY(state.currentRetrFile() == nullptr);
        QVERIFY(state.currentStorFile() == nullptr);
    }

    // --- LIST buffer ---

    void appendListData_accumulatesIntoListBuffer()
    {
        FtpTransferState state;
        state.appendListData("hello");
        state.appendListData(" world");
        QCOMPARE(state.listBuffer(), QByteArray("hello world"));
    }

    void clearListBuffer_resetsListBuffer()
    {
        FtpTransferState state;
        state.appendListData("some data");
        state.clearListBuffer();
        QVERIFY(state.listBuffer().isEmpty());
    }

    // --- RETR buffer ---

    void appendRetrData_accumulatesIntoRetrBuffer()
    {
        FtpTransferState state;
        state.appendRetrData("foo");
        state.appendRetrData("bar");
        QCOMPARE(state.retrBuffer(), QByteArray("foobar"));
    }

    void clearRetrBuffer_resetsRetrBuffer()
    {
        FtpTransferState state;
        state.appendRetrData("data");
        state.clearRetrBuffer();
        QVERIFY(state.retrBuffer().isEmpty());
    }

    // --- Buffers are independent ---

    void appendListData_doesNotAffectRetrBuffer()
    {
        FtpTransferState state;
        state.appendListData("list data");
        QVERIFY(state.retrBuffer().isEmpty());
    }

    void appendRetrData_doesNotAffectListBuffer()
    {
        FtpTransferState state;
        state.appendRetrData("retr data");
        QVERIFY(state.listBuffer().isEmpty());
    }

    // --- Pending LIST ---

    void savePendingList_thenTake_roundTripsPathAndBuffer()
    {
        FtpTransferState state;
        state.savePendingList("/some/path", QByteArray("listing data"));

        QVERIFY(state.hasPendingList());
        auto pending = state.takePendingList();

        QVERIFY(pending.has_value());
        QCOMPARE(pending->path, QString("/some/path"));
        QCOMPARE(pending->buffer, QByteArray("listing data"));
        QVERIFY(!state.hasPendingList());
    }

    void takePendingList_whenEmpty_returnsNullopt()
    {
        FtpTransferState state;
        auto result = state.takePendingList();
        QVERIFY(!result.has_value());
    }

    void appendToPendingList_appendsToExistingPendingBuffer()
    {
        FtpTransferState state;
        state.savePendingList("/dir", QByteArray("first"));
        state.appendToPendingList(QByteArray(" second"));

        auto pending = state.takePendingList();
        QVERIFY(pending.has_value());
        QCOMPARE(pending->buffer, QByteArray("first second"));
    }

    // --- Pending RETR ---

    void savePendingRetr_thenTake_roundTripsAllFields()
    {
        FtpTransferState state;
        auto file = std::make_shared<QTemporaryFile>();
        QVERIFY(file->open());

        state.savePendingRetr("/remote/file.prg", "/local/file.prg", file, false);

        QVERIFY(state.hasPendingRetr());
        auto pending = state.takePendingRetr();

        QVERIFY(pending.has_value());
        QCOMPARE(pending->remotePath, QString("/remote/file.prg"));
        QCOMPARE(pending->localPath, QString("/local/file.prg"));
        QVERIFY(pending->file != nullptr);
        QCOMPARE(pending->isMemory, false);
        QVERIFY(!state.hasPendingRetr());

        if (pending->file) {
            pending->file->close();
        }
    }

    void savePendingRetr_memoryDownload_preservesIsMemoryFlag()
    {
        FtpTransferState state;
        state.savePendingRetr("/remote/song.sid", QString(), nullptr, true);

        auto pending = state.takePendingRetr();
        QVERIFY(pending.has_value());
        QCOMPARE(pending->isMemory, true);
        QVERIFY(pending->file == nullptr);
    }

    void takePendingRetr_whenEmpty_returnsNullopt()
    {
        FtpTransferState state;
        auto result = state.takePendingRetr();
        QVERIFY(!result.has_value());
    }

    // --- reset() ---

    void reset_clearsAllState()
    {
        FtpTransferState state;

        state.appendListData("list");
        state.appendRetrData("retr");
        state.setTransferSize(1024);
        state.setDownloading(true);
        state.savePendingList("/dir", QByteArray("buf"));
        state.savePendingRetr("/remote", "/local", nullptr, true);

        auto retrFile = std::make_shared<QTemporaryFile>();
        QVERIFY(retrFile->open());
        state.setCurrentRetrFile(retrFile, false);

        auto storFile = std::make_shared<QTemporaryFile>();
        QVERIFY(storFile->open());
        state.setCurrentStorFile(storFile);

        state.reset();

        QVERIFY(state.listBuffer().isEmpty());
        QVERIFY(state.retrBuffer().isEmpty());
        QCOMPARE(state.transferSize(), 0LL);
        QVERIFY(!state.isDownloading());
        QVERIFY(!state.hasPendingList());
        QVERIFY(!state.hasPendingRetr());
        QVERIFY(state.currentRetrFile() == nullptr);
        QVERIFY(state.currentStorFile() == nullptr);
        QVERIFY(!retrFile->isOpen());
        QVERIFY(!storFile->isOpen());
    }

    void reset_onEmptyState_doesNotCrash()
    {
        FtpTransferState state;
        // Must not throw or crash
        state.reset();
        QVERIFY(state.listBuffer().isEmpty());
    }

    // --- Transfer metadata ---

    void transferSize_defaultIsZero()
    {
        FtpTransferState state;
        QCOMPARE(state.transferSize(), 0LL);
    }

    void setTransferSize_updatesValue()
    {
        FtpTransferState state;
        state.setTransferSize(42000LL);
        QCOMPARE(state.transferSize(), 42000LL);
    }

    void isDownloading_defaultIsFalse()
    {
        FtpTransferState state;
        QVERIFY(!state.isDownloading());
    }

    void setDownloading_updatesValue()
    {
        FtpTransferState state;
        state.setDownloading(true);
        QVERIFY(state.isDownloading());
        state.setDownloading(false);
        QVERIFY(!state.isDownloading());
    }
};

QTEST_MAIN(TestFtpTransferState)
#include "test_ftptransferstate.moc"
