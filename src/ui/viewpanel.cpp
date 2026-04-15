#include "viewpanel.h"

#include "streamingdiagnosticswidget.h"
#include "videodisplaywidget.h"

#include "services/audiostreamreceiver.h"
#include "services/deviceconnection.h"
#include "services/keyboardinputservice.h"
#include "services/streamingdiagnostics.h"
#include "services/streamingmanager.h"
#include "services/videorecordingservice.h"
#include "services/videostreamreceiver.h"

#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QFileInfo>
#include <QKeyEvent>
#include <QMessageBox>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

ViewPanel::ViewPanel(DeviceConnection *connection, QWidget *parent)
    : QWidget(parent), deviceConnection_(connection)
{
    // DeviceConnection is required - assert in debug builds
    Q_ASSERT(deviceConnection_ && "DeviceConnection is required");

    setupUi();
    setupConnections();
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

    // Create streaming manager (owns all streaming services)
    streamingManager_ = StreamingManager::createDefault(deviceConnection_, this);

    // Create recording service
    recordingService_ = new VideoRecordingService(this);
    recordingService_->connectToStreaming(streamingManager_);
}

void ViewPanel::setupConnections()
{
    // Subscribe to device connection state changes
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnection::stateChanged, this,
                &ViewPanel::onConnectionStateChanged);
    }

    // Connect streaming manager to display
    if (streamingManager_ && streamingManager_->videoReceiver() && videoDisplayWidget_) {
        connect(streamingManager_->videoReceiver(), &VideoStreamReceiver::frameReady,
                videoDisplayWidget_, &VideoDisplayWidget::displayFrame);
    }

    // Connect streaming manager signals
    if (streamingManager_) {
        connect(streamingManager_, &StreamingManager::streamingStarted, this,
                &ViewPanel::onStreamingStarted);
        connect(streamingManager_, &StreamingManager::streamingStopped, this,
                &ViewPanel::onStreamingStopped);
        connect(streamingManager_, &StreamingManager::videoFormatDetected, this,
                &ViewPanel::onVideoFormatDetected);
        connect(streamingManager_, &StreamingManager::error, this, &ViewPanel::onStreamingError);
        connect(streamingManager_, &StreamingManager::statusMessage, this,
                &ViewPanel::statusMessage);
    }

    // Connect video display keyboard events to keyboard service
    if (videoDisplayWidget_) {
        connect(videoDisplayWidget_, &VideoDisplayWidget::keyPressed, this,
                [this](QKeyEvent *event) {
                    if (streamingManager_ && streamingManager_->keyboardInput()) {
                        streamingManager_->keyboardInput()->handleKeyPress(event);
                    }
                });
    }

    // Connect recording service signals
    if (recordingService_) {
        connect(recordingService_, &VideoRecordingService::recordingStarted, this,
                &ViewPanel::onRecordingStarted);
        connect(recordingService_, &VideoRecordingService::recordingStopped, this,
                &ViewPanel::onRecordingStopped);
        connect(recordingService_, &VideoRecordingService::error, this,
                &ViewPanel::onRecordingError);
    }

    // Connect diagnostics service to widget
    if (streamingManager_ && streamingManager_->diagnostics() && diagnosticsWidget_) {
        connect(streamingManager_->diagnostics(), &StreamingDiagnostics::diagnosticsUpdated, this,
                &ViewPanel::onDiagnosticsUpdated);
    }
}

void ViewPanel::updateActions()
{
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();
    bool isStreaming = streamingManager_ && streamingManager_->isStreaming();
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
    if (!canOperate && streamingManager_ && streamingManager_->isStreaming()) {
        streamingManager_->stopStreaming();
    }
}

void ViewPanel::stopStreamingIfActive()
{
    if (streamingManager_ && streamingManager_->isStreaming()) {
        streamingManager_->stopStreaming();
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
        QMessageBox::warning(this, tr("Not Connected"),
                             tr("Please connect to a C64 Ultimate device first."));
        return;
    }

    if (!streamingManager_ || !streamingManager_->startStreaming()) {
        // Error already emitted by StreamingManager
        updateActions();
    }
}

void ViewPanel::onStopStreaming()
{
    if (streamingManager_) {
        streamingManager_->stopStreaming();
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

void ViewPanel::onStreamingError(const QString &error)
{
    QMessageBox::warning(this, tr("Stream Error"), error);
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
        return;
    }

    QImage frame = videoDisplayWidget_->currentFrame();
    if (frame.isNull()) {
        emit statusMessage(tr("No frame to capture"), 3000);
        return;
    }

    // Get the capture directory from settings or use Pictures folder
    QSettings settings;
    QString captureDir =
        settings
            .value("capture/directory",
                   QStandardPaths::writableLocation(QStandardPaths::PicturesLocation))
            .toString();

    // Ensure the directory exists
    QDir dir(captureDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Generate a timestamp-based filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss_zzz");
    QString filename = QString("r64u_screenshot_%1.png").arg(timestamp);
    QString filePath = dir.filePath(filename);

    // Save the image
    if (frame.save(filePath, "PNG")) {
        emit statusMessage(tr("Screenshot saved: %1").arg(filename), 5000);
    } else {
        emit statusMessage(tr("Failed to save screenshot"), 5000);
    }
}

void ViewPanel::onStartRecording()
{
    if (!recordingService_) {
        return;
    }

    // Get the capture directory from settings or use Videos folder
    QSettings settings;
    QString captureDir =
        settings
            .value("capture/directory",
                   QStandardPaths::writableLocation(QStandardPaths::MoviesLocation))
            .toString();

    // Ensure the directory exists
    QDir dir(captureDir);
    if (!dir.exists()) {
        dir.mkpath(".");
    }

    // Generate a timestamp-based filename
    QString timestamp = QDateTime::currentDateTime().toString("yyyyMMdd_HHmmss");
    QString filename = QString("r64u_recording_%1.avi").arg(timestamp);
    QString filePath = dir.filePath(filename);

    recordingService_->startRecording(filePath);
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
    emit statusMessage(tr("Recording started..."), 3000);
}

void ViewPanel::onRecordingStopped(const QString &filePath, int frameCount)
{
    if (startRecordingAction_) {
        // Re-enable if still streaming
        startRecordingAction_->setEnabled(streamingManager_ && streamingManager_->isStreaming());
    }
    if (stopRecordingAction_) {
        stopRecordingAction_->setEnabled(false);
    }

    QFileInfo fileInfo(filePath);
    emit statusMessage(
        tr("Recording saved: %1 (%2 frames)").arg(fileInfo.fileName()).arg(frameCount), 5000);
}

void ViewPanel::onRecordingError(const QString &error)
{
    emit statusMessage(tr("Recording error: %1").arg(error), 5000);

    // Reset button states
    if (startRecordingAction_) {
        startRecordingAction_->setEnabled(streamingManager_ && streamingManager_->isStreaming());
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
