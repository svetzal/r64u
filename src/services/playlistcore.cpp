/**
 * @file playlistcore.cpp
 * @brief Implementation of pure playlist core functions.
 */

#include "playlistcore.h"

#include <QFileInfo>
#include <QJsonArray>
#include <QRandomGenerator>

#include <algorithm>

namespace playlist {

PlaylistItem createItem(const QString &path, int subsong, int defaultDuration)
{
    PlaylistItem item;
    item.path = path;
    item.subsong = subsong;
    item.durationSecs = defaultDuration;
    QFileInfo fileInfo(path);
    item.title = fileInfo.completeBaseName();
    return item;
}

State addItem(const State &state, const PlaylistItem &item, std::optional<quint32> seed)
{
    State result = state;
    result.items.append(item);

    if (result.shuffle) {
        quint32 actualSeed =
            seed.has_value() ? seed.value() : QRandomGenerator::global()->generate();
        result.shuffleOrder = generateShuffleOrder(result.items.size(), actualSeed);
    }

    return result;
}

RemoveResult removeItem(const State &state, int index)
{
    if (index < 0 || index >= state.items.count()) {
        return {state, false};
    }

    RemoveResult result;
    result.wasCurrentlyPlaying = (index == state.currentIndex);

    State newState = state;
    newState.items.removeAt(index);

    if (newState.shuffle) {
        quint32 seed = QRandomGenerator::global()->generate();
        newState.shuffleOrder = generateShuffleOrder(newState.items.size(), seed);
    }

    // Adjust current index
    if (newState.currentIndex >= newState.items.count()) {
        newState.currentIndex = newState.items.isEmpty() ? -1 : newState.items.count() - 1;
    } else if (index < newState.currentIndex) {
        newState.currentIndex--;
    }

    result.newState = newState;
    return result;
}

State moveItem(const State &state, int from, int to)
{
    if (from < 0 || from >= state.items.count() || to < 0 || to >= state.items.count() ||
        from == to) {
        return state;
    }

    State result = state;
    result.items.move(from, to);

    // Update current index if affected
    if (result.currentIndex == from) {
        result.currentIndex = to;
    } else if (from < result.currentIndex && to >= result.currentIndex) {
        result.currentIndex--;
    } else if (from > result.currentIndex && to <= result.currentIndex) {
        result.currentIndex++;
    }

    if (result.shuffle) {
        quint32 seed = QRandomGenerator::global()->generate();
        result.shuffleOrder = generateShuffleOrder(result.items.size(), seed);
    }

    return result;
}

State clear(const State &state)
{
    State result = state;
    result.items.clear();
    result.shuffleOrder.clear();
    result.currentIndex = -1;
    return result;
}

int nextIndex(const State &state)
{
    if (state.items.isEmpty()) {
        return -1;
    }

    if (state.shuffle) {
        qsizetype idx = state.shuffleOrder.indexOf(state.currentIndex);
        int shufflePos = (idx < 0) ? 0 : static_cast<int>(idx);
        int nextShufflePos = shufflePos + 1;
        if (nextShufflePos >= state.shuffleOrder.count()) {
            return -1;
        }
        return state.shuffleOrder[nextShufflePos];
    }

    int next = state.currentIndex + 1;
    if (next >= state.items.count()) {
        return -1;
    }
    return next;
}

int previousIndex(const State &state)
{
    if (state.items.isEmpty()) {
        return -1;
    }

    if (state.shuffle) {
        int shufflePos = static_cast<int>(state.shuffleOrder.indexOf(state.currentIndex));
        if (shufflePos <= 0) {
            return -1;
        }
        return state.shuffleOrder[shufflePos - 1];
    }

    if (state.currentIndex <= 0) {
        return -1;
    }
    return state.currentIndex - 1;
}

int advanceIndex(const State &state)
{
    if (state.items.isEmpty()) {
        return -1;
    }

    if (state.repeatMode == RepeatMode::One) {
        return state.currentIndex;
    }

    int next = nextIndex(state);

    if (next < 0) {
        // At end of playlist
        if (state.repeatMode == RepeatMode::All) {
            // Wrap to first (in shuffle or normal order)
            return state.shuffle ? state.shuffleOrder.first() : 0;
        }
        return -1;
    }

    return next;
}

QList<int> generateShuffleOrder(int itemCount, quint32 seed)
{
    QList<int> order;
    order.reserve(itemCount);
    for (int i = 0; i < itemCount; ++i) {
        order.append(i);
    }

    // Fisher-Yates shuffle with explicit seed
    QRandomGenerator rng(seed);
    for (int i = itemCount - 1; i > 0; --i) {
        int j = static_cast<int>(rng.bounded(i + 1));
        std::swap(order[i], order[j]);
    }

    return order;
}

QJsonObject serialize(const QList<PlaylistItem> &items)
{
    QJsonObject root;
    root["version"] = 1;

    QJsonArray itemsArray;
    for (const PlaylistItem &item : items) {
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

    return root;
}

QList<PlaylistItem> deserialize(const QJsonObject &json, int defaultDuration)
{
    QList<PlaylistItem> result;
    QJsonArray itemsArray = json["items"].toArray();

    for (const QJsonValue &value : itemsArray) {
        QJsonObject itemObj = value.toObject();
        PlaylistItem item;
        item.path = itemObj["path"].toString();
        item.title = itemObj["title"].toString();
        item.author = itemObj["author"].toString();
        item.subsong = itemObj["subsong"].toInt(1);
        item.totalSubsongs = itemObj["totalSubsongs"].toInt(1);
        item.durationSecs = itemObj["duration"].toInt(defaultDuration);

        if (!item.path.isEmpty()) {
            result.append(item);
        }
    }

    return result;
}

DurationUpdateResult updateItemDurations(const QList<PlaylistItem> &items, const QString &path,
                                         const QList<int> &subsongDurations)
{
    DurationUpdateResult result;
    result.items = items;

    for (int i = 0; i < result.items.count(); ++i) {
        if (result.items[i].path != path) {
            continue;
        }

        int subsongIndex = result.items[i].subsong - 1;  // Convert to 0-indexed
        if (subsongIndex < 0 || subsongIndex >= subsongDurations.size()) {
            continue;
        }

        int newDuration = subsongDurations.at(subsongIndex);
        if (result.items[i].durationSecs != newDuration) {
            result.items[i].durationSecs = newDuration;
            result.anyUpdated = true;
            result.updatedIndices.append(i);
        }
    }

    return result;
}

}  // namespace playlist
