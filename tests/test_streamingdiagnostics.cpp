#include <QtTest>
#include <QSignalSpy>
#include <QColor>

#include "services/streamingdiagnostics.h"

class TestStreamingDiagnostics : public QObject
{
    Q_OBJECT

private slots:
    // ========== Constructor and basic state ==========

    void testConstructor()
    {
        StreamingDiagnostics diagnostics;
        QVERIFY(!diagnostics.isEnabled());
    }

    // ========== Enable/Disable ==========

    void testEnable()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);
        QVERIFY(diagnostics.isEnabled());
    }

    void testDisable()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);
        diagnostics.setEnabled(false);
        QVERIFY(!diagnostics.isEnabled());
    }

    void testEnableIdempotent()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);
        diagnostics.setEnabled(true);  // Should not reset state
        QVERIFY(diagnostics.isEnabled());
    }

    // ========== Snapshot ==========

    void testInitialSnapshot()
    {
        StreamingDiagnostics diagnostics;
        DiagnosticsSnapshot snapshot = diagnostics.currentSnapshot();

        QCOMPARE(snapshot.overallQuality, QualityLevel::Unknown);
        QCOMPARE(snapshot.videoPacketsReceived, static_cast<quint64>(0));
        QCOMPARE(snapshot.videoPacketsLost, static_cast<quint64>(0));
        QCOMPARE(snapshot.videoFramesCompleted, static_cast<quint64>(0));
        QCOMPARE(snapshot.audioPacketsReceived, static_cast<quint64>(0));
        QCOMPARE(snapshot.audioBufferUnderruns, static_cast<quint64>(0));
    }

    void testSnapshotAfterEnable()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);

        // Wait a bit for uptime to accumulate
        QTest::qWait(10);

        DiagnosticsSnapshot snapshot = diagnostics.currentSnapshot();
        QVERIFY(snapshot.uptimeMs > 0);
    }

    // ========== Quality level strings and colors ==========

    void testQualityLevelString()
    {
        QCOMPARE(StreamingDiagnostics::qualityLevelString(QualityLevel::Unknown), QString("Unknown"));
        QCOMPARE(StreamingDiagnostics::qualityLevelString(QualityLevel::Excellent), QString("Excellent"));
        QCOMPARE(StreamingDiagnostics::qualityLevelString(QualityLevel::Good), QString("Good"));
        QCOMPARE(StreamingDiagnostics::qualityLevelString(QualityLevel::Fair), QString("Fair"));
        QCOMPARE(StreamingDiagnostics::qualityLevelString(QualityLevel::Poor), QString("Poor"));
    }

    void testQualityLevelColor()
    {
        QColor unknownColor = StreamingDiagnostics::qualityLevelColor(QualityLevel::Unknown);
        QColor excellentColor = StreamingDiagnostics::qualityLevelColor(QualityLevel::Excellent);
        QColor poorColor = StreamingDiagnostics::qualityLevelColor(QualityLevel::Poor);

        // Unknown should be grey-ish
        QCOMPARE(unknownColor.red(), 128);

        // Excellent should be green
        QCOMPARE(excellentColor.green(), 200);
        QCOMPARE(excellentColor.red(), 0);

        // Poor should be red
        QCOMPARE(poorColor.red(), 200);
        QCOMPARE(poorColor.green(), 0);
    }

    // ========== Callbacks ==========

    void testVideoCallback()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.videoCallback();

        // All callback functions should be set
        QVERIFY(callback.onPacketReceived != nullptr);
        QVERIFY(callback.onFrameStarted != nullptr);
        QVERIFY(callback.onFrameCompleted != nullptr);
        QVERIFY(callback.onOutOfOrderPacket != nullptr);
    }

    void testAudioCallback()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.audioCallback();

        // All callback functions should be set
        QVERIFY(callback.onPacketReceived != nullptr);
        QVERIFY(callback.onBufferUnderrun != nullptr);
        QVERIFY(callback.onSampleDiscontinuity != nullptr);
    }

    void testVideoPacketCallback()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.videoCallback();

        // Simulate packet arrivals
        callback.onPacketReceived(1000);  // 1ms
        callback.onPacketReceived(2000);  // 2ms

        // Should not crash, diagnostics should be collecting data
        DiagnosticsSnapshot snapshot = diagnostics.currentSnapshot();
        // Can't easily verify the jitter calculation without more samples
    }

    void testVideoFrameCallback()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.videoCallback();

        // Simulate frame assembly
        callback.onFrameStarted(1, 1000);   // Frame 1 started at 1ms
        callback.onFrameCompleted(1, 5000, true);  // Frame 1 completed at 5ms

        // Frame should be counted as completed
        // Note: The callback doesn't directly update the counts - that's done via signals
    }

    // ========== Reset ==========

    void testReset()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.videoCallback();
        callback.onPacketReceived(1000);
        callback.onOutOfOrderPacket();

        diagnostics.reset();

        DiagnosticsSnapshot snapshot = diagnostics.currentSnapshot();
        QCOMPARE(snapshot.videoOutOfOrderPackets, static_cast<quint64>(0));
    }

    // ========== Update interval ==========

    void testUpdateInterval()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setUpdateInterval(100);  // 100ms

        QSignalSpy spy(&diagnostics, &StreamingDiagnostics::diagnosticsUpdated);
        diagnostics.setEnabled(true);

        // Wait for a few updates
        QTest::qWait(350);

        // Should have received at least 2-3 updates
        QVERIFY(spy.count() >= 2);
    }

    void testNoUpdatesWhenDisabled()
    {
        StreamingDiagnostics diagnostics;
        diagnostics.setUpdateInterval(50);  // Fast updates

        QSignalSpy spy(&diagnostics, &StreamingDiagnostics::diagnosticsUpdated);

        // Don't enable - should not receive updates
        QTest::qWait(150);

        QCOMPARE(spy.count(), 0);
    }

    // ========== Callbacks when disabled ==========

    void testCallbacksIgnoredWhenDisabled()
    {
        StreamingDiagnostics diagnostics;
        // Not enabled

        auto callback = diagnostics.videoCallback();

        // These should be ignored (not crash)
        callback.onPacketReceived(1000);
        callback.onFrameStarted(1, 1000);
        callback.onFrameCompleted(1, 5000, true);
        callback.onOutOfOrderPacket();

        DiagnosticsSnapshot snapshot = diagnostics.currentSnapshot();
        QCOMPARE(snapshot.videoOutOfOrderPackets, static_cast<quint64>(0));
    }
};

QTEST_MAIN(TestStreamingDiagnostics)
#include "test_streamingdiagnostics.moc"
