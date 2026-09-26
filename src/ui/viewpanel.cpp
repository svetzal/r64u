#include "viewpanel.h"

#include "pixelicons.h"
#include "streamingdiagnosticswidget.h"
#include "videodisplaywidget.h"

#include "core/c64screencore.h"
#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/keyboardinputservice.h"
#include "services/screenshotservice.h"
#include "services/streamingdiagnosticsservice.h"
#include "services/streamingservice.h"
#include "services/videorecordingservice.h"
#include "services/videostreamreceiverservice.h"
#include "utils/logging.h"

#include <QEvent>
#include <QFileInfo>
#include <QKeyEvent>
#include <QSettings>
#include <QVBoxLayout>

namespace {

constexpr int StatsOverlayInset = 8;
constexpr int PlaceholderColumns = 40;

}  // namespace

ViewPanel::ViewPanel(DeviceConnectionManager *connection, ErrorHandler *errorHandler,
                     QWidget *parent)
    : QWidget(parent), deviceConnection_(connection), errorHandler_(errorHandler)
{
    Q_ASSERT(deviceConnection_ && "DeviceConnectionManager is required");
    Q_ASSERT(errorHandler_ && "ErrorHandler is required");
    setupUi();
}

ViewPanel::~ViewPanel()
{
    // MainWindow parents the streaming and recording services to this panel, so
    // ~QWidget destroys them after the panel's child widgets. Stop listening
    // before that starts: a slot run from their teardown would act on a
    // half-destroyed panel.
    disconnectFromServices();
}

void ViewPanel::disconnectFromServices()
{
    if (streamingService_) {
        disconnect(streamingService_, nullptr, this, nullptr);
        if (streamingService_->diagnostics()) {
            disconnect(streamingService_->diagnostics(), nullptr, this, nullptr);
        }
    }
    if (recordingService_) {
        disconnect(recordingService_, nullptr, this, nullptr);
    }
}

void ViewPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    // Create toolbar
    toolBar_ = new QToolBar();
    toolBar_->setMovable(false);
    toolBar_->setIconSize(QSize(16, 16));
    toolBar_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);

    using pixeliconcore::Icon;
    startStreamAction_ =
        toolBar_->addAction(pixelicons::icon(Icon::StartStream), tr("Start Stream"));
    startStreamAction_->setToolTip(tr("Start video and audio streaming"));
    connect(startStreamAction_, &QAction::triggered, this, &ViewPanel::onStartStreaming);

    stopStreamAction_ = toolBar_->addAction(pixelicons::icon(Icon::StopStream), tr("Stop Stream"));
    stopStreamAction_->setToolTip(tr("Stop streaming"));
    stopStreamAction_->setEnabled(false);
    connect(stopStreamAction_, &QAction::triggered, this, &ViewPanel::onStopStreaming);

    toolBar_->addSeparator();

    captureScreenshotAction_ =
        toolBar_->addAction(pixelicons::icon(Icon::Screenshot), tr("Screenshot"));
    captureScreenshotAction_->setToolTip(tr("Capture screenshot (saves to Pictures folder)"));
    captureScreenshotAction_->setEnabled(false);
    connect(captureScreenshotAction_, &QAction::triggered, this, &ViewPanel::onCaptureScreenshot);

    startRecordingAction_ = toolBar_->addAction(pixelicons::icon(Icon::Record), tr("Record"));
    startRecordingAction_->setToolTip(tr("Start recording video"));
    startRecordingAction_->setEnabled(false);
    connect(startRecordingAction_, &QAction::triggered, this, &ViewPanel::onStartRecording);

    stopRecordingAction_ =
        toolBar_->addAction(pixelicons::icon(Icon::StopRecording), tr("Stop Recording"));
    stopRecordingAction_->setToolTip(tr("Stop recording video"));
    stopRecordingAction_->setEnabled(false);
    connect(stopRecordingAction_, &QAction::triggered, this, &ViewPanel::onStopRecording);

    statsAction_ = toolBar_->addAction(pixelicons::icon(Icon::Stats), tr("Stats"));
    statsAction_->setToolTip(tr("Toggle streaming statistics display"));
    statsAction_->setCheckable(true);
    statsAction_->setEnabled(false);
    connect(statsAction_, &QAction::toggled, this, &ViewPanel::onStatsToggled);

    statusSeparator_ = toolBar_->addSeparator();

    streamStatusLabel_ = new QLabel();
    toolBar_->addWidget(streamStatusLabel_);
    setStreamStatusText(tr("Not streaming"));

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

    // Create video display widget
    videoDisplayWidget_ = new VideoDisplayWidget();
    videoDisplayWidget_->setMinimumSize(384, 272);
    layout->addWidget(videoDisplayWidget_, 1);
    connect(videoDisplayWidget_, &VideoDisplayWidget::keyboardFocusChanged, this,
            &ViewPanel::onKeyboardFocusChanged);

    setupStatsOverlay();

    // Wire device connection state changes (independent of streaming services)
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnectionManager::stateChanged, this,
                &ViewPanel::onConnectionStateChanged);
    }

    updatePlaceholder();
}

void ViewPanel::setupStatsOverlay()
{
    // The stats HUD floats over the video's top-left corner so toggling it
    // never resizes the picture. It sizes itself to its contents and grows
    // downward when expanded.
    diagnosticsWidget_ = new StreamingDiagnosticsWidget(videoDisplayWidget_);
    diagnosticsWidget_->setObjectName(QStringLiteral("StatsOverlay"));
    diagnosticsWidget_->setAttribute(Qt::WA_StyledBackground, true);
    diagnosticsWidget_->setAutoFillBackground(true);
    QPalette overlayPalette = diagnosticsWidget_->palette();
    overlayPalette.setColor(QPalette::Window, QColor(0, 0, 0, 150));
    overlayPalette.setColor(QPalette::WindowText, Qt::white);
    diagnosticsWidget_->setPalette(overlayPalette);
    diagnosticsWidget_->setStyleSheet(
        QStringLiteral("#StatsOverlay { background-color: rgba(0, 0, 0, 150); "
                       "border-radius: 6px; }"
                       "#StatsOverlay QLabel { color: white; }"));
    if (QLayout *overlayLayout = diagnosticsWidget_->layout()) {
        overlayLayout->setSizeConstraint(QLayout::SetFixedSize);
    }
    diagnosticsWidget_->setVisible(false);
    connect(diagnosticsWidget_, &StreamingDiagnosticsWidget::displayModeChanged, this,
            &ViewPanel::onStatsExpandedChanged);

    videoDisplayWidget_->installEventFilter(this);
    positionStatsOverlay();
}

void ViewPanel::positionStatsOverlay()
{
    if (!diagnosticsWidget_) {
        return;
    }
    diagnosticsWidget_->move(StatsOverlayInset, StatsOverlayInset);
    diagnosticsWidget_->raise();
}

bool ViewPanel::eventFilter(QObject *watched, QEvent *event)
{
    if (watched == videoDisplayWidget_ && event->type() == QEvent::Resize) {
        positionStatsOverlay();
    }
    return QWidget::eventFilter(watched, event);
}

void ViewPanel::setChromeHidden(bool hidden)
{
    if (toolBar_) {
        toolBar_->setVisible(!hidden);
    }
}

void ViewPanel::setFullScreenAction(QAction *action)
{
    if (!action || !toolBar_) {
        return;
    }
    action->setIcon(pixelicons::icon(pixeliconcore::Icon::FullScreen));
    toolBar_->insertAction(statusSeparator_, action);
}

void ViewPanel::focusVideoDisplay()
{
    if (videoDisplayWidget_) {
        videoDisplayWidget_->setFocus(Qt::OtherFocusReason);
    }
}

void ViewPanel::updatePlaceholder()
{
    if (!videoDisplayWidget_) {
        return;
    }

    const bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();
    QStringList lines{QStringLiteral("    **** R64U VIDEO ****"), QString()};
    if (!canOperate) {
        lines << tr("NO DEVICE CONNECTED.") << tr("CONNECT TO THE ULTIMATE TO VIEW ITS SCREEN.");
    } else if (streamState_ == StreamState::Starting) {
        lines << tr("STARTING STREAM...");
    } else {
        lines << tr("NOT STREAMING.") << tr("PRESS START STREAM TO WATCH THE C64.");
    }
    lines << QString() << QStringLiteral("READY.");

    videoDisplayWidget_->setPlaceholderLines(
        c64screencore::wrapToColumns(lines.join(QLatin1Char('\n')), PlaceholderColumns));
}

void ViewPanel::setStreamStatusText(const QString &text)
{
    streamStatusText_ = text;
    refreshStreamStatusLabel();
}

void ViewPanel::refreshStreamStatusLabel()
{
    if (!streamStatusLabel_) {
        return;
    }
    QString text = streamStatusText_;
    if (keyboardFocused_) {
        text += tr(" \u00b7 keys to C64");
    }
    streamStatusLabel_->setText(text);
}

void ViewPanel::onKeyboardFocusChanged(bool focused)
{
    keyboardFocused_ = focused;
    refreshStreamStatusLabel();
}

void ViewPanel::setStreamingService(StreamingService *manager)
{
    streamingService_ = manager;

    if (!streamingService_) {
        return;
    }

    // Connect streaming service to display
    if (streamingService_->videoReceiver() && videoDisplayWidget_) {
        connect(streamingService_->videoReceiver(), &VideoStreamReceiverService::frameReady,
                videoDisplayWidget_, &VideoDisplayWidget::displayFrame);
    }

    connect(streamingService_, &StreamingService::streamingStarted, this,
            &ViewPanel::onStreamingStarted);
    connect(streamingService_, &StreamingService::streamingStopped, this,
            &ViewPanel::onStreamingStopped);
    connect(streamingService_, &StreamingService::videoFormatDetected, this,
            &ViewPanel::onVideoFormatDetected);
    connect(streamingService_, &StreamingService::statusMessage, this,
            [this](const QString &msg, int /*timeout*/) {
                errorHandler_->info(ErrorCategory::System, msg);
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
    updatePlaceholder();

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
    if (diagnosticsWidget_) {
        const bool expanded = settings.value("view/statsExpanded", false).toBool();
        diagnosticsWidget_->setDisplayMode(expanded
                                               ? StreamingDiagnosticsWidget::DisplayMode::Detailed
                                               : StreamingDiagnosticsWidget::DisplayMode::Compact);
    }

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
    if (diagnosticsWidget_) {
        settings.setValue("view/statsExpanded",
                          diagnosticsWidget_->displayMode() ==
                              StreamingDiagnosticsWidget::DisplayMode::Detailed);
    }
}

int ViewPanel::scalingMode() const
{
    return static_cast<int>(videoDisplayWidget_->scalingMode());
}

void ViewPanel::onStartStreaming()
{
    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        errorHandler_->handleError(ErrorCategory::Connection, ErrorSeverity::Warning,
                                   tr("Not connected to device"));
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
    setStreamStatusText(tr("Starting stream to %1...").arg(targetHost));
    streamState_ = StreamState::Starting;
    updatePlaceholder();
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
    setStreamStatusText(tr("Not streaming"));
    streamState_ = StreamState::Idle;
    updatePlaceholder();
}

void ViewPanel::onVideoFormatDetected(int format)
{
    auto videoFormat = static_cast<VideoStreamReceiverService::VideoFormat>(format);
    QString formatName;
    switch (videoFormat) {
    case VideoStreamReceiverService::VideoFormat::PAL:
        formatName = "PAL";
        break;
    case VideoStreamReceiverService::VideoFormat::NTSC:
        formatName = "NTSC";
        break;
    default:
        formatName = "Unknown";
        break;
    }

    setStreamStatusText(tr("Streaming (%1)").arg(formatName));
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
        errorHandler_->handleError(ErrorCategory::Validation, ErrorSeverity::Warning,
                                   tr("No frame to capture"));
        return;
    }

    if (!screenshotService_) {
        qCDebug(LogUi) << "onCaptureScreenshot: screenshotService_ is null, skipping";
        return;
    }

    QString filename = screenshotService_->capture(frame);
    if (!filename.isEmpty()) {
        errorHandler_->info(ErrorCategory::FileOperation, tr("Screenshot saved: %1").arg(filename));
    } else {
        errorHandler_->handleError(ErrorCategory::FileOperation, ErrorSeverity::Warning,
                                   tr("Failed to save screenshot"));
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
    errorHandler_->info(ErrorCategory::FileOperation, tr("Recording started..."));
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
    errorHandler_->info(
        ErrorCategory::FileOperation,
        tr("Recording saved: %1 (%2 frames)").arg(fileInfo.fileName()).arg(frameCount));
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
        positionStatsOverlay();
    }
}

void ViewPanel::onStatsExpandedChanged()
{
    QSettings settings;
    settings.setValue("view/statsExpanded", diagnosticsWidget_->displayMode() ==
                                                StreamingDiagnosticsWidget::DisplayMode::Detailed);
    positionStatsOverlay();
}

void ViewPanel::onDiagnosticsUpdated(const DiagnosticsSnapshot &snapshot)
{
    if (diagnosticsWidget_ && diagnosticsWidget_->isVisible()) {
        diagnosticsWidget_->updateDiagnostics(snapshot);
    }
}
