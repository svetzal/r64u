/**
 * @file viewpanel_test_stubs.cpp
 * @brief Link-time stubs for ViewPanel unit tests.
 *
 * ViewPanel includes StreamingService and VideoRecordingService headers.
 * These services have heavy dependency chains (Qt Multimedia, network sockets,
 * etc.).  The stubs here satisfy linker symbol requirements without pulling in
 * those chains.  All method bodies are no-ops or return safe defaults.
 *
 * StreamingDiagnostics requires VideoStreamReceiver and AudioStreamReceiver
 * concrete types.  Since we include mock versions of those (from MOCK_STREAMING),
 * we only need to stub the StreamingDiagnostics attachment hooks.
 */

#include "services/streamingservice.h"
#include "services/videorecordingservice.h"

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
// VideoRecordingService stubs
// ---------------------------------------------------------------------------

VideoRecordingService::VideoRecordingService(QObject *parent) : QObject(parent) {}

VideoRecordingService::~VideoRecordingService() = default;

void VideoRecordingService::connectToStreaming(StreamingService * /*manager*/) {}

bool VideoRecordingService::startRecording(const QString & /*filePath*/)
{
    return false;
}

bool VideoRecordingService::stopRecording()
{
    return false;
}

void VideoRecordingService::addFrame(const QImage & /*frame*/) {}

void VideoRecordingService::addAudioSamples(const QByteArray & /*samples*/, int /*sampleCount*/) {}

void VideoRecordingService::onRawFrameReady(const QByteArray & /*frameData*/,
                                            quint16 /*frameNumber*/,
                                            IVideoStreamReceiver::VideoFormat /*format*/)
{
}

// ---------------------------------------------------------------------------
// Vic2 frame converter stub (used by VideoDisplayWidget)
// ---------------------------------------------------------------------------

#include "services/vic2frameconverter.h"

QImage Vic2::convertFrame(const QByteArray & /*frameData*/,
                          IVideoStreamReceiver::VideoFormat /*format*/)
{
    return QImage();
}

// ---------------------------------------------------------------------------
// StreamingDiagnostics stubs
// ---------------------------------------------------------------------------

#include "services/streamingdiagnostics.h"

StreamingDiagnostics::StreamingDiagnostics(QObject *parent) : QObject(parent) {}

StreamingDiagnostics::~StreamingDiagnostics() = default;

void StreamingDiagnostics::reset() {}

void StreamingDiagnostics::attachVideoReceiver(VideoStreamReceiver * /*receiver*/) {}

void StreamingDiagnostics::attachAudioReceiver(AudioStreamReceiver * /*receiver*/) {}

QString StreamingDiagnostics::qualityLevelString(QualityLevel /*level*/)
{
    return QString();
}

QColor StreamingDiagnostics::qualityLevelColor(QualityLevel /*level*/)
{
    return QColor();
}

void StreamingDiagnostics::onUpdateTimer() {}

void StreamingDiagnostics::onVideoStatsUpdated(quint64 /*bytesReceived*/,
                                               quint64 /*packetsReceived*/, quint64 /*packetsLost*/)
{
}

void StreamingDiagnostics::onAudioStatsUpdated(quint64 /*bytesReceived*/,
                                               quint64 /*samplesDecoded*/, int /*bufferLevel*/)
{
}

void StreamingDiagnostics::onAudioBufferUnderrun() {}
