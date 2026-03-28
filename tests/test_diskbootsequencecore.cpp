/**
 * @file test_diskbootsequencecore.cpp
 * @brief Tests for the pure diskboot namespace state machine functions.
 *
 * Verifies the complete step sequence, action decisions, state transitions,
 * abort behaviour, and status messages.  All tests are purely in-process
 * with no timers, no Qt event loop, and no REST client.
 */

#include "services/diskbootsequencecore.h"

#include <QTest>

class TestDiskBootSequenceCore : public QObject
{
    Q_OBJECT

private slots:
    // ── actionForStep ───────────────────────────────────────────────────────

    void testActionForStep_mountDisk_returnsMountCall();
    void testActionForStep_mountDisk_usesDriveAndPath();
    void testActionForStep_waitForMount_returnsWaitMs();
    void testActionForStep_waitForMount_usesConfigDelay();
    void testActionForStep_resetMachine_returnsResetCall();
    void testActionForStep_waitForBoot_returnsWaitMs();
    void testActionForStep_waitForBoot_usesConfigDelay();
    void testActionForStep_typeLoadCommand_returnsTypeTextCall();
    void testActionForStep_typeLoadCommand_usesConfigCommand();
    void testActionForStep_waitForBuffer_returnsWaitMs();
    void testActionForStep_sendReturn_returnsTypeTextNewline();
    void testActionForStep_waitForLoad_returnsWaitMs();
    void testActionForStep_typeRunCommand_returnsTypeTextRun();
    void testActionForStep_complete_returnsDoneSuccess();
    void testActionForStep_aborted_returnsDoneFailure();

    // ── advance ─────────────────────────────────────────────────────────────

    void testAdvance_traversesFullSequenceInOrder();
    void testAdvance_completeStepDoesNotAdvanceFurther();
    void testAdvance_abortedStepDoesNotAdvanceFurther();

    // ── abort ───────────────────────────────────────────────────────────────

    void testAbort_fromMountDisk_transitionsToAborted();
    void testAbort_fromMiddleStep_transitionsToAborted();
    void testAbort_preservesDiskPath();

    // ── isTerminal ──────────────────────────────────────────────────────────

    void testIsTerminal_mountDisk_returnsFalse();
    void testIsTerminal_complete_returnsTrue();
    void testIsTerminal_aborted_returnsTrue();

    // ── statusMessage ───────────────────────────────────────────────────────

    void testStatusMessage_mountDisk_containsDiskPath();
    void testStatusMessage_complete_isNonEmpty();
    void testStatusMessage_aborted_isNonEmpty();

    // ── custom Config ───────────────────────────────────────────────────────

    void testCustomConfig_mountDelayIsUsed();
    void testCustomConfig_bootDelayIsUsed();
    void testCustomConfig_loadCommandIsUsed();
    void testCustomConfig_runCommandIsUsed();
    void testCustomConfig_driveIsUsed();
};

// ── Helpers ─────────────────────────────────────────────────────────────────

namespace {

diskboot::State makeState(diskboot::Step step, const QString &path = "/SD/game.d64")
{
    diskboot::State s;
    s.currentStep = step;
    s.diskPath = path;
    return s;
}

}  // namespace

// ── actionForStep ────────────────────────────────────────────────────────────

void TestDiskBootSequenceCore::testActionForStep_mountDisk_returnsMountCall()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::MountDisk));
    QVERIFY(std::holds_alternative<diskboot::MountDiskCall>(action));
}

void TestDiskBootSequenceCore::testActionForStep_mountDisk_usesDriveAndPath()
{
    diskboot::State s = makeState(diskboot::Step::MountDisk, "/SD/BubbleBobble.d64");
    s.config.drive = "b";
    auto action = diskboot::actionForStep(s);
    const auto &call = std::get<diskboot::MountDiskCall>(action);
    QCOMPARE(call.drive, QString("b"));
    QCOMPARE(call.path, QString("/SD/BubbleBobble.d64"));
}

void TestDiskBootSequenceCore::testActionForStep_waitForMount_returnsWaitMs()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::WaitForMount));
    QVERIFY(std::holds_alternative<diskboot::WaitMs>(action));
}

void TestDiskBootSequenceCore::testActionForStep_waitForMount_usesConfigDelay()
{
    diskboot::State s = makeState(diskboot::Step::WaitForMount);
    s.config.mountDelayMs = 123;
    auto action = diskboot::actionForStep(s);
    QCOMPARE(std::get<diskboot::WaitMs>(action).milliseconds, 123);
}

void TestDiskBootSequenceCore::testActionForStep_resetMachine_returnsResetCall()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::ResetMachine));
    QVERIFY(std::holds_alternative<diskboot::ResetMachineCall>(action));
}

void TestDiskBootSequenceCore::testActionForStep_waitForBoot_returnsWaitMs()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::WaitForBoot));
    QVERIFY(std::holds_alternative<diskboot::WaitMs>(action));
}

void TestDiskBootSequenceCore::testActionForStep_waitForBoot_usesConfigDelay()
{
    diskboot::State s = makeState(diskboot::Step::WaitForBoot);
    s.config.bootDelayMs = 42;
    auto action = diskboot::actionForStep(s);
    QCOMPARE(std::get<diskboot::WaitMs>(action).milliseconds, 42);
}

void TestDiskBootSequenceCore::testActionForStep_typeLoadCommand_returnsTypeTextCall()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::TypeLoadCommand));
    QVERIFY(std::holds_alternative<diskboot::TypeTextCall>(action));
}

void TestDiskBootSequenceCore::testActionForStep_typeLoadCommand_usesConfigCommand()
{
    diskboot::State s = makeState(diskboot::Step::TypeLoadCommand);
    s.config.loadCommand = "LOAD\"$\",8";
    auto action = diskboot::actionForStep(s);
    QCOMPARE(std::get<diskboot::TypeTextCall>(action).text, QString("LOAD\"$\",8"));
}

void TestDiskBootSequenceCore::testActionForStep_waitForBuffer_returnsWaitMs()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::WaitForBuffer));
    QVERIFY(std::holds_alternative<diskboot::WaitMs>(action));
}

void TestDiskBootSequenceCore::testActionForStep_sendReturn_returnsTypeTextNewline()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::SendReturn));
    QVERIFY(std::holds_alternative<diskboot::TypeTextCall>(action));
    QCOMPARE(std::get<diskboot::TypeTextCall>(action).text, QString("\n"));
}

void TestDiskBootSequenceCore::testActionForStep_waitForLoad_returnsWaitMs()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::WaitForLoad));
    QVERIFY(std::holds_alternative<diskboot::WaitMs>(action));
}

void TestDiskBootSequenceCore::testActionForStep_typeRunCommand_returnsTypeTextRun()
{
    diskboot::State s = makeState(diskboot::Step::TypeRunCommand);
    auto action = diskboot::actionForStep(s);
    QVERIFY(std::holds_alternative<diskboot::TypeTextCall>(action));
    QCOMPARE(std::get<diskboot::TypeTextCall>(action).text, QString("RUN\n"));
}

void TestDiskBootSequenceCore::testActionForStep_complete_returnsDoneSuccess()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::Complete));
    QVERIFY(std::holds_alternative<diskboot::Done>(action));
    QVERIFY(std::get<diskboot::Done>(action).success);
}

void TestDiskBootSequenceCore::testActionForStep_aborted_returnsDoneFailure()
{
    auto action = diskboot::actionForStep(makeState(diskboot::Step::Aborted));
    QVERIFY(std::holds_alternative<diskboot::Done>(action));
    QVERIFY(!std::get<diskboot::Done>(action).success);
}

// ── advance ──────────────────────────────────────────────────────────────────

void TestDiskBootSequenceCore::testAdvance_traversesFullSequenceInOrder()
{
    // The expected step order
    const QList<diskboot::Step> expectedOrder = {
        diskboot::Step::MountDisk,       diskboot::Step::WaitForMount,
        diskboot::Step::ResetMachine,    diskboot::Step::WaitForBoot,
        diskboot::Step::TypeLoadCommand, diskboot::Step::WaitForBuffer,
        diskboot::Step::SendReturn,      diskboot::Step::WaitForLoad,
        diskboot::Step::TypeRunCommand,  diskboot::Step::Complete,
    };

    diskboot::State state = makeState(diskboot::Step::MountDisk);

    for (int i = 0; i < expectedOrder.size(); ++i) {
        QCOMPARE(state.currentStep, expectedOrder.at(i));
        if (i < expectedOrder.size() - 1) {
            state = diskboot::advance(state);
        }
    }
}

void TestDiskBootSequenceCore::testAdvance_completeStepDoesNotAdvanceFurther()
{
    diskboot::State s = makeState(diskboot::Step::Complete);
    diskboot::State next = diskboot::advance(s);
    QCOMPARE(next.currentStep, diskboot::Step::Complete);
}

void TestDiskBootSequenceCore::testAdvance_abortedStepDoesNotAdvanceFurther()
{
    diskboot::State s = makeState(diskboot::Step::Aborted);
    diskboot::State next = diskboot::advance(s);
    QCOMPARE(next.currentStep, diskboot::Step::Aborted);
}

// ── abort ─────────────────────────────────────────────────────────────────────

void TestDiskBootSequenceCore::testAbort_fromMountDisk_transitionsToAborted()
{
    diskboot::State s = makeState(diskboot::Step::MountDisk);
    diskboot::State aborted = diskboot::abort(s);
    QCOMPARE(aborted.currentStep, diskboot::Step::Aborted);
}

void TestDiskBootSequenceCore::testAbort_fromMiddleStep_transitionsToAborted()
{
    diskboot::State s = makeState(diskboot::Step::WaitForLoad);
    diskboot::State aborted = diskboot::abort(s);
    QCOMPARE(aborted.currentStep, diskboot::Step::Aborted);
}

void TestDiskBootSequenceCore::testAbort_preservesDiskPath()
{
    diskboot::State s = makeState(diskboot::Step::TypeLoadCommand, "/SD/Turrican.d64");
    diskboot::State aborted = diskboot::abort(s);
    QCOMPARE(aborted.diskPath, QString("/SD/Turrican.d64"));
}

// ── isTerminal ────────────────────────────────────────────────────────────────

void TestDiskBootSequenceCore::testIsTerminal_mountDisk_returnsFalse()
{
    QVERIFY(!diskboot::isTerminal(makeState(diskboot::Step::MountDisk)));
}

void TestDiskBootSequenceCore::testIsTerminal_complete_returnsTrue()
{
    QVERIFY(diskboot::isTerminal(makeState(diskboot::Step::Complete)));
}

void TestDiskBootSequenceCore::testIsTerminal_aborted_returnsTrue()
{
    QVERIFY(diskboot::isTerminal(makeState(diskboot::Step::Aborted)));
}

// ── statusMessage ─────────────────────────────────────────────────────────────

void TestDiskBootSequenceCore::testStatusMessage_mountDisk_containsDiskPath()
{
    diskboot::State s = makeState(diskboot::Step::MountDisk, "/SD/Turrican.d64");
    QString msg = diskboot::statusMessage(s);
    QVERIFY(msg.contains("Turrican.d64"));
}

void TestDiskBootSequenceCore::testStatusMessage_complete_isNonEmpty()
{
    QVERIFY(!diskboot::statusMessage(makeState(diskboot::Step::Complete)).isEmpty());
}

void TestDiskBootSequenceCore::testStatusMessage_aborted_isNonEmpty()
{
    QVERIFY(!diskboot::statusMessage(makeState(diskboot::Step::Aborted)).isEmpty());
}

// ── custom Config ─────────────────────────────────────────────────────────────

void TestDiskBootSequenceCore::testCustomConfig_mountDelayIsUsed()
{
    diskboot::State s = makeState(diskboot::Step::WaitForMount);
    s.config.mountDelayMs = 1;
    QCOMPARE(std::get<diskboot::WaitMs>(diskboot::actionForStep(s)).milliseconds, 1);
}

void TestDiskBootSequenceCore::testCustomConfig_bootDelayIsUsed()
{
    diskboot::State s = makeState(diskboot::Step::WaitForBoot);
    s.config.bootDelayMs = 99;
    QCOMPARE(std::get<diskboot::WaitMs>(diskboot::actionForStep(s)).milliseconds, 99);
}

void TestDiskBootSequenceCore::testCustomConfig_loadCommandIsUsed()
{
    diskboot::State s = makeState(diskboot::Step::TypeLoadCommand);
    s.config.loadCommand = "LOAD\"$\",8";
    QCOMPARE(std::get<diskboot::TypeTextCall>(diskboot::actionForStep(s)).text,
             QString("LOAD\"$\",8"));
}

void TestDiskBootSequenceCore::testCustomConfig_runCommandIsUsed()
{
    diskboot::State s = makeState(diskboot::Step::TypeRunCommand);
    s.config.runCommand = "RUN\r\n";
    QCOMPARE(std::get<diskboot::TypeTextCall>(diskboot::actionForStep(s)).text, QString("RUN\r\n"));
}

void TestDiskBootSequenceCore::testCustomConfig_driveIsUsed()
{
    diskboot::State s = makeState(diskboot::Step::MountDisk);
    s.config.drive = "b";
    QCOMPARE(std::get<diskboot::MountDiskCall>(diskboot::actionForStep(s)).drive, QString("b"));
}

QTEST_GUILESS_MAIN(TestDiskBootSequenceCore)
#include "test_diskbootsequencecore.moc"
