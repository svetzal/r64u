#include "viewpanel.h"

#include "streamingdiagnosticswidget.h"
#include "videodisplaywidget.h"

#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/keyboardinputservice.h"
#include "services/screenshotservice.h"
#include "services/streamingdiagnosticsservice.h"
#include "services/streamingservice.h"
#include "services/videorecordingservice.h"
#include "services/videostreamreceiver.h"
#include "utils/logging.h"

#include <QFileInfo>
#include <QKeyEvent>
#include <QSettings>
#include <QVBoxLayout>

ViewPanel::ViewPanel(DeviceConnectionManager *connection, QWidget *parent)
    : QWidget(parent), deviceConnection_(connection)
{
    Q_ASSERT(deviceConnection_ && "DeviceConnectionManager is required");
    setupUi();
}

ViewPanel::~ViewPanel() = default;

void ViewPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create toolbar
    toolBar_ = new QToolBar();
    toolBar_->setMovable(false);
    toolBar_->setIconSize(QSize(16, 16));

    startStreamAction_ = toolBar_->addAction(tr("Start Stream"));
    startStreamAction_->setToolTip(tr("Start video and audio streaming"));
    connect(startStreamAction_, &QAction::triggered, this, &ViewPanel::onStartStreaming);

    stopStreamAction_ = toolBar_->addAction(tr("Stop Stream"));
    stopStreamAction_->setToolTip(tr("Stop streaming"));
    stopStreamAction_->setEnabled(false);
    connect(stopStreamAction_, &QAction::triggered, this, &ViewPanel::onStopStreaming);

    toolBar_->addSeparator();

    captureScreenshotAction_ = toolBar_->addAction(tr("Screenshot"));
    captureScreenshotAction_->setToolTip(tr("Capture screenshot (saves to Pictures folder)"));
    captureScreenshotAction_->setEnabled(false);
    connect(captureScreenshotAction_, &QAction::triggered, this, &ViewPanel::onCaptureScreenshot);

    startRecordingAction_ = toolBar_->addAction(tr("Record"));
    startRecordingAction_->setToolTip(tr("Start recording video"));
    startRecordingAction_->setEnabled(false);
    connect(startRecordingAction_, &QAction::triggered, this, &ViewPanel::onStartRecording);

    stopRecordingAction_ = toolBar_->addAction(tr("Stop Recording"));
    stopRecordingAction_->setToolTip(tr("Stop recording video"));
    stopRecordingAction_->setEnabled(false);
    connect(stopRecordingAction_, &QAction::triggered, this, &ViewPanel::onStopRecording);

    statsAction_ = toolBar_->addAction(tr("Stats"));
    statsAction_->setToolTip(tr("Toggle streaming statistics display"));
    statsAction_->setCheckable(true);
    statsAction_->setEnabled(false);
    connect(statsAction_, &QAction::toggled, this, &ViewPanel::onStatsToggled);

    toolBar_->addSeparator();

    streamStatusLabel_ = new QLabel(tr("Not streaming"));
    toolBar_->addWidget(streamStatusLabel_);

    // Add spacer to push scaling mode to the right
    auto *spacer = new QWidget();
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    toolBar_->addWidget(spacer);

    // Add scaling mode radio buttons
    auto *scalingLabel = new QLabel(tr("Scale:"));
    toolBar_->addWidget(scalingLabel);

    scalingModeGroup_ = new QButtonGroup(this);

    sharpRadio_ = new QRadioButton(tr("Sharp"));
    sharpRadio_->setToolTip(tr("Nearest-neighbor scaling - crisp pixels"));
    smoothRadio_ = new QRadioButton(tr("Smooth"));
    smoothRadio_->setToolTip(tr("Bilinear interpolation - smooth but fuzzy"));
    integerRadio_ = new QRadioButton(tr("Integer"));
    integerRadio_->setToolTip(tr("Integer scaling with letterboxing - pixel-perfect"));

    scalingModeGroup_->addButton(sharpRadio_,
                                 static_cast<int>(VideoDisplayWidget::ScalingMode::Sharp));
    scalingModeGroup_->addButton(smoothRadio_,
                                 static_cast<int>(VideoDisplayWidget::ScalingMode::Smooth));
    scalingModeGroup_->addButton(integerRadio_,
                                 static_cast<int>(VideoDisplayWidget::ScalingMode::Integer));

    toolBar_->addWidget(sharpRadio_);
    toolBar_->addWidget(smoothRadio_);
    toolBar_->addWidget(integerRadio_);

    // Default to Integer (will be overridden by loadSettings)
    integerRadio_->setChecked(true);

    connect(scalingModeGroup_, QOverload<int>::of(&QButtonGroup::idClicked), this,
            &ViewPanel::onScalingModeChanged);

    layout->addWidget(toolBar_);

    // Create diagnostics widget (hidden by default)
    diagnosticsWidget_ = new StreamingDiagnosticsWidget();
    diagnosticsWidget_->setVisible(false);
    layout->addWidget(diagnosticsWidget_);

    // Create video display widget
    videoDisplayWidget_ = new VideoDisplayWidget();
    videoDisplayWidget_->setMinimumSize(384, 272);
    layout->addWidget(videoDisplayWidget_, 1);

    // Wire device connection state changes (independent of streaming services)
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnectionManager::stateChanged, this,
                &ViewPanel::onConnectionStateChanged);
    }
}

void ViewPanel::setStreamingService(StreamingService *manager)
{
    streamingService_ = manager;

    if (!streamingService_) {
        return;
    }

    // Connect streaming service to display
    if (streamingService_->videoReceiver() && videoDisplayWidget_) {
        connect(streamingService_->videoReceiver(), &VideoStreamReceiver::frameReady,
                videoDisplayWidget_, &VideoDisplayWidget::displayFrame);
    }

    connect(streamingService_, &StreamingService::streamingStarted, this,
            &ViewPanel::onStreamingStarted);
    connect(streamingService_, &StreamingService::streamingStopped, this,
            &ViewPanel::onStreamingStopped);
    connect(streamingService_, &StreamingService::videoFormatDetected, this,
            &ViewPanel::onVideoFormatDetected);
    connect(streamingService_, &StreamingService::error, this, &ViewPanel::onStreamingError);
    connect(streamingService_, &StreamingService::statusMessage, this,
            [this](const QString &msg, int /*timeout*/) {
                if (errorHandler_) {
                    errorHandler_->info(ErrorCategory::System, msg);
                }
            });

    // Connect video display keyboard events to keyboard service
    if (videoDisplayWidget_) {
        connect(videoDisplayWidget_, &VideoDisplayWidget::keyPressed, this,
                [this](QKeyEvent *event) {
                    if (streamingService_ && streamingService_->keyboardInput()) {
                        streamingService_->keyboardInput()->handleKeyPress(event);
                    }
                });
    }

    // Connect diagnostics service to widget
    if (streamingService_->diagnostics() && diagnosticsWidget_) {
        connect(streamingService_->diagnostics(), &StreamingDiagnosticsService::diagnosticsUpdated,
                this, &ViewPanel::onDiagnosticsUpdated);
    }
}

void ViewPanel::setRecordingService(VideoRecordingService *service)
{
    recordingService_ = service;

    if (!recordingService_) {
        return;
    }

    connect(recordingService_, &VideoRecordingService::recordingStarted, this,
            &ViewPanel::onRecordingStarted);
    connect(recordingService_, &VideoRecordingService::recordingStopped, this,
            &ViewPanel::onRecordingStopped);
    connect(recordingService_, &VideoRecordingService::error, this, &ViewPanel::onRecordingError);
}

void ViewPanel::setScreenshotService(ScreenshotService *service)
{
    screenshotService_ = service;
}

void ViewPanel::setErrorHandler(ErrorHandler *handler)
{
    errorHandler_ = handler;
}

void ViewPanel::setupConnections()
{
    // Intentionally empty — connections are now wired via setters or setupUi.
    // Kept to avoid breaking any future call sites expecting this method.
}

void ViewPanel::updateActions()
{
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();
    bool isStreaming = streamingService_ && streamingService_->isStreaming();
    if (startStreamAction_) {
        startStreamAction_->setEnabled(canOperate && !isStreaming);
    }
    if (stopStreamAction_) {
        stopStreamAction_->setEnabled(isStreaming);
    }
}

void ViewPanel::onConnectionStateChanged()
{
    updateActions();

    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();
    if (!canOperate && streamingService_ && streamingService_->isStreaming()) {
        streamingService_->stopStreaming();
    }
}

void ViewPanel::stopStreamingIfActive()
{
    if (streamingService_ && streamingService_->isStreaming()) {
        streamingService_->stopStreaming();
    }
}

void ViewPanel::loadSettings()
{
    QSettings settings;
    int scalingMode =
        settings
            .value("view/scalingMode", static_cast<int>(VideoDisplayWidget::ScalingMode::Integer))
            .toInt();
    auto mode = static_cast<VideoDisplayWidget::ScalingMode>(scalingMode);
    videoDisplayWidget_->setScalingMode(mode);

    // Update the radio buttons to match
    switch (mode) {
    case VideoDisplayWidget::ScalingMode::Sharp:
        sharpRadio_->setChecked(true);
        break;
    case VideoDisplayWidget::ScalingMode::Smooth:
        smoothRadio_->setChecked(true);
        break;
    case VideoDisplayWidget::ScalingMode::Integer:
        integerRadio_->setChecked(true);
        break;
    }
}

void ViewPanel::saveSettings()
{
    QSettings settings;
    settings.setValue("view/scalingMode", static_cast<int>(videoDisplayWidget_->scalingMode()));
}

int ViewPanel::scalingMode() const
{
    return static_cast<int>(videoDisplayWidget_->scalingMode());
}

void ViewPanel::onStartStreaming()
{
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                       tr("Not connected to device"));
        }
        return;
    }

    if (!streamingService_ || !streamingService_->startStreaming()) {
        // Error already emitted by StreamingService
        updateActions();
    }
}

void ViewPanel::onStopStreaming()
{
    if (streamingService_) {
        streamingService_->stopStreaming();
    }
}

void ViewPanel::onStreamingStarted(const QString &targetHost)
{
    startStreamAction_->setEnabled(false);
    stopStreamAction_->setEnabled(true);
    captureScreenshotAction_->setEnabled(true);
    startRecordingAction_->setEnabled(true);
    if (statsAction_) {
        statsAction_->setEnabled(true);
    }
    streamStatusLabel_->setText(tr("Starting stream to %1...").arg(targetHost));
}

void ViewPanel::onStreamingStopped()
{
    // Stop recording if active
    if (recordingService_ && recordingService_->isRecording()) {
        recordingService_->stopRecording();
    }

    // Clear display
    if (videoDisplayWidget_) {
        videoDisplayWidget_->clear();
    }

    if (startStreamAction_) {
        startStreamAction_->setEnabled(deviceConnection_ &&
                                       deviceConnection_->canPerformOperations());
    }
    if (stopStreamAction_) {
        stopStreamAction_->setEnabled(false);
    }
    if (captureScreenshotAction_) {
        captureScreenshotAction_->setEnabled(false);
    }
    if (startRecordingAction_) {
        startRecordingAction_->setEnabled(false);
    }
    if (stopRecordingAction_) {
        stopRecordingAction_->setEnabled(false);
    }
    if (statsAction_) {
        statsAction_->setEnabled(false);
        statsAction_->setChecked(false);
    }
    if (diagnosticsWidget_) {
        diagnosticsWidget_->setVisible(false);
        diagnosticsWidget_->clear();
    }
    if (streamStatusLabel_) {
        streamStatusLabel_->setText(tr("Not streaming"));
    }
}

void ViewPanel::onStreamingError(const QString & /*error*/)
{
    updateActions();
}

void ViewPanel::onVideoFormatDetected(int format)
{
    auto videoFormat = static_cast<VideoStreamReceiver::VideoFormat>(format);
    QString formatName;
    switch (videoFormat) {
    case VideoStreamReceiver::VideoFormat::PAL:
        formatName = "PAL";
        break;
    case VideoStreamReceiver::VideoFormat::NTSC:
        formatName = "NTSC";
        break;
    default:
        formatName = "Unknown";
        break;
    }

    streamStatusLabel_->setText(tr("Streaming (%1)").arg(formatName));
}

void ViewPanel::onScalingModeChanged(int id)
{
    auto mode = static_cast<VideoDisplayWidget::ScalingMode>(id);
    videoDisplayWidget_->setScalingMode(mode);

    // Save immediately so preference is persisted
    QSettings settings;
    settings.setValue("view/scalingMode", id);
}

void ViewPanel::onCaptureScreenshot()
{
    if (!videoDisplayWidget_) {
        qCDebug(LogUi) << "onCaptureScreenshot: videoDisplayWidget_ is null, skipping";
        return;
    }

    QImage frame = videoDisplayWidget_->currentFrame();
    if (frame.isNull()) {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                       tr("No frame to capture"));
        }
        return;
    }

    if (!screenshotService_) {
        qCDebug(LogUi) << "onCaptureScreenshot: screenshotService_ is null, skipping";
        return;
    }

    QString filename = screenshotService_->capture(frame);
    if (!filename.isEmpty()) {
        if (errorHandler_) {
            errorHandler_->info(ErrorCategory::FileOperation,
                                tr("Screenshot saved: %1").arg(filename));
        }
    } else {
        if (errorHandler_) {
            errorHandler_->handleError(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                                       tr("Failed to save screenshot"));
        }
    }
}

void ViewPanel::onStartRecording()
{
    if (!recordingService_) {
        qCDebug(LogUi) << "onStartRecording: recordingService_ is null, skipping";
        return;
    }

    recordingService_->startRecording(VideoRecordingService::prepareRecordingPath());
}

void ViewPanel::onStopRecording()
{
    if (recordingService_) {
        recordingService_->stopRecording();
    }
}

void ViewPanel::onRecordingStarted(const QString &filePath)
{
    Q_UNUSED(filePath)
    if (startRecordingAction_) {
        startRecordingAction_->setEnabled(false);
    }
    if (stopRecordingAction_) {
        stopRecordingAction_->setEnabled(true);
    }
    if (errorHandler_) {
        errorHandler_->info(ErrorCategory::FileOperation, tr("Recording started..."));
    }
}

void ViewPanel::onRecordingStopped(const QString &filePath, int frameCount)
{
    if (startRecordingAction_) {
        // Re-enable if still streaming
        startRecordingAction_->setEnabled(streamingService_ && streamingService_->isStreaming());
    }
    if (stopRecordingAction_) {
        stopRecordingAction_->setEnabled(false);
    }

    QFileInfo fileInfo(filePath);
    if (errorHandler_) {
        errorHandler_->info(
            ErrorCategory::FileOperation,
            tr("Recording saved: %1 (%2 frames)").arg(fileInfo.fileName()).arg(frameCount));
    }
}

void ViewPanel::onRecordingError(const QString & /*error*/)
{
    // Reset button states
    if (startRecordingAction_) {
        startRecordingAction_->setEnabled(streamingService_ && streamingService_->isStreaming());
    }
    if (stopRecordingAction_) {
        stopRecordingAction_->setEnabled(false);
    }
}

void ViewPanel::onStatsToggled(bool checked)
{
    if (diagnosticsWidget_) {
        diagnosticsWidget_->setVisible(checked);
    }
}

void ViewPanel::onDiagnosticsUpdated(const DiagnosticsSnapshot &snapshot)
{
    if (diagnosticsWidget_ && diagnosticsWidget_->isVisible()) {
        diagnosticsWidget_->updateDiagnostics(snapshot);
    }
}
