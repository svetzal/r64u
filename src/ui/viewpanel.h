#ifndef VIEWPANEL_H
#define VIEWPANEL_H

#include "ui/ipanel.h"

#include <QButtonGroup>
#include <QLabel>
#include <QRadioButton>
#include <QToolBar>
#include <QWidget>

class DeviceConnection;
class ErrorHandler;
class VideoDisplayWidget;
class StreamingService;
class VideoRecordingService;
class ScreenshotService;
class StreamingDiagnosticsWidget;
struct DiagnosticsSnapshot;

class ViewPanel : public QWidget, public IPanel
{
    Q_OBJECT

public:
    QObject *asQObject() override { return this; }

    explicit ViewPanel(DeviceConnection *connection, QWidget *parent = nullptr);
    ~ViewPanel() override;

    // Public API for MainWindow coordination
    void stopStreamingIfActive();

    /**
     * @brief Returns the streaming service for external control.
     * @return Pointer to the streaming service.
     */
    [[nodiscard]] StreamingService *streamingService() const { return streamingService_; }

    /**
     * @brief Injects the streaming service. Wires all streaming-related connections.
     * @param manager Owned by the caller; must outlive this ViewPanel.
     */
    void setStreamingService(StreamingService *manager);

    /**
     * @brief Injects the video recording service. Wires recording-related connections.
     * @param service Owned by the caller; must outlive this ViewPanel.
     */
    void setRecordingService(VideoRecordingService *service);

    /**
     * @brief Injects the screenshot service.
     * @param service Owned by the caller; must outlive this ViewPanel.
     */
    void setScreenshotService(ScreenshotService *service);

    void setErrorHandler(ErrorHandler *handler);

    // Settings
    void loadSettings();
    void saveSettings();
    [[nodiscard]] int scalingMode() const;

private slots:
    void onConnectionStateChanged();
    void onStartStreaming();
    void onStopStreaming();
    void onVideoFormatDetected(int format);
    void onStreamingStarted(const QString &targetHost);
    void onStreamingStopped();
    void onStreamingError(const QString &error);
    void onScalingModeChanged(int id);
    void onCaptureScreenshot();
    void onStartRecording();
    void onStopRecording();
    void onRecordingStarted(const QString &filePath);
    void onRecordingStopped(const QString &filePath, int frameCount);
    void onRecordingError(const QString &error);
    void onStatsToggled(bool checked);
    void onDiagnosticsUpdated(const DiagnosticsSnapshot &snapshot);

private:
    void setupUi();
    void setupConnections();
    void updateActions();

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;

    // Injected services (not owned)
    StreamingService *streamingService_ = nullptr;
    VideoRecordingService *recordingService_ = nullptr;
    ScreenshotService *screenshotService_ = nullptr;

    // UI widgets
    QToolBar *toolBar_ = nullptr;
    VideoDisplayWidget *videoDisplayWidget_ = nullptr;
    QAction *startStreamAction_ = nullptr;
    QAction *stopStreamAction_ = nullptr;
    QAction *captureScreenshotAction_ = nullptr;
    QAction *startRecordingAction_ = nullptr;
    QAction *stopRecordingAction_ = nullptr;
    QLabel *streamStatusLabel_ = nullptr;
    QButtonGroup *scalingModeGroup_ = nullptr;
    QRadioButton *sharpRadio_ = nullptr;
    QRadioButton *smoothRadio_ = nullptr;
    QRadioButton *integerRadio_ = nullptr;
    QAction *statsAction_ = nullptr;
    StreamingDiagnosticsWidget *diagnosticsWidget_ = nullptr;
};

#endif  // VIEWPANEL_H
