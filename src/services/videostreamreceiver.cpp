/**
 * @file videostreamreceiver.cpp
 * @brief Implementation of the UDP video stream receiver.
 */

#include "videostreamreceiver.h"

#include "utils/logging.h"

#include <QNetworkDatagram>
#include <QVariant>

VideoStreamReceiver::VideoStreamReceiver(QObject *parent)
    : IVideoStreamReceiver(parent), socket_(new QUdpSocket(this))
{
    connect(socket_, &QUdpSocket::readyRead, this, &VideoStreamReceiver::onReadyRead);
    connect(socket_, &QUdpSocket::errorOccurred, this,
            [this](QAbstractSocket::SocketError) { emit socketError(socket_->errorString()); });

    // Pre-allocate frame buffer for maximum size (PAL)
    // Each line is 192 bytes (384 pixels at 4 bits each)
    frameBuffer_.resize(static_cast<qsizetype>(videostream::BytesPerLine) *
                        videostream::MaxFrameHeight);
}

VideoStreamReceiver::~VideoStreamReceiver()
{
    close();
}

bool VideoStreamReceiver::bind()
{
    return bind(DefaultPort);
}

bool VideoStreamReceiver::bind(quint16 port)
{
    LOG_VERBOSE() << "VideoStreamReceiver: Binding to port" << port;
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        close();
    }

    // Set a large receive buffer to handle packet bursts
    socket_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                             QVariant(2 * 1024 * 1024));

    if (!socket_->bind(QHostAddress::Any, port)) {
        LOG_VERBOSE() << "VideoStreamReceiver: Failed to bind:" << socket_->errorString();
        emit socketError(
            QString("Failed to bind to port %1: %2").arg(port).arg(socket_->errorString()));
        return false;
    }
    LOG_VERBOSE() << "VideoStreamReceiver: Bound successfully to port" << socket_->localPort();

    // Reset state
    frameInProgress_ = false;
    currentFrameNum_ = 0;
    receivedPackets_.clear();
    expectedPackets_ = 0;
    videoFormat_ = VideoFormat::Unknown;
    totalPacketsReceived_ = 0;
    totalFramesCompleted_ = 0;
    totalPacketsLost_ = 0;
    firstPacket_ = true;

    return true;
}

void VideoStreamReceiver::close()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->close();
    }
    frameInProgress_ = false;
}

bool VideoStreamReceiver::isActive() const
{
    return socket_->state() == QAbstractSocket::BoundState;
}

quint16 VideoStreamReceiver::port() const
{
    return socket_->localPort();
}

void VideoStreamReceiver::setDiagnosticsCallback(const DiagnosticsCallback &callback)
{
    diagnosticsCallback_ = callback;
    if (callback.onPacketReceived || callback.onFrameStarted || callback.onFrameCompleted) {
        diagnosticsTimer_.start();
    }
}

void VideoStreamReceiver::onReadyRead()
{
    static int packetLogCounter = 0;
    while (socket_->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket_->receiveDatagram();
        if (datagram.isValid()) {
            QByteArray packet = datagram.data();
            // Log first few packets and then periodically
            if (packetLogCounter < 5 || packetLogCounter % 1000 == 0) {
                LOG_VERBOSE() << "VideoStreamReceiver: Received packet size:" << packet.size()
                              << "from:" << datagram.senderAddress().toString()
                              << "expected size:" << videostream::PacketSize;
            }
            packetLogCounter++;
            if (packet.size() == videostream::PacketSize) {
                processPacket(packet);
            } else {
                LOG_VERBOSE() << "VideoStreamReceiver: Ignoring malformed packet, size:"
                              << packet.size();
            }
        }
    }
}

void VideoStreamReceiver::processPacket(const QByteArray &packet)
{
    totalPacketsReceived_++;

    // Call diagnostics callback for packet arrival timing
    if (diagnosticsCallback_.onPacketReceived && diagnosticsTimer_.isValid()) {
        diagnosticsCallback_.onPacketReceived(diagnosticsTimer_.nsecsElapsed() / 1000);
    }

    videostream::PacketHeader header = videostream::parseHeader(packet);

    // Track sequence numbers for packet loss and out-of-order detection
    if (!firstPacket_) {
        auto analysis = videostream::analyzeSequence(header.sequenceNumber, lastSequenceNumber_);
        if (analysis.isLoss) {
            totalPacketsLost_ += analysis.gap;
        } else if (analysis.isOutOfOrder) {
            if (diagnosticsCallback_.onOutOfOrderPacket) {
                diagnosticsCallback_.onOutOfOrderPacket();
            }
        }
    }
    lastSequenceNumber_ = header.sequenceNumber;
    firstPacket_ = false;

    // Check if this is a new frame
    if (!frameInProgress_ || header.frameNumber != currentFrameNum_) {
        // If we were working on a frame, it's now incomplete - discard it
        if (frameInProgress_) {
            // Notify diagnostics that frame was incomplete
            if (diagnosticsCallback_.onFrameCompleted && diagnosticsTimer_.isValid()) {
                qint64 endTimeUs = diagnosticsTimer_.nsecsElapsed() / 1000;
                diagnosticsCallback_.onFrameCompleted(currentFrameNum_, endTimeUs, false);
            }
        }
        startNewFrame(header.frameNumber);
    }

    // Copy payload data to frame buffer at the correct line position
    quint16 lineNumber = header.actualLineNumber();
    int bufferOffset = lineNumber * videostream::BytesPerLine;

    // Ensure we don't write beyond buffer bounds
    if (bufferOffset + videostream::PayloadSize <= frameBuffer_.size()) {
        memcpy(frameBuffer_.data() + bufferOffset, packet.constData() + videostream::HeaderSize,
               videostream::PayloadSize);
    }

    // Track which packets we've received (using line number as identifier)
    receivedPackets_.insert(lineNumber);

    // Detect format from last packet
    if (header.isLastPacket()) {
        VideoFormat detectedFormat = detectFormat(header);
        if (detectedFormat != VideoFormat::Unknown && detectedFormat != videoFormat_) {
            videoFormat_ = detectedFormat;
            emit formatDetected(videoFormat_);
        }

        // Calculate expected packet count based on format
        if (videoFormat_ == VideoFormat::PAL) {
            expectedPackets_ = videostream::PalPacketsPerFrame;
        } else if (videoFormat_ == VideoFormat::NTSC) {
            expectedPackets_ = videostream::NtscPacketsPerFrame;
        }
    }

    // Check if frame is complete
    // A frame is complete when we've received the last packet and have all expected packets
    if (header.isLastPacket() && expectedPackets_ > 0 &&
        receivedPackets_.size() >= expectedPackets_) {
        completeFrame();
    }
}

void VideoStreamReceiver::startNewFrame(quint16 frameNumber)
{
    currentFrameNum_ = frameNumber;
    receivedPackets_.clear();
    expectedPackets_ = 0;
    frameInProgress_ = true;

    // Record frame start time for diagnostics
    if (diagnosticsTimer_.isValid()) {
        frameStartTimeUs_ = diagnosticsTimer_.nsecsElapsed() / 1000;
        if (diagnosticsCallback_.onFrameStarted) {
            diagnosticsCallback_.onFrameStarted(frameNumber, frameStartTimeUs_);
        }
    }

    // Clear frame buffer (fill with black - color index 0)
    frameBuffer_.fill(0);
}

void VideoStreamReceiver::completeFrame()
{
    totalFramesCompleted_++;
    frameInProgress_ = false;

    // Call diagnostics callback for frame completion timing
    if (diagnosticsCallback_.onFrameCompleted && diagnosticsTimer_.isValid()) {
        qint64 endTimeUs = diagnosticsTimer_.nsecsElapsed() / 1000;
        diagnosticsCallback_.onFrameCompleted(currentFrameNum_, endTimeUs, true);
    }

    // Determine actual frame height based on format
    int frameHeight =
        (videoFormat_ == VideoFormat::NTSC) ? videostream::NtscHeight : videostream::PalHeight;
    int frameSize = videostream::BytesPerLine * frameHeight;

    // Extract only the used portion of the frame buffer
    QByteArray frameData = frameBuffer_.left(frameSize);

    // Log first few frames and then periodically
    if (totalFramesCompleted_ <= 3 || totalFramesCompleted_ % 50 == 0) {
        const char *formatStr = "Unknown";
        if (videoFormat_ == VideoFormat::PAL) {
            formatStr = "PAL";
        } else if (videoFormat_ == VideoFormat::NTSC) {
            formatStr = "NTSC";
        }
        LOG_VERBOSE() << "VideoStreamReceiver: Frame" << totalFramesCompleted_ << "complete"
                      << "format:" << formatStr << "height:" << frameHeight << "size:" << frameSize;
    }

    emit frameReady(frameData, currentFrameNum_, videoFormat_);

    // Emit stats periodically (every 50 frames = ~1 second)
    if (totalFramesCompleted_ % 50 == 0) {
        emit statsUpdated(totalPacketsReceived_, totalFramesCompleted_, totalPacketsLost_);
    }
}

IVideoStreamReceiver::VideoFormat
VideoStreamReceiver::detectFormat(const videostream::PacketHeader &header) const
{
    // The format can be detected from the last packet total line count:
    // - PAL:  actualLineNumber + linesPerPacket == 272
    // - NTSC: actualLineNumber + linesPerPacket == 240
    int totalLines = videostream::computeFrameLines(header);

    if (totalLines == videostream::PalHeight) {
        return VideoFormat::PAL;
    }
    if (totalLines == videostream::NtscHeight) {
        return VideoFormat::NTSC;
    }
    return VideoFormat::Unknown;
}
