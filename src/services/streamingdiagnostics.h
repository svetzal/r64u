/**
 * @file streamingdiagnostics.h
 * @brief Real-time streaming quality diagnostics service.
 *
 * Collects and aggregates metrics from video and audio stream receivers
 * to provide quality indicators and diagnostic information.
 */

#ifndef STREAMINGDIAGNOSTICS_H
#define STREAMINGDIAGNOSTICS_H

#include <QObject>
#include <QElapsedTimer>
#include <functional>
#include "utils/rollingstats.h"

class VideoStreamReceiver;
class AudioStreamReceiver;
class AudioPlaybackService;
class QTimer;

/**
 * @brief Quality level classification for streaming.
 */
enum class QualityLevel {
    Unknown,    ///< Not enough data to determine quality
    Excellent,  ///< < 0.1% loss, < 5ms jitter, > 99.9% completion
    Good,       ///< < 1% loss, < 10ms jitter, > 99% completion
    Fair,       ///< < 5% loss, < 20ms jitter, > 95% completion
    Poor        ///< >= 5% loss, >= 20ms jitter, < 95% completion
};

/**
 * @brief Snapshot of streaming diagnostics at a point in time.
 */
struct DiagnosticsSnapshot
{
    // Overall quality
    QualityLevel overallQuality = QualityLevel::Unknown;

    // Video network metrics
    quint64 videoPacketsReceived = 0;
    quint64 videoPacketsLost = 0;
    quint64 videoFramesCompleted = 0;
    quint64 videoFramesIncomplete = 0;
    double videoPacketLossPercent = 0.0;
    double videoFrameCompletionPercent = 100.0;
    double videoPacketJitterMs = 0.0;        ///< stddev of inter-packet times
    double videoFrameAssemblyTimeMs = 0.0;   ///< Average time to assemble a frame
    quint64 videoOutOfOrderPackets = 0;

    // Video playback metrics
    int videoFrameBufferLevel = 0;           ///< Frames waiting in display buffer
    double videoDisplayJitterMs = 0.0;       ///< stddev of inter-display times
    quint64 videoDisplayUnderruns = 0;       ///< Frames missed due to empty buffer

    // Audio network metrics
    quint64 audioPacketsReceived = 0;
    quint64 audioPacketsLost = 0;
    double audioPacketLossPercent = 0.0;
    double audioPacketJitterMs = 0.0;
    int audioBufferLevel = 0;
    int audioBufferTarget = 0;
    quint64 audioBufferUnderruns = 0;
    quint64 audioSampleDiscontinuities = 0;

    // Audio playback metrics
    quint64 audioSamplesWritten = 0;         ///< Bytes successfully written
    quint64 audioSamplesDropped = 0;         ///< Bytes dropped due to full buffer
    double audioWriteJitterMs = 0.0;         ///< stddev of inter-write times
    quint64 audioPlaybackUnderruns = 0;      ///< Playback buffer underruns

    // Timing
    qint64 uptimeMs = 0;  ///< Time since diagnostics started
};

/**
 * @brief Callback interface for high-frequency video timing data.
 */
struct VideoDiagnosticsCallback
{
    std::function<void(qint64 arrivalTimeUs)> onPacketReceived;
    std::function<void(quint16 frameNumber, qint64 startTimeUs)> onFrameStarted;
    std::function<void(quint16 frameNumber, qint64 endTimeUs, bool complete)> onFrameCompleted;
    std::function<void()> onOutOfOrderPacket;
};

/**
 * @brief Callback interface for high-frequency audio timing data.
 */
struct AudioDiagnosticsCallback
{
    std::function<void(qint64 arrivalTimeUs)> onPacketReceived;
    std::function<void()> onBufferUnderrun;
    std::function<void(int gap)> onSampleDiscontinuity;
};

/**
 * @brief Callback interface for audio playback timing data.
 */
struct AudioPlaybackDiagnosticsCallback
{
    std::function<void(qint64 writeTimeUs, qint64 bytesWritten, qint64 bytesDropped)> onSamplesWritten;
    std::function<void()> onPlaybackUnderrun;
};

/**
 * @brief Callback interface for video display timing data.
 */
struct VideoDisplayDiagnosticsCallback
{
    std::function<void(qint64 displayTimeUs)> onFrameDisplayed;
    std::function<void()> onDisplayUnderrun;
    std::function<void(int bufferLevel)> onBufferLevelChanged;
};

/**
 * @brief Real-time streaming quality diagnostics service.
 *
 * This service collects metrics from video and audio stream receivers,
 * calculates rolling statistics, and emits periodic updates with
 * quality assessments.
 *
 * @par Example usage:
 * @code
 * StreamingDiagnostics *diagnostics = new StreamingDiagnostics(this);
 * diagnostics->attachVideoReceiver(videoReceiver);
 * diagnostics->attachAudioReceiver(audioReceiver);
 *
 * connect(diagnostics, &StreamingDiagnostics::diagnosticsUpdated,
 *         this, &MyClass::onDiagnosticsUpdate);
 *
 * diagnostics->setEnabled(true);
 * @endcode
 */
class StreamingDiagnostics : public QObject
{
    Q_OBJECT

public:
    /// Default update interval in milliseconds
    static constexpr int DefaultUpdateIntervalMs = 500;

    /// Rolling window size for statistics
    static constexpr size_t StatisticsWindowSize = 100;

    /**
     * @brief Constructs a streaming diagnostics service.
     * @param parent Optional parent QObject for memory management.
     */
    explicit StreamingDiagnostics(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~StreamingDiagnostics() override;

    /**
     * @brief Enables or disables diagnostics collection.
     * @param enabled true to enable, false to disable.
     *
     * When disabled, no metrics are collected and no signals are emitted,
     * ensuring zero overhead when not in use.
     */
    void setEnabled(bool enabled);

    /**
     * @brief Returns whether diagnostics collection is enabled.
     * @return true if enabled, false otherwise.
     */
    [[nodiscard]] bool isEnabled() const { return enabled_; }

    /**
     * @brief Attaches a video stream receiver for monitoring.
     * @param receiver The video receiver to monitor.
     */
    void attachVideoReceiver(VideoStreamReceiver *receiver);

    /**
     * @brief Attaches an audio stream receiver for monitoring.
     * @param receiver The audio receiver to monitor.
     */
    void attachAudioReceiver(AudioStreamReceiver *receiver);

    /**
     * @brief Sets the update interval for diagnostics updates.
     * @param intervalMs Interval in milliseconds.
     */
    void setUpdateInterval(int intervalMs);

    /**
     * @brief Returns the current diagnostics snapshot.
     * @return Current diagnostics data.
     */
    [[nodiscard]] DiagnosticsSnapshot currentSnapshot() const;

    /**
     * @brief Resets all collected statistics.
     */
    void reset();

    /**
     * @brief Returns the video diagnostics callback for the receiver to use.
     * @return Callback structure with timing functions.
     */
    [[nodiscard]] VideoDiagnosticsCallback videoCallback();

    /**
     * @brief Returns the audio diagnostics callback for the receiver to use.
     * @return Callback structure with timing functions.
     */
    [[nodiscard]] AudioDiagnosticsCallback audioCallback();

    /**
     * @brief Returns the audio playback diagnostics callback.
     * @return Callback structure for playback timing functions.
     */
    [[nodiscard]] AudioPlaybackDiagnosticsCallback audioPlaybackCallback();

    /**
     * @brief Returns the video display diagnostics callback.
     * @return Callback structure for display timing functions.
     */
    [[nodiscard]] VideoDisplayDiagnosticsCallback videoDisplayCallback();

    /**
     * @brief Returns a string representation of a quality level.
     * @param level The quality level.
     * @return Human-readable quality string.
     */
    static QString qualityLevelString(QualityLevel level);

    /**
     * @brief Returns a color for a quality level.
     * @param level The quality level.
     * @return Color suitable for UI display.
     */
    static QColor qualityLevelColor(QualityLevel level);

signals:
    /**
     * @brief Emitted periodically with updated diagnostics.
     * @param snapshot Current diagnostics data.
     */
    void diagnosticsUpdated(const DiagnosticsSnapshot &snapshot);

private slots:
    void onUpdateTimer();
    void onVideoStatsUpdated(quint64 packetsReceived, quint64 framesCompleted, quint64 packetsLost);
    void onAudioStatsUpdated(quint64 packetsReceived, quint64 packetsLost, int bufferLevel);
    void onAudioBufferUnderrun();

private:
    [[nodiscard]] QualityLevel calculateQualityLevel() const;

    // Callback handlers (called from receiver threads)
    void handleVideoPacket(qint64 arrivalTimeUs);
    void handleVideoFrameStart(quint16 frameNumber, qint64 startTimeUs);
    void handleVideoFrameComplete(quint16 frameNumber, qint64 endTimeUs, bool complete);
    void handleVideoOutOfOrder();
    void handleAudioPacket(qint64 arrivalTimeUs);
    void handleAudioBufferUnderrun();
    void handleAudioDiscontinuity(int gap);

    // Playback callback handlers
    void handleAudioSamplesWritten(qint64 writeTimeUs, qint64 bytesWritten, qint64 bytesDropped);
    void handleAudioPlaybackUnderrun();
    void handleVideoFrameDisplayed(qint64 displayTimeUs);
    void handleVideoDisplayUnderrun();
    void handleVideoBufferLevelChanged(int bufferLevel);

    bool enabled_ = false;
    QTimer *updateTimer_ = nullptr;
    QElapsedTimer uptimeTimer_;

    // Attached receivers (not owned)
    VideoStreamReceiver *videoReceiver_ = nullptr;
    AudioStreamReceiver *audioReceiver_ = nullptr;

    // Video statistics
    quint64 videoPacketsReceived_ = 0;
    quint64 videoPacketsLost_ = 0;
    quint64 videoFramesCompleted_ = 0;
    quint64 videoFramesIncomplete_ = 0;
    quint64 videoOutOfOrderPackets_ = 0;
    qint64 lastVideoPacketTimeUs_ = 0;
    RollingStats videoPacketIntervalStats_{StatisticsWindowSize};
    RollingStats videoFrameAssemblyStats_{StatisticsWindowSize};

    // Frame assembly tracking
    quint16 currentFrameNumber_ = 0;
    qint64 currentFrameStartUs_ = 0;

    // Audio statistics
    quint64 audioPacketsReceived_ = 0;
    quint64 audioPacketsLost_ = 0;
    quint64 audioBufferUnderruns_ = 0;
    quint64 audioSampleDiscontinuities_ = 0;
    int audioBufferLevel_ = 0;
    int audioBufferTarget_ = 10;  // Default jitter buffer size
    qint64 lastAudioPacketTimeUs_ = 0;
    RollingStats audioPacketIntervalStats_{StatisticsWindowSize};

    // Audio playback statistics
    quint64 audioSamplesWritten_ = 0;
    quint64 audioSamplesDropped_ = 0;
    quint64 audioPlaybackUnderruns_ = 0;
    qint64 lastAudioWriteTimeUs_ = 0;
    RollingStats audioWriteIntervalStats_{StatisticsWindowSize};

    // Video display statistics
    int videoFrameBufferLevel_ = 0;
    quint64 videoDisplayUnderruns_ = 0;
    qint64 lastVideoDisplayTimeUs_ = 0;
    RollingStats videoDisplayIntervalStats_{StatisticsWindowSize};
};

#endif // STREAMINGDIAGNOSTICS_H
