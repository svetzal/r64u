/**
 * @file playlistmanager_test_stubs.cpp
 * @brief Link-time stubs for PlaylistManager unit tests.
 *
 * PlaylistManager includes StreamingManager and SonglengthsDatabase headers
 * and calls their methods only when those pointers are non-null.  In tests
 * we always pass nullptr for both, so the method bodies are never reached —
 * but the linker still demands their symbols from the TUs that include the
 * concrete headers.
 *
 * Stubs here satisfy those demands without pulling in the full dependency
 * trees of StreamingManager (Qt Multimedia, network, etc.) or
 * SonglengthsDatabase (MD5, ZLIB, SQL, etc.).
 */

#include "services/songlengthsdatabase.h"
#include "services/streamingmanager.h"

// ---------------------------------------------------------------------------
// SonglengthsDatabase stubs
// ---------------------------------------------------------------------------

SonglengthsDatabase::SonglengthsDatabase(IFileDownloader * /*downloader*/, QObject *parent)
    : QObject(parent)
{
}

SonglengthsDatabase::SongLengths
SonglengthsDatabase::lookupByData(const QByteArray & /*sidData*/) const
{
    return {};
}

// ---------------------------------------------------------------------------
// StreamingManager stubs
// ---------------------------------------------------------------------------

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

// ---------------------------------------------------------------------------
// SonglengthsDatabase private slots stubs
// ---------------------------------------------------------------------------

void SonglengthsDatabase::downloadDatabase() {}

void SonglengthsDatabase::onDownloaderProgress(qint64 /*bytesReceived*/, qint64 /*bytesTotal*/) {}

void SonglengthsDatabase::onDownloaderFinished(const QByteArray & /*data*/) {}

void SonglengthsDatabase::onDownloaderFailed(const QString & /*error*/) {}
