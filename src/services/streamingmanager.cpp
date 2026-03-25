/**
 * @file streamingmanager.cpp
 * @brief Implementation of the StreamingManager service.
 */

#include "streamingmanager.h"

#include "audioplaybackservice.h"
#include "audiostreamreceiver.h"
#include "deviceconnection.h"
#include "iaudioplaybackservice.h"
#include "iaudiostreamreceiver.h"
#include "inetworkinterfaceprovider.h"
#include "irestclient.h"
#include "istreamcontrolclient.h"
#include "ivideostreamreceiver.h"
#include "keyboardinputservice.h"
#include "networkinterfaceprovider.h"
#include "streamcontrolclient.h"
#include "streamingdiagnostics.h"
#include "videostreamreceiver.h"

#include "utils/logging.h"

#include <QHostAddress>
#include <QUrl>

StreamingManager::StreamingManager(DeviceConnection *connection,
                                   IStreamControlClient *streamControl,
                                   IVideoStreamReceiver *videoReceiver,
                                   IAudioStreamReceiver *audioReceiver,
                                   IAudioPlaybackService *audioPlayback,
                                   KeyboardInputService *keyboardInput,
                                   INetworkInterfaceProvider *networkProvider, QObject *parent)
    : QObject(parent), deviceConnection_(connection), streamControl_(streamControl),
      videoReceiver_(videoReceiver), audioReceiver_(audioReceiver), audioPlayback_(audioPlayback),
      keyboardInput_(keyboardInput), networkProvider_(networkProvider),
      concreteVideoReceiver_(qobject_cast<VideoStreamReceiver *>(videoReceiver)),
      concreteAudioReceiver_(qobject_cast<AudioStreamReceiver *>(audioReceiver))
{
    Q_ASSERT(deviceConnection_ && "DeviceConnection is required");

    // Connect video receiver format detection
    connect(videoReceiver_, &IVideoStreamReceiver::formatDetected, this,
            [this](IVideoStreamReceiver::VideoFormat format) {
                onVideoFormatDetected(static_cast<int>(format));
            });

    // Connect audio receiver to playback
    connect(audioReceiver_, &IAudioStreamReceiver::samplesReady, audioPlayback_,
            &IAudioPlaybackService::writeSamples);

    // Connect stream control signals
    connect(streamControl_, &IStreamControlClient::commandSucceeded, this,
            &StreamingManager::onStreamCommandSucceeded);
    connect(streamControl_, &IStreamControlClient::commandFailed, this,
            &StreamingManager::onStreamCommandFailed);
}

StreamingManager *StreamingManager::createDefault(DeviceConnection *connection, QObject *parent)
{
    // Create owned streaming services (parented to manager)
    auto *streamControl = new StreamControlClient(nullptr);
    auto *videoReceiver = new VideoStreamReceiver(nullptr);
    auto *audioReceiver = new AudioStreamReceiver(nullptr);
    auto *audioPlayback = new AudioPlaybackService(nullptr);
    IRestClient *restClient = connection ? connection->restClient() : nullptr;
    auto *keyboardInput = new KeyboardInputService(restClient, nullptr);
    auto *networkProvider = new NetworkInterfaceProvider();

    auto *manager = new StreamingManager(connection, streamControl, videoReceiver, audioReceiver,
                                         audioPlayback, keyboardInput, networkProvider, parent);

    // Transfer ownership of all services to the manager
    streamControl->setParent(manager);
    videoReceiver->setParent(manager);
    audioReceiver->setParent(manager);
    audioPlayback->setParent(manager);
    keyboardInput->setParent(manager);

    // Attach diagnostics to receivers (diagnostics also owned by manager)
    auto *diagnostics = new StreamingDiagnostics(manager);
    manager->diagnostics_ = diagnostics;
    diagnostics->attachVideoReceiver(videoReceiver);
    diagnostics->attachAudioReceiver(audioReceiver);

    // Set up diagnostics callbacks
    auto videoCallback = diagnostics->videoCallback();
    videoReceiver->setDiagnosticsCallback(
        {videoCallback.onPacketReceived, videoCallback.onFrameStarted,
         videoCallback.onFrameCompleted, videoCallback.onOutOfOrderPacket});

    auto audioCallback = diagnostics->audioCallback();
    audioReceiver->setDiagnosticsCallback({audioCallback.onPacketReceived,
                                           audioCallback.onBufferUnderrun,
                                           audioCallback.onSampleDiscontinuity});

    return manager;
}

StreamingManager::~StreamingManager()
{
    if (isStreaming_) {
        stopStreaming();
    }
    delete networkProvider_;
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

    if (!streamControl_) {
        emit error(tr("Stream control client not available"));
        return false;
    }

    // Clear any pending commands from previous sessions
    streamControl_->clearPendingCommands();

    // Extract device host from REST client URL
    QString deviceUrl = deviceConnection_->restClient()->host();
    LOG_VERBOSE() << "StreamingManager::startStreaming: deviceUrl from restClient:" << deviceUrl;
    QString deviceHost = QUrl(deviceUrl).host();
    if (deviceHost.isEmpty()) {
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
    streamControl_->startAllStreams(targetHost, VideoStreamReceiver::DefaultPort,
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
    auto videoFormat = static_cast<IVideoStreamReceiver::VideoFormat>(format);
    switch (videoFormat) {
    case IVideoStreamReceiver::VideoFormat::PAL:
        audioReceiver_->setAudioFormat(IAudioStreamReceiver::AudioFormat::PAL);
        break;
    case IVideoStreamReceiver::VideoFormat::NTSC:
        audioReceiver_->setAudioFormat(IAudioStreamReceiver::AudioFormat::NTSC);
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

    QHostAddress deviceAddr(deviceHost);
    if (deviceAddr.isNull() || deviceAddr.protocol() != QAbstractSocket::IPv4Protocol) {
        LOG_VERBOSE() << "StreamingManager::findLocalHostForDevice: Invalid device IP - isNull:"
                      << deviceAddr.isNull() << "protocol:" << deviceAddr.protocol();
        return {};
    }
    LOG_VERBOSE() << "StreamingManager::findLocalHostForDevice: device IP address:"
                  << deviceAddr.toString();

    QString targetHost;

    for (const QNetworkInterface &iface : networkProvider_->allInterfaces()) {
        if (!(iface.flags() & QNetworkInterface::IsUp) ||
            !(iface.flags() & QNetworkInterface::IsRunning) ||
            (iface.flags() & QNetworkInterface::IsLoopBack)) {
            continue;
        }

        for (const QNetworkAddressEntry &entry : iface.addressEntries()) {
            if (entry.ip().protocol() != QAbstractSocket::IPv4Protocol) {
                continue;
            }

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
        LOG_VERBOSE() << "StreamingManager::findLocalHostForDevice: Could not find local IP on "
                         "same subnet as device";
    }

    return targetHost;
}
