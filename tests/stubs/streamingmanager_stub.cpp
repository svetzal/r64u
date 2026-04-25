/**
 * @file streamingmanager_stub.cpp
 * @brief Link-time stub for StreamingManager.
 *
 * ServiceFactory tests pull in the real service layer but not the full
 * streaming dependency chain (Qt Multimedia, stream sockets, etc.).
 * PlaylistManager includes streamingmanager.h and calls isStreaming() /
 * startStreaming() / stopStreaming() only when the pointer is non-null;
 * since ServiceFactory never sets a StreamingManager on PlaylistManager,
 * these bodies are never reached — but the linker still requires the symbols.
 */

#include "services/streamingmanager.h"

StreamingManager::StreamingManager(DeviceConnection * /*connection*/,
                                   IStreamControlClient * /*streamControl*/,
                                   IVideoStreamReceiver * /*videoReceiver*/,
                                   IAudioStreamReceiver * /*audioReceiver*/,
                                   IAudioPlaybackService * /*audioPlayback*/,
                                   KeyboardInputService * /*keyboardInput*/,
                                   INetworkInterfaceProvider * /*networkProvider*/, QObject *parent)
    : QObject(parent)
{
}

StreamingManager::~StreamingManager() = default;

bool StreamingManager::startStreaming()
{
    return false;
}

void StreamingManager::stopStreaming() {}

void StreamingManager::onVideoFormatDetected(int /*format*/) {}

void StreamingManager::onStreamCommandSucceeded(const QString & /*command*/) {}

void StreamingManager::onStreamCommandFailed(const QString & /*command*/, const QString & /*error*/)
{
}
