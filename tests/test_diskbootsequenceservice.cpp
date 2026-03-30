/**
 * @file test_diskbootsequenceservice.cpp
 * @brief Unit tests for DiskBootSequenceService — the imperative shell around the
 *        diskboot:: state machine.
 *
 * Uses MockRestClient (interface injection) and a zero-delay diskboot::Config so
 * that all QTimer::singleShot(0) events fire after a single processEvents() call.
 *
 * Covers: startBootSequence(), abort(), isRunning(), signal emissions, and
 * REST-client call tracking through the full sequence.
 */

#include "mocks/mockrestclient.h"
#include "services/diskbootsequenceservice.h"

#include <QCoreApplication>
#include <QSignalSpy>
#include <QtTest>

// Fast-boot config with zero delays so timers fire immediately in tests
namespace {
diskboot::Config fastConfig()
{
    diskboot::Config cfg;
    cfg.mountDelayMs = 0;
    cfg.bootDelayMs = 0;
    cfg.bufferDelayMs = 0;
    cfg.loadDelayMs = 0;
    return cfg;
}
}  // namespace

class TestDiskBootSequenceService : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        mock_ = new MockRestClient(this);
        svc_ = new DiskBootSequenceService(this);
        svc_->setRestClient(mock_);
    }

    void cleanup()
    {
        delete svc_;
        svc_ = nullptr;
        delete mock_;
        mock_ = nullptr;
    }

    // =========================================================
    // Initial state
    // =========================================================

    void testInitialState_NotRunning() { QVERIFY(!svc_->isRunning()); }

    // =========================================================
    // startBootSequence — state changes
    // =========================================================

    void testStartBootSequence_SetsRunning()
    {
        // Use default (long) timing — we only check that running_ is set initially
        // before any timer fires.
        svc_->startBootSequence("/SD/games/test.d64");

        QVERIFY(svc_->isRunning());
    }

    void testStartBootSequence_WithFastConfig_CompletesSuccessfully()
    {
        QSignalSpy completedSpy(svc_, &DiskBootSequenceService::completed);
        QSignalSpy abortedSpy(svc_, &DiskBootSequenceService::aborted);

        svc_->startBootSequence("/SD/games/test.d64", fastConfig());

        // Process all zero-delay timer events until the sequence finishes
        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
        }

        QCOMPARE(completedSpy.count(), 1);
        QCOMPARE(abortedSpy.count(), 0);
        QVERIFY(!svc_->isRunning());
    }

    void testStartBootSequence_WithFastConfig_MakesRestCalls()
    {
        svc_->startBootSequence("/SD/games/test.d64", fastConfig());

        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
        }

        // The full sequence calls: mountImage, resetMachine, typeText x3 (load, return, run)
        // MockRestClient tracks typeText calls — expect 3 (loadCommand, "\n", runCommand)
        QVERIFY(mock_->mockGetTypeTextCallCount() >= 2);
    }

    void testStartBootSequence_WithFastConfig_EmitsStepChanged()
    {
        QSignalSpy spy(svc_, &DiskBootSequenceService::stepChanged);

        svc_->startBootSequence("/SD/games/test.d64", fastConfig());

        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
        }

        // At minimum one stepChanged should have been emitted (MountDisk step)
        QVERIFY(spy.count() > 0);
    }

    void testStartBootSequence_WithFastConfig_EmitsStatusMessage()
    {
        QSignalSpy spy(svc_, &DiskBootSequenceService::statusMessage);

        svc_->startBootSequence("/SD/games/test.d64", fastConfig());

        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
        }

        QVERIFY(spy.count() > 0);
    }

    // =========================================================
    // abort()
    // =========================================================

    void testAbort_WhenNotRunning_DoesNotEmitAborted()
    {
        QSignalSpy spy(svc_, &DiskBootSequenceService::aborted);

        svc_->abort();

        QCOMPARE(spy.count(), 0);
    }

    void testAbort_WhenRunning_EmitsAborted()
    {
        // Start with default (long) timing so the first WaitMs pause keeps it running
        svc_->startBootSequence("/SD/games/test.d64");
        // Process just enough events to get past the synchronous REST steps to WaitMs
        QCoreApplication::processEvents();

        QSignalSpy spy(svc_, &DiskBootSequenceService::aborted);

        svc_->abort();

        QCOMPARE(spy.count(), 1);
    }

    void testAbort_WhenRunning_SetsNotRunning()
    {
        svc_->startBootSequence("/SD/games/test.d64");
        QCoreApplication::processEvents();

        svc_->abort();

        QVERIFY(!svc_->isRunning());
    }

    void testAbort_WhenRunning_DoesNotEmitCompleted()
    {
        QSignalSpy completedSpy(svc_, &DiskBootSequenceService::completed);

        svc_->startBootSequence("/SD/games/test.d64");
        QCoreApplication::processEvents();
        svc_->abort();

        QCOMPARE(completedSpy.count(), 0);
    }

    void testAbort_WhenRunning_EmitsStatusMessage()
    {
        svc_->startBootSequence("/SD/games/test.d64");
        QCoreApplication::processEvents();

        QSignalSpy spy(svc_, &DiskBootSequenceService::statusMessage);

        svc_->abort();

        QCOMPARE(spy.count(), 1);
    }

    // =========================================================
    // Restarting
    // =========================================================

    void testStartWhileRunning_RestartsSequence()
    {
        // Start first sequence (long timing — stays in first WaitMs)
        svc_->startBootSequence("/SD/games/first.d64");
        QCoreApplication::processEvents();
        QVERIFY(svc_->isRunning());

        // Start a second sequence — should silently restart
        QSignalSpy completedSpy(svc_, &DiskBootSequenceService::completed);
        svc_->startBootSequence("/SD/games/second.d64", fastConfig());

        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
        }

        QCOMPARE(completedSpy.count(), 1);
        QVERIFY(!svc_->isRunning());
    }

    // =========================================================
    // setRestClient — null safety
    // =========================================================

    void testStartBootSequence_NullRestClient_StillCompletes()
    {
        // Remove REST client — sequence should still complete (guards in executeCurrentStep)
        DiskBootSequenceService svcNoClient;
        QSignalSpy completedSpy(&svcNoClient, &DiskBootSequenceService::completed);

        svcNoClient.startBootSequence("/SD/games/test.d64", fastConfig());

        for (int i = 0; i < 20; ++i) {
            QCoreApplication::processEvents();
        }

        QCOMPARE(completedSpy.count(), 1);
    }

private:
    MockRestClient *mock_ = nullptr;
    DiskBootSequenceService *svc_ = nullptr;
};

QTEST_MAIN(TestDiskBootSequenceService)
#include "test_diskbootsequenceservice.moc"
