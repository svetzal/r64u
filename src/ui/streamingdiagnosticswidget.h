/**
 * @file streamingdiagnosticswidget.h
 * @brief Widget for displaying real-time streaming quality diagnostics.
 */

#ifndef STREAMINGDIAGNOSTICSWIDGET_H
#define STREAMINGDIAGNOSTICSWIDGET_H

#include <QWidget>
#include "services/streamingdiagnostics.h"

class QLabel;
class QFrame;
class QPushButton;

/**
 * @brief Widget for displaying streaming quality diagnostics.
 *
 * Provides two display modes:
 * - Compact: Single line with quality indicator, packet loss, and jitter
 * - Detailed: Full breakdown of all video and audio metrics
 *
 * The quality indicator dot changes color based on overall quality:
 * - Green: Excellent
 * - Yellow-Green: Good
 * - Orange: Fair
 * - Red: Poor
 */
class StreamingDiagnosticsWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Display mode enumeration.
     */
    enum class DisplayMode {
        Compact,    ///< Single line with summary
        Detailed    ///< Full metrics breakdown
    };

    /**
     * @brief Constructs a streaming diagnostics widget.
     * @param parent Optional parent widget.
     */
    explicit StreamingDiagnosticsWidget(QWidget *parent = nullptr);

    /**
     * @brief Sets the display mode.
     * @param mode The display mode (Compact or Detailed).
     */
    void setDisplayMode(DisplayMode mode);

    /**
     * @brief Returns the current display mode.
     * @return The display mode.
     */
    [[nodiscard]] DisplayMode displayMode() const { return displayMode_; }

    /**
     * @brief Toggles between compact and detailed modes.
     */
    void toggleDisplayMode();

public slots:
    /**
     * @brief Updates the display with new diagnostics data.
     * @param snapshot The diagnostics snapshot to display.
     */
    void updateDiagnostics(const DiagnosticsSnapshot &snapshot);

    /**
     * @brief Clears the display and shows no data state.
     */
    void clear();

private:
    void setupUi();
    void updateCompactDisplay(const DiagnosticsSnapshot &snapshot);
    void updateDetailedDisplay(const DiagnosticsSnapshot &snapshot);
    void updateQualityIndicator(QualityLevel level);
    static QString formatBytes(quint64 bytes);

    DisplayMode displayMode_ = DisplayMode::Compact;

    // Compact mode widgets
    QFrame *compactFrame_ = nullptr;
    QWidget *qualityDot_ = nullptr;
    QLabel *summaryLabel_ = nullptr;
    QPushButton *expandButton_ = nullptr;

    // Detailed mode widgets
    QFrame *detailedFrame_ = nullptr;
    QLabel *qualityLabel_ = nullptr;
    QLabel *uptimeLabel_ = nullptr;

    // Video network metrics labels
    QLabel *videoPacketsLabel_ = nullptr;
    QLabel *videoLossLabel_ = nullptr;
    QLabel *videoFramesLabel_ = nullptr;
    QLabel *videoCompletionLabel_ = nullptr;
    QLabel *videoJitterLabel_ = nullptr;
    QLabel *videoAssemblyLabel_ = nullptr;

    // Video playback metrics labels
    QLabel *videoDisplayBufferLabel_ = nullptr;
    QLabel *videoDisplayJitterLabel_ = nullptr;

    // Audio network metrics labels
    QLabel *audioPacketsLabel_ = nullptr;
    QLabel *audioLossLabel_ = nullptr;
    QLabel *audioBufferLabel_ = nullptr;
    QLabel *audioUnderrunsLabel_ = nullptr;
    QLabel *audioJitterLabel_ = nullptr;

    // Audio playback metrics labels
    QLabel *audioWriteJitterLabel_ = nullptr;
    QLabel *audioDroppedLabel_ = nullptr;
};

#endif // STREAMINGDIAGNOSTICSWIDGET_H
