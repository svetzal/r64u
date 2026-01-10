/**
 * @file videostreamreceiver.cpp
 * @brief Implementation of the UDP video stream receiver.
 */

#include "videostreamreceiver.h"
#include "utils/logging.h"
#include <QNetworkDatagram>
#include <QVariant>

VideoStreamReceiver::VideoStreamReceiver(QObject *parent)
    : QObject(parent)
    , socket_(new QUdpSocket(this))
{
    connect(socket_, &QUdpSocket::readyRead,
            this, &VideoStreamReceiver::onReadyRead);
    connect(socket_, &QUdpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) {
        emit socketError(socket_->errorString());
    });

    // Pre-allocate frame buffer for maximum size (PAL)
    // Each line is 192 bytes (384 pixels at 4 bits each)
    frameBuffer_.resize(static_cast<qsizetype>(BytesPerLine) * MaxFrameHeight);
}

VideoStreamReceiver::~VideoStreamReceiver()
{
    close();
}

bool VideoStreamReceiver::bind(quint16 port)
{
    LOG_VERBOSE() << "VideoStreamReceiver: Binding to port" << port;
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        close();
    }

    // Set a large receive buffer to handle packet bursts
    socket_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption, QVariant(2 * 1024 * 1024));

    if (!socket_->bind(QHostAddress::Any, port)) {
        LOG_VERBOSE() << "VideoStreamReceiver: Failed to bind:" << socket_->errorString();
        emit socketError(QString("Failed to bind to port %1: %2")
                         .arg(port)
                         .arg(socket_->errorString()));
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
                         << "expected size:" << PacketSize;
            }
            packetLogCounter++;
            if (packet.size() == PacketSize) {
                processPacket(packet);
            } else {
                LOG_VERBOSE() << "VideoStreamReceiver: Ignoring malformed packet, size:" << packet.size();
            }
        }
    }
}

void VideoStreamReceiver::processPacket(const QByteArray &packet)
{
    totalPacketsReceived_++;

    PacketHeader header = parseHeader(packet);

    // Track sequence numbers for packet loss detection
    if (!firstPacket_) {
        quint16 expectedSeq = lastSequenceNumber_ + 1;
        if (header.sequenceNumber != expectedSeq) {
            // Check for valid wraparound (0xFFFF -> 0)
            bool isValidWraparound = (lastSequenceNumber_ == 0xFFFF && header.sequenceNumber == 0);
            if (!isValidWraparound) {
                quint16 gap = header.sequenceNumber - expectedSeq;
                if (gap < 1000) { // Reasonable gap (not wraparound)
                    totalPacketsLost_ += gap;
                }
            }
        }
    }
    lastSequenceNumber_ = header.sequenceNumber;
    firstPacket_ = false;

    // Check if this is a new frame
    if (!frameInProgress_ || header.frameNumber != currentFrameNum_) {
        // If we were working on a frame, it's now incomplete - discard it
        if (frameInProgress_) {
            // Frame was incomplete, could emit signal here if needed
        }
        startNewFrame(header.frameNumber);
    }

    // Copy payload data to frame buffer at the correct line position
    quint16 lineNumber = header.actualLineNumber();
    int bufferOffset = lineNumber * BytesPerLine;

    // Ensure we don't write beyond buffer bounds
    if (bufferOffset + PayloadSize <= frameBuffer_.size()) {
        memcpy(frameBuffer_.data() + bufferOffset,
               packet.constData() + HeaderSize,
               PayloadSize);
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
            expectedPackets_ = PalPacketsPerFrame;
        } else if (videoFormat_ == VideoFormat::NTSC) {
            expectedPackets_ = NtscPacketsPerFrame;
        }
    }

    // Check if frame is complete
    // A frame is complete when we've received the last packet and have all expected packets
    if (header.isLastPacket() && expectedPackets_ > 0 &&
        receivedPackets_.size() >= expectedPackets_) {
        completeFrame();
    }
}

VideoStreamReceiver::PacketHeader VideoStreamReceiver::parseHeader(const QByteArray &packet) const
{
    PacketHeader header{};
    const auto *data = reinterpret_cast<const quint8 *>(packet.constData());

    // All values are little-endian
    header.sequenceNumber = static_cast<quint16>(data[0] | (data[1] << 8));
    header.frameNumber = static_cast<quint16>(data[2] | (data[3] << 8));
    header.lineNumber = static_cast<quint16>(data[4] | (data[5] << 8));
    header.pixelsPerLine = static_cast<quint16>(data[6] | (data[7] << 8));
    header.linesPerPacket = data[8];
    header.bitsPerPixel = data[9];
    header.encodingType = static_cast<quint16>(data[10] | (data[11] << 8));

    return header;
}

void VideoStreamReceiver::startNewFrame(quint16 frameNumber)
{
    currentFrameNum_ = frameNumber;
    receivedPackets_.clear();
    expectedPackets_ = 0;
    frameInProgress_ = true;

    // Clear frame buffer (fill with black - color index 0)
    frameBuffer_.fill(0);
}

void VideoStreamReceiver::completeFrame()
{
    totalFramesCompleted_++;
    frameInProgress_ = false;

    // Determine actual frame height based on format
    int frameHeight = (videoFormat_ == VideoFormat::NTSC) ? NtscHeight : PalHeight;
    int frameSize = BytesPerLine * frameHeight;

    // Extract only the used portion of the frame buffer
    QByteArray frameData = frameBuffer_.left(frameSize);

    // Log first few frames and then periodically
    if (totalFramesCompleted_ <= 3 || totalFramesCompleted_ % 50 == 0) {
        LOG_VERBOSE() << "VideoStreamReceiver: Frame" << totalFramesCompleted_ << "complete"
                 << "format:" << (videoFormat_ == VideoFormat::PAL ? "PAL" :
                                  videoFormat_ == VideoFormat::NTSC ? "NTSC" : "Unknown")
                 << "height:" << frameHeight << "size:" << frameSize;
    }

    emit frameReady(frameData, currentFrameNum_, videoFormat_);

    // Emit stats periodically (every 50 frames = ~1 second)
    if (totalFramesCompleted_ % 50 == 0) {
        emit statsUpdated(totalPacketsReceived_, totalFramesCompleted_, totalPacketsLost_);
    }
}

VideoStreamReceiver::VideoFormat VideoStreamReceiver::detectFormat(const PacketHeader &header) const
{
    // The format can be detected from the last packet:
    // - PAL: final line_num + lines_per_packet = 272
    // - NTSC: final line_num + lines_per_packet = 240

    quint16 lastLine = header.actualLineNumber();
    int totalLines = lastLine + header.linesPerPacket;

    if (totalLines == PalHeight) {
        return VideoFormat::PAL;
    } else if (totalLines == NtscHeight) {
        return VideoFormat::NTSC;
    }

    return VideoFormat::Unknown;
}
