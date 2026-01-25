/**
 * @file playlistmanager.cpp
 * @brief Implementation of the PlaylistManager service.
 */

#include "playlistmanager.h"
#include "deviceconnection.h"
#include "c64urestclient.h"

#include <QSettings>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QFileInfo>
#include <QRandomGenerator>
#include <algorithm>

PlaylistManager::PlaylistManager(DeviceConnection *connection, QObject *parent)
    : QObject(parent)
    , deviceConnection_(connection)
    , advanceTimer_(new QTimer(this))
{
    advanceTimer_->setSingleShot(true);
    connect(advanceTimer_, &QTimer::timeout, this, &PlaylistManager::onAdvanceTimer);

    loadSettings();
}

void PlaylistManager::addItem(const QString &path, int subsong)
{
    PlaylistItem item;
    item.path = path;
    item.subsong = subsong;
    item.durationSecs = defaultDuration_;

    // Extract filename as default title
    QFileInfo fileInfo(path);
    item.title = fileInfo.completeBaseName();

    addItem(item);
}

void PlaylistManager::addItem(const PlaylistItem &item)
{
    items_.append(item);

    // Regenerate shuffle order if shuffle is enabled
    if (shuffle_) {
        generateShuffleOrder();
    }

    emit playlistChanged();
}

void PlaylistManager::removeItem(int index)
{
    if (index < 0 || index >= items_.count()) {
        return;
    }

    // Adjust current index if needed
    bool wasPlaying = (index == currentIndex_);
    if (wasPlaying) {
        stopTimer();
    }

    items_.removeAt(index);

    // Regenerate shuffle order
    if (shuffle_) {
        generateShuffleOrder();
    }

    // Adjust current index
    if (currentIndex_ >= items_.count()) {
        currentIndex_ = items_.isEmpty() ? -1 : items_.count() - 1;
    } else if (index < currentIndex_) {
        currentIndex_--;
    }

    emit playlistChanged();

    if (wasPlaying && !items_.isEmpty() && playing_) {
        // Continue playing next item
        playCurrentItem();
    } else if (items_.isEmpty()) {
        stop();
    }
}

void PlaylistManager::moveItem(int from, int to)
{
    if (from < 0 || from >= items_.count() ||
        to < 0 || to >= items_.count() ||
        from == to) {
        return;
    }

    items_.move(from, to);

    // Update current index if affected
    if (currentIndex_ == from) {
        currentIndex_ = to;
    } else if (from < currentIndex_ && to >= currentIndex_) {
        currentIndex_--;
    } else if (from > currentIndex_ && to <= currentIndex_) {
        currentIndex_++;
    }

    // Regenerate shuffle order
    if (shuffle_) {
        generateShuffleOrder();
    }

    emit playlistChanged();
}

void PlaylistManager::clear()
{
    if (items_.isEmpty()) {
        return;
    }

    stop();
    items_.clear();
    shuffleOrder_.clear();
    currentIndex_ = -1;

    emit playlistChanged();
}

PlaylistManager::PlaylistItem PlaylistManager::itemAt(int index) const
{
    if (index < 0 || index >= items_.count()) {
        return {};
    }
    return items_.at(index);
}

void PlaylistManager::play(int index)
{
    if (items_.isEmpty()) {
        return;
    }

    if (index < 0) {
        // Use current index or start from beginning
        if (currentIndex_ < 0) {
            currentIndex_ = shuffle_ ? shuffledIndex(0) : 0;
        }
    } else if (index < items_.count()) {
        currentIndex_ = index;
    } else {
        return;
    }

    playing_ = true;
    playCurrentItem();

    emit playbackStarted(currentIndex_);
    emit currentIndexChanged(currentIndex_);
}

void PlaylistManager::stop()
{
    stopTimer();
    playing_ = false;
    emit playbackStopped();
}

void PlaylistManager::next()
{
    if (items_.isEmpty()) {
        return;
    }

    int nextIdx = nextIndex();
    if (nextIdx < 0) {
        // End of playlist
        stop();
        return;
    }

    stopTimer();
    currentIndex_ = nextIdx;
    emit currentIndexChanged(currentIndex_);

    if (playing_) {
        playCurrentItem();
    }
}

void PlaylistManager::previous()
{
    if (items_.isEmpty()) {
        return;
    }

    int prevIdx = previousIndex();
    if (prevIdx < 0) {
        return;
    }

    stopTimer();
    currentIndex_ = prevIdx;
    emit currentIndexChanged(currentIndex_);

    if (playing_) {
        playCurrentItem();
    }
}

void PlaylistManager::setShuffle(bool enabled)
{
    if (shuffle_ == enabled) {
        return;
    }

    shuffle_ = enabled;

    if (shuffle_) {
        generateShuffleOrder();
    }

    saveSettings();
    emit shuffleChanged(enabled);
}

void PlaylistManager::setRepeatMode(RepeatMode mode)
{
    if (repeatMode_ == mode) {
        return;
    }

    repeatMode_ = mode;
    saveSettings();
    emit repeatModeChanged(mode);
}

void PlaylistManager::setDefaultDuration(int seconds)
{
    seconds = std::clamp(seconds, 10, 3600);  // 10 seconds to 1 hour

    if (defaultDuration_ == seconds) {
        return;
    }

    defaultDuration_ = seconds;
    saveSettings();
    emit defaultDurationChanged(seconds);
}

void PlaylistManager::setItemDuration(int index, int seconds)
{
    if (index < 0 || index >= items_.count()) {
        return;
    }

    seconds = std::clamp(seconds, 10, 3600);

    items_[index].durationSecs = seconds;

    // If this is the current item and we're playing, restart timer
    if (index == currentIndex_ && playing_) {
        startTimer();
    }

    emit playlistChanged();
}

bool PlaylistManager::savePlaylist(const QString &filePath)
{
    QJsonObject root;
    root["version"] = 1;

    QJsonArray itemsArray;
    for (const PlaylistItem &item : items_) {
        QJsonObject itemObj;
        itemObj["path"] = item.path;
        itemObj["title"] = item.title;
        itemObj["author"] = item.author;
        itemObj["subsong"] = item.subsong;
        itemObj["totalSubsongs"] = item.totalSubsongs;
        itemObj["duration"] = item.durationSecs;
        itemsArray.append(itemObj);
    }
    root["items"] = itemsArray;

    QJsonDocument doc(root);
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

    QJsonObject root = doc.object();

    // Stop current playback before loading
    stop();
    items_.clear();
    currentIndex_ = -1;

    QJsonArray itemsArray = root["items"].toArray();
    for (const QJsonValue &value : itemsArray) {
        QJsonObject itemObj = value.toObject();
        PlaylistItem item;
        item.path = itemObj["path"].toString();
        item.title = itemObj["title"].toString();
        item.author = itemObj["author"].toString();
        item.subsong = itemObj["subsong"].toInt(1);
        item.totalSubsongs = itemObj["totalSubsongs"].toInt(1);
        item.durationSecs = itemObj["duration"].toInt(defaultDuration_);

        if (!item.path.isEmpty()) {
            items_.append(item);
        }
    }

    if (shuffle_) {
        generateShuffleOrder();
    }

    emit playlistChanged();
    return true;
}

void PlaylistManager::saveSettings()
{
    QSettings settings;
    settings.setValue("playlist/shuffle", shuffle_);
    settings.setValue("playlist/repeatMode", static_cast<int>(repeatMode_));
    settings.setValue("playlist/defaultDuration", defaultDuration_);
}

void PlaylistManager::loadSettings()
{
    QSettings settings;
    shuffle_ = settings.value("playlist/shuffle", false).toBool();
    repeatMode_ = static_cast<RepeatMode>(settings.value("playlist/repeatMode", 0).toInt());
    defaultDuration_ = settings.value("playlist/defaultDuration", 180).toInt();
}

void PlaylistManager::onAdvanceTimer()
{
    if (!playing_ || items_.isEmpty()) {
        return;
    }

    // Check repeat mode
    if (repeatMode_ == RepeatMode::One) {
        // Replay current track
        playCurrentItem();
        emit trackAdvanced(currentIndex_);
        return;
    }

    int nextIdx = nextIndex();
    if (nextIdx < 0) {
        // End of playlist
        if (repeatMode_ == RepeatMode::All) {
            // Start over
            currentIndex_ = shuffle_ ? shuffledIndex(0) : 0;
            playCurrentItem();
            emit currentIndexChanged(currentIndex_);
            emit trackAdvanced(currentIndex_);
        } else {
            stop();
        }
        return;
    }

    currentIndex_ = nextIdx;
    playCurrentItem();
    emit currentIndexChanged(currentIndex_);
    emit trackAdvanced(currentIndex_);
}

void PlaylistManager::startTimer()
{
    if (currentIndex_ < 0 || currentIndex_ >= items_.count()) {
        return;
    }

    int durationMs = items_[currentIndex_].durationSecs * 1000;
    advanceTimer_->start(durationMs);
}

void PlaylistManager::stopTimer()
{
    advanceTimer_->stop();
}

void PlaylistManager::playCurrentItem()
{
    if (currentIndex_ < 0 || currentIndex_ >= items_.count()) {
        return;
    }

    if (!deviceConnection_ || !deviceConnection_->canPerformOperations()) {
        emit statusMessage(tr("Not connected to device"), 3000);
        return;
    }

    const PlaylistItem &item = items_[currentIndex_];

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

void PlaylistManager::generateShuffleOrder()
{
    shuffleOrder_.clear();
    for (int i = 0; i < items_.count(); ++i) {
        shuffleOrder_.append(i);
    }

    // Fisher-Yates shuffle
    for (int i = shuffleOrder_.count() - 1; i > 0; --i) {
        int j = QRandomGenerator::global()->bounded(i + 1);
        std::swap(shuffleOrder_[i], shuffleOrder_[j]);
    }
}

int PlaylistManager::nextIndex() const
{
    if (items_.isEmpty()) {
        return -1;
    }

    if (shuffle_) {
        // Find current position in shuffle order
        qsizetype idx = shuffleOrder_.indexOf(currentIndex_);
        int shufflePos = (idx < 0) ? 0 : static_cast<int>(idx);
        int nextShufflePos = shufflePos + 1;
        if (nextShufflePos >= shuffleOrder_.count()) {
            return -1;  // End of shuffled list
        }
        return shuffleOrder_[nextShufflePos];
    } else {
        int next = currentIndex_ + 1;
        if (next >= items_.count()) {
            return -1;  // End of list
        }
        return next;
    }
}

int PlaylistManager::previousIndex() const
{
    if (items_.isEmpty()) {
        return -1;
    }

    if (shuffle_) {
        int shufflePos = shuffleOrder_.indexOf(currentIndex_);
        if (shufflePos <= 0) {
            return -1;  // At beginning
        }
        return shuffleOrder_[shufflePos - 1];
    } else {
        if (currentIndex_ <= 0) {
            return -1;  // At beginning
        }
        return currentIndex_ - 1;
    }
}

int PlaylistManager::shuffledIndex(int index) const
{
    if (index < 0 || index >= shuffleOrder_.count()) {
        return 0;
    }
    return shuffleOrder_[index];
}

int PlaylistManager::unshuffledIndex(int shuffledIdx) const
{
    return shuffleOrder_.indexOf(shuffledIdx);
}
