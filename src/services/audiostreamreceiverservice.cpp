/**
 * @file audiostreamreceiverservice.cpp
 * @brief Implementation of the UDP audio stream receiver.
 */

#include "audiostreamreceiverservice.h"

#include <QMutexLocker>

#include <algorithm>

AudioStreamReceiverService::AudioStreamReceiverService(QObject *parent)
    : IAudioStreamReceiverService(parent),
      streamSocket_(new UdpStreamSocket(1024 * 1024, PacketSize, this)),
      flushTimer_(new QTimer(this))
{
    connect(streamSocket_, &UdpStreamSocket::packetReceived, this,
            &AudioStreamReceiverService::processPacket);
    connect(streamSocket_, &UdpStreamSocket::socketError, this,
            &AudioStreamReceiverService::socketError);

    // Set up flush timer for steady playback timing
    flushTimer_->setTimerType(Qt::PreciseTimer);
    connect(flushTimer_, &QTimer::timeout, this, &AudioStreamReceiverService::onFlushTimer);
}

AudioStreamReceiverService::~AudioStreamReceiverService()
{
    close();
}

bool AudioStreamReceiverService::bind()
{
    return bind(DefaultPort);
}

bool AudioStreamReceiverService::bind(quint16 port)
{
    // Reset state before binding
    {
        QMutexLocker locker(&bufferMutex_);
        jitterBuffer_.clear();
        bufferPrimed_ = false;
    }
    totalPacketsReceived_ = 0;
    totalPacketsLost_ = 0;
    firstPacket_ = true;

    return streamSocket_->bind(port);
}

void AudioStreamReceiverService::close()
{
    stopFlushTimer();
    streamSocket_->close();

    QMutexLocker locker(&bufferMutex_);
    jitterBuffer_.clear();
    bufferPrimed_ = false;
}

bool AudioStreamReceiverService::isActive() const
{
    return streamSocket_->isActive();
}

quint16 AudioStreamReceiverService::port() const
{
    return streamSocket_->port();
}

void AudioStreamReceiverService::setJitterBufferSize(int packets)
{
    jitterBufferSize_ = qMax(1, packets);
}

int AudioStreamReceiverService::bufferedPackets() const
{
    QMutexLocker locker(&bufferMutex_);
    return jitterBuffer_.size();
}

void AudioStreamReceiverService::setAudioFormat(AudioFormat format)
{
    audioFormat_ = format;
}

double AudioStreamReceiverService::sampleRate() const
{
    return audiostream::sampleRateForFormat(audioFormat_ == AudioFormat::NTSC);
}

void AudioStreamReceiverService::setDiagnosticsCallback(const DiagnosticsCallback &callback)
{
    diagnosticsCallback_ = callback;
    if (callback.onPacketReceived || callback.onBufferUnderrun || callback.onSampleDiscontinuity) {
        diagnosticsTimer_.start();
    }
}

void AudioStreamReceiverService::processPacket(const QByteArray &packet)
{
    totalPacketsReceived_++;

    // Call diagnostics callback for packet arrival timing
    if (diagnosticsCallback_.onPacketReceived && diagnosticsTimer_.isValid()) {
        diagnosticsCallback_.onPacketReceived(diagnosticsTimer_.nsecsElapsed() / 1000);
    }

    // Parse header and payload using pure core function
    auto parsed = audiostream::parsePacket(packet);
    if (!parsed) {
        return;  // Malformed packet (wrong size)
    }
    quint16 sequenceNumber = parsed->sequenceNumber;

    // Track sequence numbers for packet loss and sample discontinuity detection
    if (!firstPacket_) {
        auto analysis = audiostream::analyzeSequence(sequenceNumber, lastSequenceNumber_);
        if (analysis.isDiscontinuity) {
            totalPacketsLost_ += analysis.gap;
            if (diagnosticsCallback_.onSampleDiscontinuity) {
                diagnosticsCallback_.onSampleDiscontinuity(static_cast<int>(analysis.gap));
            }
        }
    }
    lastSequenceNumber_ = sequenceNumber;
    firstPacket_ = false;

    // Extract audio payload
    QByteArray samples = parsed->samples;

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
            QMetaObject::invokeMethod(this, &AudioStreamReceiverService::startFlushTimer,
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

void AudioStreamReceiverService::flushBuffer()
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

void AudioStreamReceiverService::onFlushTimer()
{
    QMutexLocker locker(&bufferMutex_);

    if (!bufferPrimed_) {
        return;
    }

    flushBuffer();
}

void AudioStreamReceiverService::startFlushTimer()
{
    if (!flushTimer_->isActive()) {
        // Calculate interval: samples per packet / sample rate * 1000 for ms
        // 192 samples / 48000 Hz = 4ms per packet
        int intervalMs = std::max(1, calculateFlushIntervalUs() / 1000);
        flushTimer_->start(intervalMs);
    }
}

void AudioStreamReceiverService::stopFlushTimer()
{
    if (flushTimer_->isActive()) {
        flushTimer_->stop();
    }
}

int AudioStreamReceiverService::calculateFlushIntervalUs() const
{
    return audiostream::calculateFlushIntervalUs(sampleRate());
}
