/**
 * @file playlistmanager.cpp
 * @brief Implementation of the PlaylistManager service.
 */

#include "playlistmanager.h"

#include "deviceconnection.h"
#include "iftpclient.h"
#include "playlistcore.h"
#include "songlengthsdatabase.h"
#include "streamingmanager.h"

#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRandomGenerator>
#include <QSettings>

#include <algorithm>

PlaylistManager::PlaylistManager(DeviceConnection *connection, QObject *parent)
    : QObject(parent), deviceConnection_(connection), advanceTimer_(new QTimer(this))
{
    advanceTimer_->setSingleShot(true);
    connect(advanceTimer_, &QTimer::timeout, this, &PlaylistManager::onAdvanceTimer);

    // Connect to FTP client for SID data fetching (for duration lookup)
    if (deviceConnection_ != nullptr && deviceConnection_->ftpClient() != nullptr) {
        connect(deviceConnection_->ftpClient(), &IFtpClient::downloadToMemoryFinished, this,
                &PlaylistManager::onSidDataReceived);
    }

    loadSettings();
}

void PlaylistManager::setSonglengthsDatabase(SonglengthsDatabase *database)
{
    songlengthsDatabase_ = database;
}

void PlaylistManager::setStreamingManager(StreamingManager *manager)
{
    streamingManager_ = manager;
}

void PlaylistManager::addItem(const QString &path, int subsong)
{
    addItem(playlist::createItem(path, subsong, state_.defaultDuration));
}

void PlaylistManager::addItem(const PlaylistItem &item)
{
    state_ = playlist::addItem(state_, item);

    // Request SID data to look up duration from database
    if (songlengthsDatabase_ != nullptr && songlengthsDatabase_->isLoaded() &&
        deviceConnection_ != nullptr && deviceConnection_->ftpClient() != nullptr &&
        deviceConnection_->canPerformOperations()) {

        // Only request if we haven't already requested this path
        if (!pendingDurationLookups_.contains(item.path)) {
            pendingDurationLookups_.insert(item.path);
            deviceConnection_->ftpClient()->downloadToMemory(item.path);
        }
    }

    emit playlistChanged();
}

void PlaylistManager::removeItem(int index)
{
    auto [newState, wasPlaying] = playlist::removeItem(state_, index);
    bool wasStreaming = wasPlaying && playing_;
    state_ = newState;

    emit playlistChanged();

    if (wasStreaming && !state_.items.isEmpty()) {
        playCurrentItem();
    } else if (state_.items.isEmpty()) {
        stop();
    }
}

void PlaylistManager::moveItem(int from, int to)
{
    state_ = playlist::moveItem(state_, from, to);
    emit playlistChanged();
}

void PlaylistManager::clear()
{
    if (state_.items.isEmpty()) {
        return;
    }

    stop();
    state_ = playlist::clear(state_);

    emit playlistChanged();
}

PlaylistManager::PlaylistItem PlaylistManager::itemAt(int index) const
{
    if (index < 0 || index >= state_.items.count()) {
        return {};
    }
    return state_.items.at(index);
}

void PlaylistManager::play(int index)
{
    if (state_.items.isEmpty()) {
        return;
    }

    if (index < 0) {
        // Use current index or start from beginning
        if (state_.currentIndex < 0) {
            state_.currentIndex = state_.shuffle ? state_.shuffleOrder.first() : 0;
        }
    } else if (index < state_.items.count()) {
        state_.currentIndex = index;
    } else {
        return;
    }

    playing_ = true;

    // Start streaming if available and not already streaming
    if (streamingManager_ != nullptr && !streamingManager_->isStreaming()) {
        streamingManager_->startStreaming();
    }

    playCurrentItem();

    emit playbackStarted(state_.currentIndex);
    emit currentIndexChanged(state_.currentIndex);
}

void PlaylistManager::stop()
{
    stopTimer();
    playing_ = false;

    // Reset the C64 to stop the music
    if (deviceConnection_ != nullptr && deviceConnection_->canPerformOperations()) {
        deviceConnection_->restClient()->resetMachine();
    }

    // Stop streaming if active
    if (streamingManager_ != nullptr && streamingManager_->isStreaming()) {
        streamingManager_->stopStreaming();
    }

    emit playbackStopped();
}

void PlaylistManager::next()
{
    if (state_.items.isEmpty()) {
        return;
    }

    int nextIdx = playlist::nextIndex(state_);
    if (nextIdx < 0) {
        stop();
        return;
    }

    stopTimer();
    state_.currentIndex = nextIdx;
    emit currentIndexChanged(state_.currentIndex);

    if (playing_) {
        playCurrentItem();
    }
}

void PlaylistManager::previous()
{
    if (state_.items.isEmpty()) {
        return;
    }

    int prevIdx = playlist::previousIndex(state_);
    if (prevIdx < 0) {
        return;
    }

    stopTimer();
    state_.currentIndex = prevIdx;
    emit currentIndexChanged(state_.currentIndex);

    if (playing_) {
        playCurrentItem();
    }
}

void PlaylistManager::setShuffle(bool enabled)
{
    if (state_.shuffle == enabled) {
        return;
    }

    state_.shuffle = enabled;

    if (state_.shuffle) {
        state_.shuffleOrder = playlist::generateShuffleOrder(
            state_.items.size(), QRandomGenerator::global()->generate());
    }

    saveSettings();
    emit shuffleChanged(enabled);
}

void PlaylistManager::setRepeatMode(RepeatMode mode)
{
    if (state_.repeatMode == mode) {
        return;
    }

    state_.repeatMode = mode;
    saveSettings();
    emit repeatModeChanged(mode);
}

void PlaylistManager::setDefaultDuration(int seconds)
{
    seconds = std::clamp(seconds, 10, 3600);  // 10 seconds to 1 hour

    if (state_.defaultDuration == seconds) {
        return;
    }

    state_.defaultDuration = seconds;
    saveSettings();
    emit defaultDurationChanged(seconds);
}

void PlaylistManager::setItemDuration(int index, int seconds)
{
    if (index < 0 || index >= state_.items.count()) {
        return;
    }

    seconds = std::clamp(seconds, 10, 3600);

    state_.items[index].durationSecs = seconds;

    // If this is the current item and we're playing, restart timer
    if (index == state_.currentIndex && playing_) {
        startTimer();
    }

    emit playlistChanged();
}

void PlaylistManager::updateDurationFromData(const QString &path, const QByteArray &sidData)
{
    if (songlengthsDatabase_ == nullptr || !songlengthsDatabase_->isLoaded()) {
        return;
    }

    SonglengthsDatabase::SongLengths lengths = songlengthsDatabase_->lookupByData(sidData);
    if (!lengths.found || lengths.durations.isEmpty()) {
        return;
    }

    auto result = playlist::updateItemDurations(state_.items, path, lengths.durations);
    if (!result.anyUpdated) {
        return;
    }

    state_.items = result.items;

    // If the current item was updated and we're playing, restart the timer
    if (playing_ && result.updatedIndices.contains(state_.currentIndex)) {
        startTimer();
    }

    emit playlistChanged();
}

bool PlaylistManager::savePlaylist(const QString &filePath) const
{
    QJsonObject json = playlist::serialize(state_.items);
    QJsonDocument doc(json);
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly)) {
        return false;
    }

    file.write(doc.toJson());
    return true;
}

bool PlaylistManager::loadPlaylist(const QString &filePath)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        return false;
    }

    QJsonParseError error;
    QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &error);
    if (error.error != QJsonParseError::NoError) {
        return false;
    }

    if (!doc.isObject()) {
        return false;
    }

    // Stop current playback before loading
    stop();
    state_ = playlist::clear(state_);

    state_.items = playlist::deserialize(doc.object(), state_.defaultDuration);

    if (state_.shuffle && !state_.items.isEmpty()) {
        state_.shuffleOrder = playlist::generateShuffleOrder(
            state_.items.size(), QRandomGenerator::global()->generate());
    }

    emit playlistChanged();
    return true;
}

void PlaylistManager::saveSettings() const
{
    QSettings settings;
    settings.setValue("playlist/shuffle", state_.shuffle);
    settings.setValue("playlist/repeatMode", static_cast<int>(state_.repeatMode));
    settings.setValue("playlist/defaultDuration", state_.defaultDuration);
}

void PlaylistManager::loadSettings()
{
    QSettings settings;
    state_.shuffle = settings.value("playlist/shuffle", false).toBool();
    state_.repeatMode = static_cast<RepeatMode>(settings.value("playlist/repeatMode", 0).toInt());
    state_.defaultDuration = settings.value("playlist/defaultDuration", 180).toInt();
}

void PlaylistManager::onAdvanceTimer()
{
    if (!playing_ || state_.items.isEmpty()) {
        return;
    }

    int nextIdx = playlist::advanceIndex(state_);
    if (nextIdx < 0) {
        stop();
        return;
    }

    state_.currentIndex = nextIdx;
    playCurrentItem();
    emit currentIndexChanged(state_.currentIndex);
    emit trackAdvanced(state_.currentIndex);
}

void PlaylistManager::startTimer()
{
    if (state_.currentIndex < 0 || state_.currentIndex >= state_.items.count()) {
        return;
    }

    int durationMs = state_.items[state_.currentIndex].durationSecs * 1000;
    advanceTimer_->start(durationMs);
}

void PlaylistManager::stopTimer()
{
    advanceTimer_->stop();
}

void PlaylistManager::playCurrentItem()
{
    if (state_.currentIndex < 0 || state_.currentIndex >= state_.items.count()) {
        return;
    }

    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected to device"), 3000);
        return;
    }

    const PlaylistItem &item = state_.items[state_.currentIndex];

    // Play the SID via REST API
    // API uses 0-indexed subsongs, but we display 1-indexed
    int apiSubsong = item.subsong - 1;
    deviceConnection_->restClient()->playSid(item.path, apiSubsong);

    QString msg = item.title.isEmpty() ? QFileInfo(item.path).fileName() : item.title;
    if (item.totalSubsongs > 1) {
        msg += QString(" (%1/%2)").arg(item.subsong).arg(item.totalSubsongs);
    }
    emit statusMessage(tr("Playing: %1").arg(msg), 3000);

    // Start auto-advance timer
    startTimer();
}

void PlaylistManager::onSidDataReceived(const QString &remotePath, const QByteArray &data)
{
    // Check if this is a path we requested for duration lookup
    if (!pendingDurationLookups_.contains(remotePath)) {
        return;
    }

    pendingDurationLookups_.remove(remotePath);

    if (songlengthsDatabase_ == nullptr || !songlengthsDatabase_->isLoaded()) {
        return;
    }

    SonglengthsDatabase::SongLengths lengths = songlengthsDatabase_->lookupByData(data);
    if (!lengths.found || lengths.durations.isEmpty()) {
        return;
    }

    auto result = playlist::updateItemDurations(state_.items, remotePath, lengths.durations);
    if (!result.anyUpdated) {
        return;
    }

    state_.items = result.items;

    // If the current item was updated and we're playing, restart the timer
    if (playing_ && result.updatedIndices.contains(state_.currentIndex)) {
        startTimer();
    }

    emit playlistChanged();
}
