/**
 * @file streamingservice_stub.cpp
 * @brief Link-time stub for StreamingService.
 *
 * ServiceFactory tests pull in the real service layer but not the full
 * streaming dependency chain (Qt Multimedia, stream sockets, etc.).
 * PlaylistService includes streamingservice.h and calls isStreaming() /
 * startStreaming() / stopStreaming() only when the pointer is non-null;
 * since ServiceFactory never sets a StreamingService on PlaylistService,
 * these bodies are never reached — but the linker still requires the symbols.
 */

#include "services/ierroremitter.h"
#include "services/streamingservice.h"

StreamingService::StreamingService(DeviceConnectionManager * /*connection*/,
                                   IStreamControlService * /*streamControl*/,
                                   IVideoStreamReceiverService * /*videoReceiver*/,
                                   IAudioStreamReceiverService * /*audioReceiver*/,
                                   IAudioPlaybackService * /*audioPlayback*/,
                                   KeyboardInputService * /*keyboardInput*/,
                                   INetworkInterfaceProvider * /*networkProvider*/, QObject *parent)
    : IErrorEmitter(parent)
{
}

StreamingService::~StreamingService() = default;

bool StreamingService::startStreaming()
{
    return false;
}

void StreamingService::stopStreaming() {}

void StreamingService::onVideoFormatDetected(int /*format*/) {}

void StreamingService::onStreamCommandSucceeded(const QString & /*command*/) {}

void StreamingService::onStreamCommandFailed(const QString & /*command*/, const QString & /*error*/)
{
}
