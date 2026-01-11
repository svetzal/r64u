#include "viewpanel.h"
#include "videodisplaywidget.h"
#include "services/deviceconnection.h"
#include "services/streamingmanager.h"
#include "services/videostreamreceiver.h"
#include "services/keyboardinputservice.h"
#include "utils/logging.h"

#include <QVBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QKeyEvent>

ViewPanel::ViewPanel(DeviceConnection *connection, QWidget *parent)
    : QWidget(parent)
    , deviceConnection_(connection)
{
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

    scalingModeGroup_->addButton(sharpRadio_, static_cast<int>(VideoDisplayWidget::ScalingMode::Sharp));
    scalingModeGroup_->addButton(smoothRadio_, static_cast<int>(VideoDisplayWidget::ScalingMode::Smooth));
    scalingModeGroup_->addButton(integerRadio_, static_cast<int>(VideoDisplayWidget::ScalingMode::Integer));

    toolBar_->addWidget(sharpRadio_);
    toolBar_->addWidget(smoothRadio_);
    toolBar_->addWidget(integerRadio_);

    // Default to Integer (will be overridden by loadSettings)
    integerRadio_->setChecked(true);

    connect(scalingModeGroup_, QOverload<int>::of(&QButtonGroup::idClicked),
            this, &ViewPanel::onScalingModeChanged);

    layout->addWidget(toolBar_);

    // Create video display widget
    videoDisplayWidget_ = new VideoDisplayWidget();
    videoDisplayWidget_->setMinimumSize(384, 272);
    layout->addWidget(videoDisplayWidget_, 1);

    // Create streaming manager (owns all streaming services)
    streamingManager_ = new StreamingManager(deviceConnection_, this);
}

void ViewPanel::setupConnections()
{
    // Subscribe to device connection state changes
    connect(deviceConnection_, &DeviceConnection::stateChanged,
            this, &ViewPanel::onConnectionStateChanged);

    // Connect streaming manager to display
    connect(streamingManager_->videoReceiver(), &VideoStreamReceiver::frameReady,
            videoDisplayWidget_, &VideoDisplayWidget::displayFrame);

    // Connect streaming manager signals
    connect(streamingManager_, &StreamingManager::streamingStarted,
            this, &ViewPanel::onStreamingStarted);
    connect(streamingManager_, &StreamingManager::streamingStopped,
            this, &ViewPanel::onStreamingStopped);
    connect(streamingManager_, &StreamingManager::videoFormatDetected,
            this, &ViewPanel::onVideoFormatDetected);
    connect(streamingManager_, &StreamingManager::error,
            this, &ViewPanel::onStreamingError);
    connect(streamingManager_, &StreamingManager::statusMessage,
            this, &ViewPanel::statusMessage);

    // Connect video display keyboard events to keyboard service
    connect(videoDisplayWidget_, &VideoDisplayWidget::keyPressed,
            this, [this](QKeyEvent *event) {
        streamingManager_->keyboardInput()->handleKeyPress(event);
    });
}

void ViewPanel::updateActions()
{
    bool connected = deviceConnection_->isConnected();
    bool isStreaming = streamingManager_->isStreaming();
    startStreamAction_->setEnabled(connected && !isStreaming);
    stopStreamAction_->setEnabled(isStreaming);
}

void ViewPanel::onConnectionStateChanged()
{
    updateActions();

    bool connected = deviceConnection_->isConnected();
    if (!connected && streamingManager_->isStreaming()) {
        streamingManager_->stopStreaming();
    }
}

void ViewPanel::stopStreamingIfActive()
{
    if (streamingManager_->isStreaming()) {
        streamingManager_->stopStreaming();
    }
}

void ViewPanel::loadSettings()
{
    QSettings settings;
    int scalingMode = settings.value("view/scalingMode",
        static_cast<int>(VideoDisplayWidget::ScalingMode::Integer)).toInt();
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
    settings.setValue("view/scalingMode",
        static_cast<int>(videoDisplayWidget_->scalingMode()));
}

int ViewPanel::scalingMode() const
{
    return static_cast<int>(videoDisplayWidget_->scalingMode());
}

void ViewPanel::onStartStreaming()
{
    if (!deviceConnection_->isConnected()) {
        QMessageBox::warning(this, tr("Not Connected"),
                           tr("Please connect to a C64 Ultimate device first."));
        return;
    }

    if (!streamingManager_->startStreaming()) {
        // Error already emitted by StreamingManager
        updateActions();
    }
}

void ViewPanel::onStopStreaming()
{
    streamingManager_->stopStreaming();
}

void ViewPanel::onStreamingStarted(const QString &targetHost)
{
    startStreamAction_->setEnabled(false);
    stopStreamAction_->setEnabled(true);
    streamStatusLabel_->setText(tr("Starting stream to %1...").arg(targetHost));
}

void ViewPanel::onStreamingStopped()
{
    // Clear display
    videoDisplayWidget_->clear();

    startStreamAction_->setEnabled(deviceConnection_->isConnected());
    stopStreamAction_->setEnabled(false);
    streamStatusLabel_->setText(tr("Not streaming"));
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
