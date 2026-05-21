#include "services/streamingdiagnosticsservice.h"

#include <QColor>
#include <QSignalSpy>
#include <QtTest>

#include <tuple>

class TestStreamingDiagnosticsService : public QObject
{
    Q_OBJECT

private slots:
    // ========== Constructor and basic state ==========

    void testConstructor()
    {
        StreamingDiagnosticsService diagnostics;
        QVERIFY(!diagnostics.isEnabled());
    }

    // ========== Enable/Disable ==========

    void testEnable()
    {
        StreamingDiagnosticsService diagnostics;
        diagnostics.setEnabled(true);
        QVERIFY(diagnostics.isEnabled());
    }

    void testDisable()
    {
        StreamingDiagnosticsService diagnostics;
        diagnostics.setEnabled(true);
        diagnostics.setEnabled(false);
        QVERIFY(!diagnostics.isEnabled());
    }

    void testEnableIdempotent()
    {
        StreamingDiagnosticsService diagnostics;
        diagnostics.setEnabled(true);
        diagnostics.setEnabled(true);  // Should not reset state
        QVERIFY(diagnostics.isEnabled());
    }

    // ========== Snapshot ==========

    void testInitialSnapshot()
    {
        StreamingDiagnosticsService diagnostics;
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
        StreamingDiagnosticsService diagnostics;
        diagnostics.setEnabled(true);

        // Wait a bit for uptime to accumulate
        QTest::qWait(10);

        DiagnosticsSnapshot snapshot = diagnostics.currentSnapshot();
        QVERIFY(snapshot.uptimeMs > 0);
    }

    // ========== Quality level strings and colors ==========

    void testQualityLevelString()
    {
        QCOMPARE(StreamingDiagnosticsService::qualityLevelString(QualityLevel::Unknown),
                 QString("Unknown"));
        QCOMPARE(StreamingDiagnosticsService::qualityLevelString(QualityLevel::Excellent),
                 QString("Excellent"));
        QCOMPARE(StreamingDiagnosticsService::qualityLevelString(QualityLevel::Good),
                 QString("Good"));
        QCOMPARE(StreamingDiagnosticsService::qualityLevelString(QualityLevel::Fair),
                 QString("Fair"));
        QCOMPARE(StreamingDiagnosticsService::qualityLevelString(QualityLevel::Poor),
                 QString("Poor"));
    }

    void testQualityLevelColor()
    {
        QColor unknownColor = StreamingDiagnosticsService::qualityLevelColor(QualityLevel::Unknown);
        QColor excellentColor =
            StreamingDiagnosticsService::qualityLevelColor(QualityLevel::Excellent);
        QColor poorColor = StreamingDiagnosticsService::qualityLevelColor(QualityLevel::Poor);

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
        StreamingDiagnosticsService diagnostics;
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
        StreamingDiagnosticsService diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.audioCallback();

        // All callback functions should be set
        QVERIFY(callback.onPacketReceived != nullptr);
        QVERIFY(callback.onBufferUnderrun != nullptr);
        QVERIFY(callback.onSampleDiscontinuity != nullptr);
    }

    void testVideoPacketCallback()
    {
        StreamingDiagnosticsService diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.videoCallback();

        // Simulate packet arrivals
        callback.onPacketReceived(1000);  // 1ms
        callback.onPacketReceived(2000);  // 2ms

        // Should not crash, diagnostics should be collecting data
        // (Can't easily verify the jitter calculation without more samples)
        std::ignore = diagnostics.currentSnapshot();
    }

    void testVideoFrameCallback()
    {
        StreamingDiagnosticsService diagnostics;
        diagnostics.setEnabled(true);

        auto callback = diagnostics.videoCallback();

        // Simulate frame assembly
        callback.onFrameStarted(1, 1000);          // Frame 1 started at 1ms
        callback.onFrameCompleted(1, 5000, true);  // Frame 1 completed at 5ms

        // Frame should be counted as completed
        // Note: The callback doesn't directly update the counts - that's done via signals
    }

    // ========== Reset ==========

    void testReset()
    {
        StreamingDiagnosticsService diagnostics;
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
        StreamingDiagnosticsService diagnostics;
        diagnostics.setUpdateInterval(100);  // 100ms

        QSignalSpy spy(&diagnostics, &StreamingDiagnosticsService::diagnosticsUpdated);
        diagnostics.setEnabled(true);

        // Wait for a few updates
        QTest::qWait(350);

        // Should have received at least 2-3 updates
        QVERIFY(spy.count() >= 2);
    }

    void testNoUpdatesWhenDisabled()
    {
        StreamingDiagnosticsService diagnostics;
        diagnostics.setUpdateInterval(50);  // Fast updates

        QSignalSpy spy(&diagnostics, &StreamingDiagnosticsService::diagnosticsUpdated);

        // Don't enable - should not receive updates
        QTest::qWait(150);

        QCOMPARE(spy.count(), 0);
    }

    // ========== Callbacks when disabled ==========

    void testCallbacksIgnoredWhenDisabled()
    {
        StreamingDiagnosticsService diagnostics;
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

QTEST_MAIN(TestStreamingDiagnosticsService)
#include "test_streamingdiagnosticsservice.moc"
