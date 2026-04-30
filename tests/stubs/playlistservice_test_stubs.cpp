/**
 * @file playlistservice_test_stubs.cpp
 * @brief Link-time stubs for PlaylistService unit tests.
 *
 * PlaylistService includes StreamingService and SonglengthsDatabase headers
 * and calls their methods only when those pointers are non-null.  In tests
 * we always pass nullptr for both, so the method bodies are never reached —
 * but the linker still demands their symbols from the TUs that include the
 * concrete headers.
 *
 * Stubs here satisfy those demands without pulling in the full dependency
 * trees of StreamingService (Qt Multimedia, network, etc.) or
 * SonglengthsDatabase (MD5, ZLIB, SQL, etc.).
 */

#include "services/songlengthsdatabase.h"
#include "services/streamingservice.h"

// ---------------------------------------------------------------------------
// SonglengthsDatabase stubs
// ---------------------------------------------------------------------------

SonglengthsDatabase::SonglengthsDatabase(IFileDownloader * /*downloader*/, QObject *parent)
    : QObject(parent), manager_(nullptr)
{
}

SonglengthsDatabase::SongLengths
SonglengthsDatabase::lookupByData(const QByteArray & /*sidData*/) const
{
    return {};
}

// ---------------------------------------------------------------------------
// StreamingService stubs
// ---------------------------------------------------------------------------

StreamingService::StreamingService(DeviceConnection * /*connection*/,
                                   IStreamControlClient * /*streamControl*/,
                                   IVideoStreamReceiver * /*videoReceiver*/,
                                   IAudioStreamReceiver * /*audioReceiver*/,
                                   IAudioPlaybackService * /*audioPlayback*/,
                                   KeyboardInputService * /*keyboardInput*/,
                                   INetworkInterfaceProvider * /*networkProvider*/, QObject *parent)
    : QObject(parent)
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

// ---------------------------------------------------------------------------
// SonglengthsDatabase public slot stubs
// ---------------------------------------------------------------------------

void SonglengthsDatabase::downloadDatabase() {}
