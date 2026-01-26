/**
 * @file streamingdiagnostics.cpp
 * @brief Implementation of the streaming diagnostics service.
 */

#include "streamingdiagnostics.h"
#include "videostreamreceiver.h"
#include "audiostreamreceiver.h"
#include <QTimer>
#include <QColor>
#include <QMutexLocker>

StreamingDiagnostics::StreamingDiagnostics(QObject *parent)
    : QObject(parent)
    , updateTimer_(new QTimer(this))
{
    updateTimer_->setInterval(DefaultUpdateIntervalMs);
    connect(updateTimer_, &QTimer::timeout,
            this, &StreamingDiagnostics::onUpdateTimer);
}

StreamingDiagnostics::~StreamingDiagnostics() = default;

void StreamingDiagnostics::setEnabled(bool enabled)
{
    if (enabled_ == enabled) {
        return;
    }

    enabled_ = enabled;

    if (enabled) {
        reset();
        uptimeTimer_.start();
        updateTimer_->start();
    } else {
        updateTimer_->stop();
    }
}

void StreamingDiagnostics::attachVideoReceiver(VideoStreamReceiver *receiver)
{
    if (videoReceiver_) {
        disconnect(videoReceiver_, nullptr, this, nullptr);
    }

    videoReceiver_ = receiver;

    if (videoReceiver_) {
        connect(videoReceiver_, &VideoStreamReceiver::statsUpdated,
                this, &StreamingDiagnostics::onVideoStatsUpdated);
    }
}

void StreamingDiagnostics::attachAudioReceiver(AudioStreamReceiver *receiver)
{
    if (audioReceiver_) {
        disconnect(audioReceiver_, nullptr, this, nullptr);
    }

    audioReceiver_ = receiver;

    if (audioReceiver_) {
        connect(audioReceiver_, &AudioStreamReceiver::statsUpdated,
                this, &StreamingDiagnostics::onAudioStatsUpdated);
        connect(audioReceiver_, &AudioStreamReceiver::bufferUnderrun,
                this, &StreamingDiagnostics::onAudioBufferUnderrun);

        audioBufferTarget_ = audioReceiver_->jitterBufferSize();
    }
}

void StreamingDiagnostics::setUpdateInterval(int intervalMs)
{
    updateTimer_->setInterval(intervalMs);
}

DiagnosticsSnapshot StreamingDiagnostics::currentSnapshot() const
{
    DiagnosticsSnapshot snapshot;

    // Timing
    snapshot.uptimeMs = uptimeTimer_.isValid() ? uptimeTimer_.elapsed() : 0;

    // Video metrics
    snapshot.videoPacketsReceived = videoPacketsReceived_;
    snapshot.videoPacketsLost = videoPacketsLost_;
    snapshot.videoFramesCompleted = videoFramesCompleted_;
    snapshot.videoFramesIncomplete = videoFramesIncomplete_;
    snapshot.videoOutOfOrderPackets = videoOutOfOrderPackets_;

    if (videoPacketsReceived_ + videoPacketsLost_ > 0) {
        snapshot.videoPacketLossPercent = 100.0 * static_cast<double>(videoPacketsLost_) /
            static_cast<double>(videoPacketsReceived_ + videoPacketsLost_);
    }

    if (videoFramesCompleted_ + videoFramesIncomplete_ > 0) {
        snapshot.videoFrameCompletionPercent = 100.0 * static_cast<double>(videoFramesCompleted_) /
            static_cast<double>(videoFramesCompleted_ + videoFramesIncomplete_);
    }

    // Convert microseconds to milliseconds for jitter
    snapshot.videoPacketJitterMs = videoPacketIntervalStats_.stddev() / 1000.0;
    snapshot.videoFrameAssemblyTimeMs = videoFrameAssemblyStats_.mean() / 1000.0;

    // Video playback metrics
    snapshot.videoFrameBufferLevel = videoFrameBufferLevel_;
    snapshot.videoDisplayJitterMs = videoDisplayIntervalStats_.stddev() / 1000.0;
    snapshot.videoDisplayUnderruns = videoDisplayUnderruns_;

    // Audio metrics
    snapshot.audioPacketsReceived = audioPacketsReceived_;
    snapshot.audioPacketsLost = audioPacketsLost_;
    snapshot.audioBufferLevel = audioBufferLevel_;
    snapshot.audioBufferTarget = audioBufferTarget_;
    snapshot.audioBufferUnderruns = audioBufferUnderruns_;
    snapshot.audioSampleDiscontinuities = audioSampleDiscontinuities_;

    if (audioPacketsReceived_ + audioPacketsLost_ > 0) {
        snapshot.audioPacketLossPercent = 100.0 * static_cast<double>(audioPacketsLost_) /
            static_cast<double>(audioPacketsReceived_ + audioPacketsLost_);
    }

    snapshot.audioPacketJitterMs = audioPacketIntervalStats_.stddev() / 1000.0;

    // Audio playback metrics
    snapshot.audioSamplesWritten = audioSamplesWritten_;
    snapshot.audioSamplesDropped = audioSamplesDropped_;
    snapshot.audioWriteJitterMs = audioWriteIntervalStats_.stddev() / 1000.0;
    snapshot.audioPlaybackUnderruns = audioPlaybackUnderruns_;

    // Overall quality
    snapshot.overallQuality = calculateQualityLevel();

    return snapshot;
}

void StreamingDiagnostics::reset()
{
    videoPacketsReceived_ = 0;
    videoPacketsLost_ = 0;
    videoFramesCompleted_ = 0;
    videoFramesIncomplete_ = 0;
    videoOutOfOrderPackets_ = 0;
    lastVideoPacketTimeUs_ = 0;
    videoPacketIntervalStats_.clear();
    videoFrameAssemblyStats_.clear();
    currentFrameNumber_ = 0;
    currentFrameStartUs_ = 0;

    // Video display stats
    videoFrameBufferLevel_ = 0;
    videoDisplayUnderruns_ = 0;
    lastVideoDisplayTimeUs_ = 0;
    videoDisplayIntervalStats_.clear();

    audioPacketsReceived_ = 0;
    audioPacketsLost_ = 0;
    audioBufferUnderruns_ = 0;
    audioSampleDiscontinuities_ = 0;
    audioBufferLevel_ = 0;
    lastAudioPacketTimeUs_ = 0;
    audioPacketIntervalStats_.clear();

    // Audio playback stats
    audioSamplesWritten_ = 0;
    audioSamplesDropped_ = 0;
    audioPlaybackUnderruns_ = 0;
    lastAudioWriteTimeUs_ = 0;
    audioWriteIntervalStats_.clear();

    uptimeTimer_.restart();
}

VideoDiagnosticsCallback StreamingDiagnostics::videoCallback()
{
    VideoDiagnosticsCallback callback;

    callback.onPacketReceived = [this](qint64 arrivalTimeUs) {
        handleVideoPacket(arrivalTimeUs);
    };

    callback.onFrameStarted = [this](quint16 frameNumber, qint64 startTimeUs) {
        handleVideoFrameStart(frameNumber, startTimeUs);
    };

    callback.onFrameCompleted = [this](quint16 frameNumber, qint64 endTimeUs, bool complete) {
        handleVideoFrameComplete(frameNumber, endTimeUs, complete);
    };

    callback.onOutOfOrderPacket = [this]() {
        handleVideoOutOfOrder();
    };

    return callback;
}

AudioDiagnosticsCallback StreamingDiagnostics::audioCallback()
{
    AudioDiagnosticsCallback callback;

    callback.onPacketReceived = [this](qint64 arrivalTimeUs) {
        handleAudioPacket(arrivalTimeUs);
    };

    callback.onBufferUnderrun = [this]() {
        handleAudioBufferUnderrun();
    };

    callback.onSampleDiscontinuity = [this](int gap) {
        handleAudioDiscontinuity(gap);
    };

    return callback;
}

QString StreamingDiagnostics::qualityLevelString(QualityLevel level)
{
    switch (level) {
    case QualityLevel::Excellent:
        return tr("Excellent");
    case QualityLevel::Good:
        return tr("Good");
    case QualityLevel::Fair:
        return tr("Fair");
    case QualityLevel::Poor:
        return tr("Poor");
    case QualityLevel::Unknown:
    default:
        return tr("Unknown");
    }
}

QColor StreamingDiagnostics::qualityLevelColor(QualityLevel level)
{
    switch (level) {
    case QualityLevel::Excellent:
        return {0, 200, 0};      // Green
    case QualityLevel::Good:
        return {150, 200, 0};    // Yellow-Green
    case QualityLevel::Fair:
        return {255, 165, 0};    // Orange
    case QualityLevel::Poor:
        return {200, 0, 0};      // Red
    case QualityLevel::Unknown:
    default:
        return {128, 128, 128};  // Grey
    }
}

void StreamingDiagnostics::onUpdateTimer()
{
    if (!enabled_) {
        return;
    }

    DiagnosticsSnapshot snapshot = currentSnapshot();
    emit diagnosticsUpdated(snapshot);
}

void StreamingDiagnostics::onVideoStatsUpdated(quint64 packetsReceived, quint64 framesCompleted, quint64 packetsLost)
{
    if (!enabled_) {
        return;
    }

    videoPacketsReceived_ = packetsReceived;
    videoFramesCompleted_ = framesCompleted;
    videoPacketsLost_ = packetsLost;
}

void StreamingDiagnostics::onAudioStatsUpdated(quint64 packetsReceived, quint64 packetsLost, int bufferLevel)
{
    if (!enabled_) {
        return;
    }

    audioPacketsReceived_ = packetsReceived;
    audioPacketsLost_ = packetsLost;
    audioBufferLevel_ = bufferLevel;
}

void StreamingDiagnostics::onAudioBufferUnderrun()
{
    if (enabled_) {
        audioBufferUnderruns_++;
    }
}

QualityLevel StreamingDiagnostics::calculateQualityLevel() const
{
    // Need some data to calculate quality
    if (videoPacketsReceived_ < 100 && audioPacketsReceived_ < 100) {
        return QualityLevel::Unknown;
    }

    double packetLoss = 0.0;
    double jitterMs = 0.0;
    double frameCompletion = 100.0;

    // Use worse of video/audio metrics
    if (videoPacketsReceived_ > 0) {
        double videoLoss = 100.0 * static_cast<double>(videoPacketsLost_) /
            static_cast<double>(videoPacketsReceived_ + videoPacketsLost_);
        packetLoss = std::max(packetLoss, videoLoss);
        jitterMs = std::max(jitterMs, videoPacketIntervalStats_.stddev() / 1000.0);

        if (videoFramesCompleted_ + videoFramesIncomplete_ > 0) {
            frameCompletion = 100.0 * static_cast<double>(videoFramesCompleted_) /
                static_cast<double>(videoFramesCompleted_ + videoFramesIncomplete_);
        }
    }

    if (audioPacketsReceived_ > 0) {
        double audioLoss = 100.0 * static_cast<double>(audioPacketsLost_) /
            static_cast<double>(audioPacketsReceived_ + audioPacketsLost_);
        packetLoss = std::max(packetLoss, audioLoss);
        jitterMs = std::max(jitterMs, audioPacketIntervalStats_.stddev() / 1000.0);
    }

    // Quality thresholds
    // Excellent: < 0.1% loss, < 5ms jitter, > 99.9% completion
    if (packetLoss < 0.1 && jitterMs < 5.0 && frameCompletion > 99.9) {
        return QualityLevel::Excellent;
    }

    // Good: < 1% loss, < 10ms jitter, > 99% completion
    if (packetLoss < 1.0 && jitterMs < 10.0 && frameCompletion > 99.0) {
        return QualityLevel::Good;
    }

    // Fair: < 5% loss, < 20ms jitter, > 95% completion
    if (packetLoss < 5.0 && jitterMs < 20.0 && frameCompletion > 95.0) {
        return QualityLevel::Fair;
    }

    return QualityLevel::Poor;
}

void StreamingDiagnostics::handleVideoPacket(qint64 arrivalTimeUs)
{
    if (!enabled_) {
        return;
    }

    if (lastVideoPacketTimeUs_ > 0) {
        qint64 interval = arrivalTimeUs - lastVideoPacketTimeUs_;
        videoPacketIntervalStats_.addSample(static_cast<double>(interval));
    }
    lastVideoPacketTimeUs_ = arrivalTimeUs;
}

void StreamingDiagnostics::handleVideoFrameStart(quint16 frameNumber, qint64 startTimeUs)
{
    if (!enabled_) {
        return;
    }

    currentFrameNumber_ = frameNumber;
    currentFrameStartUs_ = startTimeUs;
}

void StreamingDiagnostics::handleVideoFrameComplete(quint16 frameNumber, qint64 endTimeUs, bool complete)
{
    if (!enabled_) {
        return;
    }

    if (frameNumber == currentFrameNumber_ && currentFrameStartUs_ > 0) {
        qint64 assemblyTime = endTimeUs - currentFrameStartUs_;
        videoFrameAssemblyStats_.addSample(static_cast<double>(assemblyTime));
    }

    if (complete) {
        videoFramesCompleted_++;
    } else {
        videoFramesIncomplete_++;
    }

    currentFrameStartUs_ = 0;
}

void StreamingDiagnostics::handleVideoOutOfOrder()
{
    if (enabled_) {
        videoOutOfOrderPackets_++;
    }
}

void StreamingDiagnostics::handleAudioPacket(qint64 arrivalTimeUs)
{
    if (!enabled_) {
        return;
    }

    if (lastAudioPacketTimeUs_ > 0) {
        qint64 interval = arrivalTimeUs - lastAudioPacketTimeUs_;
        audioPacketIntervalStats_.addSample(static_cast<double>(interval));
    }
    lastAudioPacketTimeUs_ = arrivalTimeUs;
}

void StreamingDiagnostics::handleAudioBufferUnderrun()
{
    if (enabled_) {
        audioBufferUnderruns_++;
    }
}

void StreamingDiagnostics::handleAudioDiscontinuity(int gap)
{
    Q_UNUSED(gap)
    if (enabled_) {
        audioSampleDiscontinuities_++;
    }
}

AudioPlaybackDiagnosticsCallback StreamingDiagnostics::audioPlaybackCallback()
{
    AudioPlaybackDiagnosticsCallback callback;

    callback.onSamplesWritten = [this](qint64 writeTimeUs, qint64 bytesWritten, qint64 bytesDropped) {
        handleAudioSamplesWritten(writeTimeUs, bytesWritten, bytesDropped);
    };

    callback.onPlaybackUnderrun = [this]() {
        handleAudioPlaybackUnderrun();
    };

    return callback;
}

VideoDisplayDiagnosticsCallback StreamingDiagnostics::videoDisplayCallback()
{
    VideoDisplayDiagnosticsCallback callback;

    callback.onFrameDisplayed = [this](qint64 displayTimeUs) {
        handleVideoFrameDisplayed(displayTimeUs);
    };

    callback.onDisplayUnderrun = [this]() {
        handleVideoDisplayUnderrun();
    };

    callback.onBufferLevelChanged = [this](int bufferLevel) {
        handleVideoBufferLevelChanged(bufferLevel);
    };

    return callback;
}

void StreamingDiagnostics::handleAudioSamplesWritten(qint64 writeTimeUs, qint64 bytesWritten, qint64 bytesDropped)
{
    if (!enabled_) {
        return;
    }

    audioSamplesWritten_ += static_cast<quint64>(bytesWritten);
    audioSamplesDropped_ += static_cast<quint64>(bytesDropped);

    if (lastAudioWriteTimeUs_ > 0) {
        qint64 interval = writeTimeUs - lastAudioWriteTimeUs_;
        audioWriteIntervalStats_.addSample(static_cast<double>(interval));
    }
    lastAudioWriteTimeUs_ = writeTimeUs;
}

void StreamingDiagnostics::handleAudioPlaybackUnderrun()
{
    if (enabled_) {
        audioPlaybackUnderruns_++;
    }
}

void StreamingDiagnostics::handleVideoFrameDisplayed(qint64 displayTimeUs)
{
    if (!enabled_) {
        return;
    }

    if (lastVideoDisplayTimeUs_ > 0) {
        qint64 interval = displayTimeUs - lastVideoDisplayTimeUs_;
        videoDisplayIntervalStats_.addSample(static_cast<double>(interval));
    }
    lastVideoDisplayTimeUs_ = displayTimeUs;
}

void StreamingDiagnostics::handleVideoDisplayUnderrun()
{
    if (enabled_) {
        videoDisplayUnderruns_++;
    }
}

void StreamingDiagnostics::handleVideoBufferLevelChanged(int bufferLevel)
{
    if (enabled_) {
        videoFrameBufferLevel_ = bufferLevel;
    }
}
