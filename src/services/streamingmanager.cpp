#include "streamingmanager.h"
#include "deviceconnection.h"
#include "c64urestclient.h"
#include "streamcontrolclient.h"
#include "videostreamreceiver.h"
#include "audiostreamreceiver.h"
#include "audioplaybackservice.h"
#include "keyboardinputservice.h"
#include "streamingdiagnostics.h"
#include "utils/logging.h"

#include <QUrl>
#include <QNetworkInterface>
#include <QHostAddress>

StreamingManager::StreamingManager(DeviceConnection *connection, QObject *parent)
    : QObject(parent)
    , deviceConnection_(connection)
{
    // DeviceConnection is required - assert in debug builds
    Q_ASSERT(deviceConnection_ && "DeviceConnection is required");

    // Create streaming services (owned by this manager)
    streamControl_ = new StreamControlClient(this);
    videoReceiver_ = new VideoStreamReceiver(this);
    audioReceiver_ = new AudioStreamReceiver(this);
    audioPlayback_ = new AudioPlaybackService(this);
    // Guard against null restClient
    C64URestClient *restClient = deviceConnection_ ? deviceConnection_->restClient() : nullptr;
    keyboardInput_ = new KeyboardInputService(restClient, this);
    diagnostics_ = new StreamingDiagnostics(this);

    // Attach diagnostics to receivers
    diagnostics_->attachVideoReceiver(videoReceiver_);
    diagnostics_->attachAudioReceiver(audioReceiver_);

    // Set up diagnostics callbacks for high-frequency timing data
    auto videoCallback = diagnostics_->videoCallback();
    videoReceiver_->setDiagnosticsCallback({
        videoCallback.onPacketReceived,
        videoCallback.onFrameStarted,
        videoCallback.onFrameCompleted,
        videoCallback.onOutOfOrderPacket
    });

    auto audioCallback = diagnostics_->audioCallback();
    audioReceiver_->setDiagnosticsCallback({
        audioCallback.onPacketReceived,
        audioCallback.onBufferUnderrun,
        audioCallback.onSampleDiscontinuity
    });

    // Connect video receiver format detection
    connect(videoReceiver_, &VideoStreamReceiver::formatDetected,
            this, [this](VideoStreamReceiver::VideoFormat format) {
        onVideoFormatDetected(static_cast<int>(format));
    });

    // Connect audio receiver to playback
    connect(audioReceiver_, &AudioStreamReceiver::samplesReady,
            audioPlayback_, &AudioPlaybackService::writeSamples);

    // Connect stream control signals
    connect(streamControl_, &StreamControlClient::commandSucceeded,
            this, &StreamingManager::onStreamCommandSucceeded);
    connect(streamControl_, &StreamControlClient::commandFailed,
            this, &StreamingManager::onStreamCommandFailed);
}

StreamingManager::~StreamingManager()
{
    if (isStreaming_) {
        stopStreaming();
    }
}

bool StreamingManager::startStreaming()
{
    if (isStreaming_) {
        LOG_VERBOSE() << "StreamingManager::startStreaming: Already streaming";
        return false;
    }

    if (!deviceConnection_ || !deviceConnection_->isConnected()) {
        emit error(tr("Not connected to device"));
        return false;
    }

    if (!deviceConnection_->restClient()) {
        emit error(tr("REST client not available"));
        return false;
    }

    // Clear any pending commands from previous sessions
    if (streamControl_) {
        streamControl_->clearPendingCommands();
    }

    // Extract device host from REST client URL
    QString deviceUrl = deviceConnection_->restClient()->host();
    LOG_VERBOSE() << "StreamingManager::startStreaming: deviceUrl from restClient:" << deviceUrl;
    QString deviceHost = QUrl(deviceUrl).host();
    if (deviceHost.isEmpty()) {
        // Maybe it's already just an IP address without scheme
        deviceHost = deviceUrl;
    }
    LOG_VERBOSE() << "StreamingManager::startStreaming: extracted deviceHost:" << deviceHost;
    streamControl_->setHost(deviceHost);

    // Find local IP that can reach the device
    QString targetHost = findLocalHostForDevice();
    if (targetHost.isEmpty()) {
        emit error(tr("Could not determine local IP address for streaming.\n\n"
                      "Device IP: %1\n"
                      "Make sure you're on the same network as the C64 device.")
                   .arg(deviceHost));
        return false;
    }

    LOG_VERBOSE() << "StreamingManager::startStreaming: Local IP for streaming:" << targetHost;

    // Start UDP receivers
    if (!videoReceiver_->bind()) {
        emit error(tr("Failed to bind video receiver port."));
        return false;
    }

    if (!audioReceiver_->bind()) {
        videoReceiver_->close();
        emit error(tr("Failed to bind audio receiver port."));
        return false;
    }

    // Start audio playback
    if (!audioPlayback_->start()) {
        videoReceiver_->close();
        audioReceiver_->close();
        emit error(tr("Failed to start audio playback."));
        return false;
    }

    // Send stream start commands to the device
    LOG_VERBOSE() << "StreamingManager::startStreaming: Sending stream commands to device"
             << deviceHost << "- target:" << targetHost
             << "video port:" << VideoStreamReceiver::DefaultPort
             << "audio port:" << AudioStreamReceiver::DefaultPort;
    streamControl_->startAllStreams(targetHost,
                                    VideoStreamReceiver::DefaultPort,
                                    AudioStreamReceiver::DefaultPort);

    isStreaming_ = true;
    currentTargetHost_ = targetHost;

    // Enable diagnostics collection
    if (diagnostics_) {
        diagnostics_->setEnabled(true);
    }

    emit streamingStarted(targetHost);

    return true;
}

void StreamingManager::stopStreaming()
{
    if (!isStreaming_) {
        return;
    }

    // Disable diagnostics collection
    if (diagnostics_) {
        diagnostics_->setEnabled(false);
    }

    // Send stop commands
    streamControl_->stopAllStreams();

    // Stop receivers and playback
    audioPlayback_->stop();
    videoReceiver_->close();
    audioReceiver_->close();

    isStreaming_ = false;
    currentTargetHost_.clear();
    emit streamingStopped();
}

void StreamingManager::onVideoFormatDetected(int format)
{
    auto videoFormat = static_cast<VideoStreamReceiver::VideoFormat>(format);
    switch (videoFormat) {
    case VideoStreamReceiver::VideoFormat::PAL:
        audioReceiver_->setAudioFormat(AudioStreamReceiver::AudioFormat::PAL);
        break;
    case VideoStreamReceiver::VideoFormat::NTSC:
        audioReceiver_->setAudioFormat(AudioStreamReceiver::AudioFormat::NTSC);
        break;
    default:
        break;
    }

    emit videoFormatDetected(format);
}

void StreamingManager::onStreamCommandSucceeded(const QString &command)
{
    emit statusMessage(tr("Stream: %1").arg(command), 2000);
}

void StreamingManager::onStreamCommandFailed(const QString &command, const QString &errorMsg)
{
    emit statusMessage(tr("Stream failed: %1 - %2").arg(command, errorMsg), 5000);

    // If we're trying to start and it failed, clean up
    if (isStreaming_ && command.contains("start")) {
        stopStreaming();
    }
}

QString StreamingManager::findLocalHostForDevice() const
{
    if (!deviceConnection_ || !deviceConnection_->restClient()) {
        return {};
    }

    QString deviceUrl = deviceConnection_->restClient()->host();
    QString deviceHost = QUrl(deviceUrl).host();
    if (deviceHost.isEmpty()) {
        deviceHost = deviceUrl;
    }

    // Parse the device IP address
    QHostAddress deviceAddr(deviceHost);
    if (deviceAddr.isNull() || deviceAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        LOG_VERBOSE() << "StreamingManager::findLocalHostForDevice: Invalid device IP - isNull:" << deviceAddr.isNull()
                 << "protocol:" << deviceAddr.protocol();
        return {};
    }
    LOG_VERBOSE() << "StreamingManager::findLocalHostForDevice: device IP address:" << deviceAddr.toString();

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
        LOG_VERBOSE() << "StreamingManager::findLocalHostForDevice: Could not find local IP on same subnet as device";
    }

    return targetHost;
}
