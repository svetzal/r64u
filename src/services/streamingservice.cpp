/**
 * @file streamingservice.cpp
 * @brief Implementation of the StreamingService service.
 */

#include "streamingservice.h"

#include "audioplaybackservice.h"
#include "audiostreamreceiverservice.h"
#include "deviceconnectionmanager.h"
#include "iaudioplaybackservice.h"
#include "iaudiostreamreceiverservice.h"
#include "inetworkinterfaceprovider.h"
#include "irestclient.h"
#include "istreamcontrolservice.h"
#include "ivideostreamreceiverservice.h"
#include "keyboardinputservice.h"
#include "networkinterfaceprovider.h"
#include "streamcontrolservice.h"
#include "streamingdiagnosticsservice.h"
#include "videostreamreceiverservice.h"

#include "utils/logging.h"

#include <QHostAddress>
#include <QUrl>

StreamingService::StreamingService(DeviceConnectionManager *connection,
                                   IStreamControlService *streamControl,
                                   IVideoStreamReceiverService *videoReceiver,
                                   IAudioStreamReceiverService *audioReceiver,
                                   IAudioPlaybackService *audioPlayback,
                                   KeyboardInputService *keyboardInput,
                                   INetworkInterfaceProvider *networkProvider, QObject *parent)
    : IErrorEmitter(parent), deviceConnection_(connection), streamControl_(streamControl),
      videoReceiver_(videoReceiver), audioReceiver_(audioReceiver), audioPlayback_(audioPlayback),
      keyboardInput_(keyboardInput), networkProvider_(networkProvider),
      concreteVideoReceiver_(qobject_cast<VideoStreamReceiverService *>(videoReceiver)),
      concreteAudioReceiver_(qobject_cast<AudioStreamReceiverService *>(audioReceiver))
{
    Q_ASSERT(deviceConnection_ && "DeviceConnectionManager is required");

    // Connect video receiver format detection
    connect(videoReceiver_, &IVideoStreamReceiverService::formatDetected, this,
            [this](IVideoStreamReceiverService::VideoFormat format) {
                onVideoFormatDetected(static_cast<int>(format));
            });

    // Connect audio receiver to playback
    connect(audioReceiver_, &IAudioStreamReceiverService::samplesReady, audioPlayback_,
            &IAudioPlaybackService::writeSamples);

    // Connect stream control signals
    connect(streamControl_, &IStreamControlService::commandSucceeded, this,
            &StreamingService::onStreamCommandSucceeded);
    connect(streamControl_, &IStreamControlService::commandFailed, this,
            &StreamingService::onStreamCommandFailed);
}

StreamingService *StreamingService::createDefault(DeviceConnectionManager *connection,
                                                  QObject *parent)
{
    // Create owned streaming services (parented to manager)
    auto *streamControl = new StreamControlService(nullptr);
    auto *videoReceiver = new VideoStreamReceiverService(nullptr);
    auto *audioReceiver = new AudioStreamReceiverService(nullptr);
    auto *audioPlayback = new AudioPlaybackService(nullptr);
    IRestClient *restClient = connection ? connection->restClient() : nullptr;
    auto *keyboardInput = new KeyboardInputService(restClient, nullptr);
    auto *networkProvider = new NetworkInterfaceProvider();

    auto *manager = new StreamingService(connection, streamControl, videoReceiver, audioReceiver,
                                         audioPlayback, keyboardInput, networkProvider, parent);

    // Transfer ownership of all services to the manager
    streamControl->setParent(manager);
    videoReceiver->setParent(manager);
    audioReceiver->setParent(manager);
    audioPlayback->setParent(manager);
    keyboardInput->setParent(manager);

    // Attach diagnostics to receivers (diagnostics also owned by manager)
    auto *diagnostics = new StreamingDiagnosticsService(manager);
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

StreamingService::~StreamingService()
{
    if (isStreaming_) {
        stopStreaming();
    }
    delete networkProvider_;
}

bool StreamingService::startStreaming()
{
    if (isStreaming_) {
        const QString msg = tr("Already streaming");
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    if (!deviceConnection_ || !deviceConnection_->isConnected()) {
        const QString msg = tr("Not connected to device");
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    if (!deviceConnection_->restClient()) {
        const QString msg = tr("REST client not available");
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    if (!streamControl_) {
        const QString msg = tr("Stream control client not available");
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    // Clear any pending commands from previous sessions
    streamControl_->clearPendingCommands();

    // Extract device host from REST client URL
    QString deviceUrl = deviceConnection_->restClient()->host();
    LOG_VERBOSE() << "StreamingService::startStreaming: deviceUrl from restClient:" << deviceUrl;
    QString deviceHost = QUrl(deviceUrl).host();
    if (deviceHost.isEmpty()) {
        deviceHost = deviceUrl;
    }
    LOG_VERBOSE() << "StreamingService::startStreaming: extracted deviceHost:" << deviceHost;
    streamControl_->setHost(deviceHost);

    // Find local IP that can reach the device
    QString targetHost = findLocalHostForDevice();
    if (targetHost.isEmpty()) {
        const QString msg = tr("Could not determine local IP address for streaming.\n\n"
                               "Device IP: %1\n"
                               "Make sure you're on the same network as the C64 device.")
                                .arg(deviceHost);
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    LOG_VERBOSE() << "StreamingService::startStreaming: Local IP for streaming:" << targetHost;

    // Start UDP receivers
    if (!videoReceiver_->bind()) {
        const QString msg = tr("Failed to bind video receiver port.");
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    if (!audioReceiver_->bind()) {
        videoReceiver_->close();
        const QString msg = tr("Failed to bind audio receiver port.");
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    // Start audio playback
    if (!audioPlayback_->start()) {
        videoReceiver_->close();
        audioReceiver_->close();
        const QString msg = tr("Failed to start audio playback.");
        emit error(msg);
        emit errorReported(ErrorCategory::System, ErrorSeverity::Warning, tr("Streaming Error"),
                           msg);
        return false;
    }

    // Send stream start commands to the device
    LOG_VERBOSE() << "StreamingService::startStreaming: Sending stream commands to device"
                  << deviceHost << "- target:" << targetHost
                  << "video port:" << VideoStreamReceiverService::DefaultPort
                  << "audio port:" << AudioStreamReceiverService::DefaultPort;
    streamControl_->startAllStreams(targetHost, VideoStreamReceiverService::DefaultPort,
                                    AudioStreamReceiverService::DefaultPort);

    isStreaming_ = true;
    currentTargetHost_ = targetHost;

    // Enable diagnostics collection
    if (diagnostics_) {
        diagnostics_->setEnabled(true);
    }

    emit streamingStarted(targetHost);

    return true;
}

void StreamingService::stopStreaming()
{
    if (!isStreaming_) {
        qCWarning(LogStreaming) << "stopStreaming called but not currently streaming";
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

void StreamingService::onVideoFormatDetected(int format)
{
    auto videoFormat = static_cast<IVideoStreamReceiverService::VideoFormat>(format);
    switch (videoFormat) {
    case IVideoStreamReceiverService::VideoFormat::PAL:
        audioReceiver_->setAudioFormat(IAudioStreamReceiverService::AudioFormat::PAL);
        break;
    case IVideoStreamReceiverService::VideoFormat::NTSC:
        audioReceiver_->setAudioFormat(IAudioStreamReceiverService::AudioFormat::NTSC);
        break;
    default:
        break;
    }

    emit videoFormatDetected(format);
}

void StreamingService::onStreamCommandSucceeded(const QString &command)
{
    emit statusMessage(tr("Stream: %1").arg(command), 2000);
}

void StreamingService::onStreamCommandFailed(const QString &command, const QString &errorMsg)
{
    emit statusMessage(tr("Stream failed: %1 - %2").arg(command, errorMsg), 5000);

    // If we're trying to start and it failed, clean up
    if (isStreaming_ && command.contains("start")) {
        stopStreaming();
    }
}

QString StreamingService::findLocalHostForDevice() const
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
        LOG_VERBOSE() << "StreamingService::findLocalHostForDevice: Invalid device IP - isNull:"
                      << deviceAddr.isNull() << "protocol:" << deviceAddr.protocol();
        return {};
    }
    LOG_VERBOSE() << "StreamingService::findLocalHostForDevice: device IP address:"
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
        LOG_VERBOSE() << "StreamingService::findLocalHostForDevice: Could not find local IP on "
                         "same subnet as device";
    }

    return targetHost;
}
