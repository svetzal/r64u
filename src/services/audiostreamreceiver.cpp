/**
 * @file audiostreamreceiver.cpp
 * @brief Implementation of the UDP audio stream receiver.
 */

#include "audiostreamreceiver.h"
#include <QNetworkDatagram>
#include <QMutexLocker>
#include <QVariant>
#include <algorithm>

AudioStreamReceiver::AudioStreamReceiver(QObject *parent)
    : QObject(parent)
    , socket_(new QUdpSocket(this))
    , flushTimer_(new QTimer(this))
{
    connect(socket_, &QUdpSocket::readyRead,
            this, &AudioStreamReceiver::onReadyRead);
    connect(socket_, &QUdpSocket::errorOccurred,
            this, [this](QAbstractSocket::SocketError) {
        emit socketError(socket_->errorString());
    });

    // Set up flush timer for steady playback timing
    flushTimer_->setTimerType(Qt::PreciseTimer);
    connect(flushTimer_, &QTimer::timeout,
            this, &AudioStreamReceiver::onFlushTimer);
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
    stopFlushTimer();

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

void AudioStreamReceiver::setDiagnosticsCallback(const DiagnosticsCallback &callback)
{
    diagnosticsCallback_ = callback;
    if (callback.onPacketReceived || callback.onBufferUnderrun || callback.onSampleDiscontinuity) {
        diagnosticsTimer_.start();
    }
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

    // Call diagnostics callback for packet arrival timing
    if (diagnosticsCallback_.onPacketReceived && diagnosticsTimer_.isValid()) {
        diagnosticsCallback_.onPacketReceived(diagnosticsTimer_.nsecsElapsed() / 1000);
    }

    // Parse header (2-byte sequence number, little-endian)
    const auto *data = reinterpret_cast<const quint8 *>(packet.constData());
    auto sequenceNumber = static_cast<quint16>(data[0] | (data[1] << 8));

    // Track sequence numbers for packet loss and sample discontinuity detection
    if (!firstPacket_) {
        quint16 expectedSeq = lastSequenceNumber_ + 1;
        if (sequenceNumber != expectedSeq) {
            // Check for valid wraparound (0xFFFF -> 0)
            bool isValidWraparound = (lastSequenceNumber_ == 0xFFFF && sequenceNumber == 0);
            if (!isValidWraparound) {
                quint16 gap = sequenceNumber - expectedSeq;
                if (gap < 1000) { // Reasonable gap
                    totalPacketsLost_ += gap;
                    // Report sample discontinuity
                    if (diagnosticsCallback_.onSampleDiscontinuity) {
                        diagnosticsCallback_.onSampleDiscontinuity(static_cast<int>(gap));
                    }
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

        // Check if buffer is primed and start flush timer
        if (!bufferPrimed_ && jitterBuffer_.size() >= jitterBufferSize_ / 2) {
            bufferPrimed_ = true;
            // Start timer outside of mutex lock
            QMetaObject::invokeMethod(this, &AudioStreamReceiver::startFlushTimer,
                                      Qt::QueuedConnection);
        }

        // Prevent buffer overflow - drop oldest packets if too full
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
        if (diagnosticsCallback_.onBufferUnderrun) {
            diagnosticsCallback_.onBufferUnderrun();
        }
        return;
    }

    AudioPacket audioPacket = jitterBuffer_.dequeue();
    emit samplesReady(audioPacket.samples, SamplesPerPacket);
}

void AudioStreamReceiver::onFlushTimer()
{
    QMutexLocker locker(&bufferMutex_);

    if (!bufferPrimed_) {
        return;
    }

    flushBuffer();
}

void AudioStreamReceiver::startFlushTimer()
{
    if (!flushTimer_->isActive()) {
        // Calculate interval: samples per packet / sample rate * 1000 for ms
        // 192 samples / 48000 Hz = 4ms per packet
        int intervalMs = std::max(1, calculateFlushIntervalUs() / 1000);
        flushTimer_->start(intervalMs);
    }
}

void AudioStreamReceiver::stopFlushTimer()
{
    if (flushTimer_->isActive()) {
        flushTimer_->stop();
    }
}

int AudioStreamReceiver::calculateFlushIntervalUs() const
{
    // Calculate microseconds per packet based on sample rate
    // interval = samples_per_packet / sample_rate * 1,000,000
    double rate = sampleRate();
    return static_cast<int>((SamplesPerPacket / rate) * 1000000.0);
}
