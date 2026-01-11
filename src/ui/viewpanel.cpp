#include "viewpanel.h"
#include "videodisplaywidget.h"
#include "services/deviceconnection.h"
#include "services/streamcontrolclient.h"
#include "services/videostreamreceiver.h"
#include "services/audiostreamreceiver.h"
#include "services/audioplaybackservice.h"
#include "services/keyboardinputservice.h"
#include "utils/logging.h"

#include <QVBoxLayout>
#include <QMessageBox>
#include <QSettings>
#include <QUrl>
#include <QNetworkInterface>
#include <QKeyEvent>

ViewPanel::ViewPanel(DeviceConnection *connection, QWidget *parent)
    : QWidget(parent)
    , deviceConnection_(connection)
{
    setupUi();
    setupConnections();
}

ViewPanel::~ViewPanel()
{
    if (isStreaming_) {
        onStopStreaming();
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

    // Create streaming services (owned by this panel)
    streamControl_ = new StreamControlClient(this);
    videoReceiver_ = new VideoStreamReceiver(this);
    audioReceiver_ = new AudioStreamReceiver(this);
    audioPlayback_ = new AudioPlaybackService(this);

    // Create keyboard input service
    keyboardInput_ = new KeyboardInputService(deviceConnection_->restClient(), this);
}

void ViewPanel::setupConnections()
{
    // Connect video receiver to display
    connect(videoReceiver_, &VideoStreamReceiver::frameReady,
            videoDisplayWidget_, &VideoDisplayWidget::displayFrame);
    connect(videoReceiver_, &VideoStreamReceiver::formatDetected,
            this, [this](VideoStreamReceiver::VideoFormat format) {
        onVideoFormatDetected(static_cast<int>(format));
    });

    // Connect audio receiver to playback
    connect(audioReceiver_, &AudioStreamReceiver::samplesReady,
            audioPlayback_, &AudioPlaybackService::writeSamples);

    // Connect stream control signals
    connect(streamControl_, &StreamControlClient::commandSucceeded,
            this, &ViewPanel::onStreamCommandSucceeded);
    connect(streamControl_, &StreamControlClient::commandFailed,
            this, &ViewPanel::onStreamCommandFailed);

    // Connect video display keyboard events to keyboard service
    connect(videoDisplayWidget_, &VideoDisplayWidget::keyPressed,
            this, [this](QKeyEvent *event) {
        keyboardInput_->handleKeyPress(event);
    });
}

void ViewPanel::updateActions()
{
    bool connected = deviceConnection_->isConnected();
    startStreamAction_->setEnabled(connected && !isStreaming_);
    stopStreamAction_->setEnabled(isStreaming_);
}

void ViewPanel::onConnectionStateChanged(bool connected)
{
    updateActions();

    if (!connected && isStreaming_) {
        onStopStreaming();
    }
}

void ViewPanel::stopStreamingIfActive()
{
    if (isStreaming_) {
        onStopStreaming();
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

    // Clear any pending commands from previous sessions
    streamControl_->clearPendingCommands();

    // Extract just the host/IP from the REST client URL
    QString deviceUrl = deviceConnection_->restClient()->host();
    LOG_VERBOSE() << "ViewPanel::onStartStreaming: deviceUrl from restClient:" << deviceUrl;
    QString deviceHost = QUrl(deviceUrl).host();
    if (deviceHost.isEmpty()) {
        // Maybe it's already just an IP address without scheme
        deviceHost = deviceUrl;
    }
    LOG_VERBOSE() << "ViewPanel::onStartStreaming: extracted deviceHost:" << deviceHost;
    streamControl_->setHost(deviceHost);

    // Parse the device IP address
    QHostAddress deviceAddr(deviceHost);
    if (deviceAddr.isNull() || deviceAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        LOG_VERBOSE() << "ViewPanel::onStartStreaming: Invalid device IP - isNull:" << deviceAddr.isNull()
                 << "protocol:" << deviceAddr.protocol();
        QMessageBox::warning(this, tr("Network Error"),
                           tr("Invalid device IP address: %1").arg(deviceHost));
        return;
    }
    LOG_VERBOSE() << "ViewPanel::onStartStreaming: device IP address:" << deviceAddr.toString();

    // Find our local IP address that can reach the device
    // Look for an interface on the same subnet as the C64 device
    QString targetHost;

    for (const QNetworkInterface &iface : QNetworkInterface::allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }

            // Check if device is in same subnet
            QHostAddress subnet = entry.netmask();
            if ((entry.ip().toIPv4Address() & subnet.toIPv4Address()) ==
                (deviceAddr.toIPv4Address() & subnet.toIPv4Address())) {
                targetHost = entry.ip().toString();
                break;
            }
        }
        if (!targetHost.isEmpty()) {
            break;
        }
    }

    if (targetHost.isEmpty()) {
        LOG_VERBOSE() << "ViewPanel::onStartStreaming: Could not find local IP on same subnet as device";
        QMessageBox::warning(this, tr("Network Error"),
                           tr("Could not determine local IP address for streaming.\n\n"
                              "Device IP: %1\n"
                              "Make sure you're on the same network as the C64 device.")
                           .arg(deviceAddr.toString()));
        return;
    }

    LOG_VERBOSE() << "ViewPanel::onStartStreaming: Local IP for streaming:" << targetHost;

    // Start UDP receivers
    if (!videoReceiver_->bind()) {
        QMessageBox::warning(this, tr("Stream Error"),
                           tr("Failed to bind video receiver port."));
        return;
    }

    if (!audioReceiver_->bind()) {
        videoReceiver_->close();
        QMessageBox::warning(this, tr("Stream Error"),
                           tr("Failed to bind audio receiver port."));
        return;
    }

    // Start audio playback
    if (!audioPlayback_->start()) {
        videoReceiver_->close();
        audioReceiver_->close();
        QMessageBox::warning(this, tr("Stream Error"),
                           tr("Failed to start audio playback."));
        return;
    }

    // Send stream start commands to the device
    LOG_VERBOSE() << "ViewPanel::onStartStreaming: Sending stream commands to device"
             << deviceHost << "- target:" << targetHost
             << "video port:" << VideoStreamReceiver::DefaultPort
             << "audio port:" << AudioStreamReceiver::DefaultPort;
    streamControl_->startAllStreams(targetHost,
                                    VideoStreamReceiver::DefaultPort,
                                    AudioStreamReceiver::DefaultPort);

    isStreaming_ = true;
    startStreamAction_->setEnabled(false);
    stopStreamAction_->setEnabled(true);
    streamStatusLabel_->setText(tr("Starting stream to %1...").arg(targetHost));
}

void ViewPanel::onStopStreaming()
{
    if (!isStreaming_) {
        return;
    }

    // Send stop commands
    streamControl_->stopAllStreams();

    // Stop receivers and playback
    audioPlayback_->stop();
    videoReceiver_->close();
    audioReceiver_->close();

    // Clear display
    videoDisplayWidget_->clear();

    isStreaming_ = false;
    startStreamAction_->setEnabled(deviceConnection_->isConnected());
    stopStreamAction_->setEnabled(false);
    streamStatusLabel_->setText(tr("Not streaming"));
}

void ViewPanel::onVideoFormatDetected(int format)
{
    auto videoFormat = static_cast<VideoStreamReceiver::VideoFormat>(format);
    QString formatName;
    switch (videoFormat) {
    case VideoStreamReceiver::VideoFormat::PAL:
        formatName = "PAL";
        audioReceiver_->setAudioFormat(AudioStreamReceiver::AudioFormat::PAL);
        break;
    case VideoStreamReceiver::VideoFormat::NTSC:
        formatName = "NTSC";
        audioReceiver_->setAudioFormat(AudioStreamReceiver::AudioFormat::NTSC);
        break;
    default:
        formatName = "Unknown";
        break;
    }

    streamStatusLabel_->setText(tr("Streaming (%1)").arg(formatName));
}

void ViewPanel::onStreamCommandSucceeded(const QString &command)
{
    emit statusMessage(tr("Stream: %1").arg(command), 2000);
}

void ViewPanel::onStreamCommandFailed(const QString &command, const QString &error)
{
    emit statusMessage(tr("Stream failed: %1 - %2").arg(command, error), 5000);

    // If we're trying to start and it failed, clean up
    if (isStreaming_ && command.contains("start")) {
        onStopStreaming();
    }
}

void ViewPanel::onScalingModeChanged(int id)
{
    auto mode = static_cast<VideoDisplayWidget::ScalingMode>(id);
    videoDisplayWidget_->setScalingMode(mode);

    // Save immediately so preference is persisted
    QSettings settings;
    settings.setValue("view/scalingMode", id);
}
