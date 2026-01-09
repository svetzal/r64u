/**
 * @file audiostreamreceiver.cpp
 * @brief Implementation of the UDP audio stream receiver.
 */

#include "audiostreamreceiver.h"
#include <QNetworkDatagram>
#include <QMutexLocker>
#include <QVariant>

AudioStreamReceiver::AudioStreamReceiver(QObject *parent)
    : QObject(parent)
    , socket_(new QUdpSocket(this))
{
    connect(socket_, &QUdpSocket::readyRead,
            this, &AudioStreamReceiver::onReadyRead);
    connect(socket_, &QUdpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) {
        emit socketError(socket_->errorString());
    });
}

AudioStreamReceiver::~AudioStreamReceiver()
{
    close();
}

bool AudioStreamReceiver::bind(quint16 port)
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        close();
    }

    // Set a large receive buffer
    socket_->setSocketOption(QAbstractSocket::ReceiveBufferSizeSocketOption,
                             QVariant(1024 * 1024));

    if (!socket_->bind(QHostAddress::Any, port)) {
        emit socketError(QString("Failed to bind to port %1: %2")
                         .arg(port)
                         .arg(socket_->errorString()));
        return false;
    }

    // Reset state
    {
        QMutexLocker locker(&bufferMutex_);
        jitterBuffer_.clear();
        bufferPrimed_ = false;
    }
    totalPacketsReceived_ = 0;
    totalPacketsLost_ = 0;
    firstPacket_ = true;

    return true;
}

void AudioStreamReceiver::close()
{
    if (socket_->state() != QAbstractSocket::UnconnectedState) {
        socket_->close();
    }

    QMutexLocker locker(&bufferMutex_);
    jitterBuffer_.clear();
    bufferPrimed_ = false;
}

bool AudioStreamReceiver::isActive() const
{
    return socket_->state() == QAbstractSocket::BoundState;
}

quint16 AudioStreamReceiver::port() const
{
    return socket_->localPort();
}

void AudioStreamReceiver::setJitterBufferSize(int packets)
{
    jitterBufferSize_ = qMax(1, packets);
}

int AudioStreamReceiver::bufferedPackets() const
{
    QMutexLocker locker(&bufferMutex_);
    return jitterBuffer_.size();
}

void AudioStreamReceiver::setAudioFormat(AudioFormat format)
{
    audioFormat_ = format;
}

double AudioStreamReceiver::sampleRate() const
{
    return (audioFormat_ == AudioFormat::NTSC) ? NtscSampleRate : PalSampleRate;
}

void AudioStreamReceiver::onReadyRead()
{
    while (socket_->hasPendingDatagrams()) {
        QNetworkDatagram datagram = socket_->receiveDatagram();
        if (datagram.isValid()) {
            QByteArray packet = datagram.data();
            if (packet.size() == PacketSize) {
                processPacket(packet);
            }
            // Ignore malformed packets
        }
    }
}

void AudioStreamReceiver::processPacket(const QByteArray &packet)
{
    totalPacketsReceived_++;

    // Parse header (2-byte sequence number, little-endian)
    const auto *data = reinterpret_cast<const quint8 *>(packet.constData());
    auto sequenceNumber = static_cast<quint16>(data[0] | (data[1] << 8));

    // Track sequence numbers for packet loss detection
    if (!firstPacket_) {
        quint16 expectedSeq = lastSequenceNumber_ + 1;
        if (sequenceNumber != expectedSeq) {
            // Check for valid wraparound (0xFFFF -> 0)
            bool isValidWraparound = (lastSequenceNumber_ == 0xFFFF && sequenceNumber == 0);
            if (!isValidWraparound) {
                quint16 gap = sequenceNumber - expectedSeq;
                if (gap < 1000) { // Reasonable gap
                    totalPacketsLost_ += gap;
                }
            }
        }
    }
    lastSequenceNumber_ = sequenceNumber;
    firstPacket_ = false;

    // Extract audio payload
    QByteArray samples = packet.mid(HeaderSize, PayloadSize);

    // Add to jitter buffer
    AudioPacket audioPacket;
    audioPacket.sequenceNumber = sequenceNumber;
    audioPacket.samples = samples;

    {
        QMutexLocker locker(&bufferMutex_);
        jitterBuffer_.enqueue(audioPacket);

        // Check if buffer is primed
        if (!bufferPrimed_ && jitterBuffer_.size() >= jitterBufferSize_ / 2) {
            bufferPrimed_ = true;
        }

        // If buffer is primed and has enough data, flush samples
        if (bufferPrimed_ && jitterBuffer_.size() >= 1) {
            flushBuffer();
        }

        // Prevent buffer overflow
        while (jitterBuffer_.size() > static_cast<qsizetype>(jitterBufferSize_) * 2) {
            jitterBuffer_.dequeue();
        }
    }

    // Emit stats periodically
    if (totalPacketsReceived_ % 250 == 0) {
        emit statsUpdated(totalPacketsReceived_, totalPacketsLost_, bufferedPackets());
    }
}

void AudioStreamReceiver::flushBuffer()
{
    // Called with mutex held
    if (jitterBuffer_.isEmpty()) {
        emit bufferUnderrun();
        return;
    }

    AudioPacket packet = jitterBuffer_.dequeue();
    emit samplesReady(packet.samples, SamplesPerPacket);
}
