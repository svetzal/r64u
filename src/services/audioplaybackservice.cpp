/**
 * @file audioplaybackservice.cpp
 * @brief Implementation of the audio playback service.
 */

#include "audioplaybackservice.h"
#include <QAudioDevice>
#include <QMediaDevices>

AudioPlaybackService::AudioPlaybackService(QObject *parent)
    : QObject(parent)
{
    // Initialize audio format
    audioFormat_.setSampleRate(sampleRate_);
    audioFormat_.setChannelCount(Channels);
    audioFormat_.setSampleFormat(QAudioFormat::Int16);
}

AudioPlaybackService::~AudioPlaybackService()
{
    stop();
}

bool AudioPlaybackService::start()
{
    if (isPlaying_) {
        return true;
    }

    createAudioSink();

    if (!audioSink_) {
        emit errorOccurred("Failed to create audio output");
        return false;
    }

    // Start the audio output
    audioDevice_ = audioSink_->start();
    if (!audioDevice_) {
        emit errorOccurred(QString("Failed to start audio output: error code %1").arg(audioSink_->error()));
        audioSink_.reset();
        return false;
    }

    isPlaying_ = true;
    emit playbackStateChanged(true);
    return true;
}

void AudioPlaybackService::stop()
{
    if (!isPlaying_) {
        return;
    }

    if (audioSink_) {
        audioSink_->stop();
        audioSink_.reset();
    }
    audioDevice_ = nullptr;
    isPlaying_ = false;
    emit playbackStateChanged(false);
}

void AudioPlaybackService::setSampleRate(int rate)
{
    if (rate != sampleRate_) {
        bool wasPlaying = isPlaying_;
        if (wasPlaying) {
            stop();
        }

        sampleRate_ = rate;
        audioFormat_.setSampleRate(rate);

        if (wasPlaying) {
            start();
        }
    }
}

void AudioPlaybackService::setVolume(qreal volume)
{
    volume_ = qBound(0.0, volume, 1.0);
    if (audioSink_) {
        audioSink_->setVolume(volume_);
    }
}

qreal AudioPlaybackService::volume() const
{
    if (audioSink_) {
        return audioSink_->volume();
    }
    return volume_;
}

void AudioPlaybackService::setDiagnosticsCallback(const DiagnosticsCallback &callback)
{
    diagnosticsCallback_ = callback;
    if (callback.onSamplesWritten || callback.onPlaybackUnderrun) {
        diagnosticsTimer_.start();
    }
}

void AudioPlaybackService::writeSamples(const QByteArray &samples, int /*sampleCount*/)
{
    QMutexLocker locker(&writeMutex_);

    if (!isPlaying_ || !audioDevice_) {
        return;
    }

    // Write samples directly to audio device
    qint64 written = audioDevice_->write(samples);
    qint64 dropped = samples.size() - written;

    // Report to diagnostics
    if (diagnosticsCallback_.onSamplesWritten && diagnosticsTimer_.isValid()) {
        qint64 timeUs = diagnosticsTimer_.nsecsElapsed() / 1000;
        diagnosticsCallback_.onSamplesWritten(timeUs, written, dropped);
    }
}

void AudioPlaybackService::onStateChanged(QAudio::State state)
{
    switch (state) {
    case QAudio::IdleState:
        // No more data available - buffer underrun
        emit bufferUnderrun();
        if (diagnosticsCallback_.onPlaybackUnderrun) {
            diagnosticsCallback_.onPlaybackUnderrun();
        }
        break;
    case QAudio::StoppedState:
        if (audioSink_ && audioSink_->error() != QAudio::NoError) {
            emit errorOccurred(QString("Audio error: %1").arg(audioSink_->error()));
        }
        break;
    default:
        break;
    }
}

void AudioPlaybackService::createAudioSink()
{
    // Get default audio output device
    QAudioDevice outputDevice = QMediaDevices::defaultAudioOutput();

    if (outputDevice.isNull()) {
        emit errorOccurred("No audio output device available");
        return;
    }

    // Check if format is supported, otherwise use nearest format
    if (!outputDevice.isFormatSupported(audioFormat_)) {
        // Try to find a compatible format
        // Most devices support 48000 Hz stereo 16-bit
        QAudioFormat preferredFormat;
        preferredFormat.setSampleRate(48000);
        preferredFormat.setChannelCount(2);
        preferredFormat.setSampleFormat(QAudioFormat::Int16);

        if (outputDevice.isFormatSupported(preferredFormat)) {
            audioFormat_ = preferredFormat;
        }
        // If still not supported, QAudioSink will try its best
    }

    audioSink_ = std::make_unique<QAudioSink>(outputDevice, audioFormat_);
    audioSink_->setVolume(volume_);

    // Set reasonable buffer size (about 50ms of audio for low latency)
    int bufferSize = sampleRate_ * BytesPerFrame / 20; // 50ms
    audioSink_->setBufferSize(bufferSize);

    connect(audioSink_.get(), &QAudioSink::stateChanged,
            this, &AudioPlaybackService::onStateChanged);
}
