/**
 * @file test_audioplaybackservice.cpp
 * @brief Unit tests for AudioPlaybackService.
 *
 * Tests cover state, configuration, and signal behavior.  Hardware-dependent
 * tests (those requiring a real audio output device) are guarded with
 * QMediaDevices::defaultAudioOutput().isNull() and skipped when no audio
 * hardware is available (e.g. in CI environments).
 */

#include "services/audioplaybackservice.h"

#include <QAudioDevice>
#include <QMediaDevices>
#include <QSignalSpy>
#include <QtTest>

class TestAudioPlaybackService : public QObject
{
    Q_OBJECT

private:
    // Returns true when a usable audio output device is present
    static bool hasAudioDevice() { return !QMediaDevices::defaultAudioOutput().isNull(); }

private slots:
    void init() { service_ = new AudioPlaybackService(this); }

    void cleanup()
    {
        service_->stop();
        delete service_;
        service_ = nullptr;
    }

    // =========================================================
    // Initial state
    // =========================================================

    void testInitialState_NotPlaying() { QVERIFY(!service_->isPlaying()); }

    void testInitialState_DefaultSampleRate()
    {
        QCOMPARE(service_->sampleRate(), AudioPlaybackService::DefaultSampleRate);
    }

    // =========================================================
    // setSampleRate / sampleRate
    // =========================================================

    void testSetSampleRate_UpdatesValue()
    {
        service_->setSampleRate(44100);

        QCOMPARE(service_->sampleRate(), 44100);
    }

    void testSetSampleRate_AnotherValue()
    {
        service_->setSampleRate(48000);

        QCOMPARE(service_->sampleRate(), 48000);
    }

    // =========================================================
    // Constants
    // =========================================================

    void testConstants_DefaultSampleRate()
    {
        QCOMPARE(AudioPlaybackService::DefaultSampleRate, 48000);
    }

    void testConstants_Channels() { QCOMPARE(AudioPlaybackService::Channels, 2); }

    void testConstants_BitsPerSample() { QCOMPARE(AudioPlaybackService::BitsPerSample, 16); }

    void testConstants_BytesPerFrame() { QCOMPARE(AudioPlaybackService::BytesPerFrame, 4); }

    // =========================================================
    // stop() — safe when not playing
    // =========================================================

    void testStop_WhenNotPlaying_IsNoOp()
    {
        // Should not crash or assert
        service_->stop();
        QVERIFY(!service_->isPlaying());
    }

    // =========================================================
    // Hardware-dependent tests (skipped in CI without audio)
    // =========================================================

    void testStart_WithAudioDevice_ReturnsTrue()
    {
        if (!hasAudioDevice()) {
            QSKIP("No audio output device available");
        }

        bool started = service_->start();

        QVERIFY(started);
    }

    void testStart_WithAudioDevice_SetsPlaying()
    {
        if (!hasAudioDevice()) {
            QSKIP("No audio output device available");
        }

        service_->start();

        QVERIFY(service_->isPlaying());
    }

    void testStart_WithAudioDevice_EmitsPlaybackStateChangedTrue()
    {
        if (!hasAudioDevice()) {
            QSKIP("No audio output device available");
        }

        QSignalSpy spy(service_, &AudioPlaybackService::playbackStateChanged);

        service_->start();

        QVERIFY(spy.count() >= 1);
        QVERIFY(spy.last().first().toBool());
    }

    void testStop_AfterStart_SetsNotPlaying()
    {
        if (!hasAudioDevice()) {
            QSKIP("No audio output device available");
        }

        service_->start();
        service_->stop();

        QVERIFY(!service_->isPlaying());
    }

    void testStop_AfterStart_EmitsPlaybackStateChangedFalse()
    {
        if (!hasAudioDevice()) {
            QSKIP("No audio output device available");
        }

        service_->start();

        QSignalSpy spy(service_, &AudioPlaybackService::playbackStateChanged);

        service_->stop();

        QVERIFY(spy.count() >= 1);
        QVERIFY(!spy.last().first().toBool());
    }

    void testSetVolume_WithAudioDevice_UpdatesVolume()
    {
        if (!hasAudioDevice()) {
            QSKIP("No audio output device available");
        }

        service_->start();
        service_->setVolume(0.5);

        // Volume should be in [0.0, 1.0]; just verify it doesn't crash
        qreal vol = service_->volume();
        QVERIFY(vol >= 0.0 && vol <= 1.0);
    }

    void testWriteSamples_WithAudioDevice_DoesNotCrash()
    {
        if (!hasAudioDevice()) {
            QSKIP("No audio output device available");
        }

        service_->start();

        // Write one packet worth of silence
        constexpr int sampleCount = 192;
        QByteArray silence(
            static_cast<qsizetype>(sampleCount) * AudioPlaybackService::BytesPerFrame, '\0');
        service_->writeSamples(silence, sampleCount);

        QVERIFY(true);  // no crash
    }

private:
    AudioPlaybackService *service_ = nullptr;
};

QTEST_MAIN(TestAudioPlaybackService)
#include "test_audioplaybackservice.moc"
