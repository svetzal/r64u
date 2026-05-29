/**
 * @file viewpanel_test_stubs.cpp
 * @brief Link-time stubs for ViewPanel unit tests.
 *
 * ViewPanel includes StreamingService and VideoRecordingService headers.
 * These services have heavy dependency chains (Qt Multimedia, network sockets,
 * etc.).  The stubs here satisfy linker symbol requirements without pulling in
 * those chains.  All method bodies are no-ops or return safe defaults.
 *
 * StreamingDiagnosticsServiceService requires VideoStreamReceiverService and
 * AudioStreamReceiverService concrete types.  Since we include mock versions of those (from
 * MOCK_STREAMING), we only need to stub the StreamingDiagnosticsServiceService attachment hooks.
 */

#include "services/streamingservice.h"
#include "services/videorecordingservice.h"

// ---------------------------------------------------------------------------
// StreamingService stubs
// ---------------------------------------------------------------------------

StreamingService::StreamingService(DeviceConnectionManager * /*connection*/,
                                   IStreamControlService * /*streamControl*/,
                                   IVideoStreamReceiverService * /*videoReceiver*/,
                                   IAudioStreamReceiverService * /*audioReceiver*/,
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
                                            IVideoStreamReceiverService::VideoFormat /*format*/)
{
}

QString VideoRecordingService::prepareRecordingPath()
{
    return QString();
}

// ---------------------------------------------------------------------------
// Vic2 frame converter stub (used by VideoDisplayWidget)
// ---------------------------------------------------------------------------

#include "services/vic2frameconverter.h"

QImage Vic2::convertFrame(const QByteArray & /*frameData*/,
                          IVideoStreamReceiverService::VideoFormat /*format*/)
{
    return QImage();
}

// ---------------------------------------------------------------------------
// StreamingDiagnosticsService stubs
// ---------------------------------------------------------------------------

#include "services/streamingdiagnosticsservice.h"

StreamingDiagnosticsService::StreamingDiagnosticsService(QObject *parent) : QObject(parent) {}

StreamingDiagnosticsService::~StreamingDiagnosticsService() = default;

void StreamingDiagnosticsService::reset() {}

void StreamingDiagnosticsService::attachVideoReceiver(VideoStreamReceiverService * /*receiver*/) {}

void StreamingDiagnosticsService::attachAudioReceiver(AudioStreamReceiverService * /*receiver*/) {}

QString StreamingDiagnosticsService::qualityLevelString(QualityLevel /*level*/)
{
    return QString();
}

QColor StreamingDiagnosticsService::qualityLevelColor(QualityLevel /*level*/)
{
    return QColor();
}

void StreamingDiagnosticsService::onUpdateTimer() {}

void StreamingDiagnosticsService::onVideoStatsUpdated(quint64 /*bytesReceived*/,
                                                      quint64 /*packetsReceived*/,
                                                      quint64 /*packetsLost*/)
{
}

void StreamingDiagnosticsService::onAudioStatsUpdated(quint64 /*bytesReceived*/,
                                                      quint64 /*samplesDecoded*/,
                                                      int /*bufferLevel*/)
{
}

void StreamingDiagnosticsService::onAudioBufferUnderrun() {}

// ---------------------------------------------------------------------------
// StreamingService::createDefault stub
// ---------------------------------------------------------------------------

#include "services/deviceconnectionmanager.h"

StreamingService *StreamingService::createDefault(DeviceConnectionManager *connection,
                                                  QObject *parent)
{
    return new StreamingService(connection, nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                parent);
}
