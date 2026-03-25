/**
 * @file playlistcore.h
 * @brief Pure core functions for playlist management logic.
 *
 * All functions in this namespace are pure: they take immutable input and return
 * new output with no side effects. This enables comprehensive unit testing and
 * clean separation from I/O concerns (REST calls, QSettings, QTimer).
 */

#ifndef PLAYLISTCORE_H
#define PLAYLISTCORE_H

#include <QJsonObject>
#include <QList>
#include <QString>

#include <optional>

namespace playlist {

/**
 * @brief Single item in the playlist.
 */
struct PlaylistItem
{
    QString path;            ///< Remote file path on the Ultimate device
    QString title;           ///< Song title (from SID metadata or filename)
    QString author;          ///< Composer name (from SID metadata)
    int subsong = 1;         ///< Which subsong to play (1-indexed for display)
    int totalSubsongs = 1;   ///< Total subsongs in the file
    int durationSecs = 180;  ///< Duration before advancing (default 3 minutes)
};

/**
 * @brief Repeat mode for playlist playback.
 */
enum class RepeatMode {
    Off,  ///< Stop after last track
    All,  ///< Restart from beginning after last track
    One   ///< Repeat current track indefinitely
};

/**
 * @brief Immutable playlist state.
 *
 * All playlist state is captured here so that pure functions can compute
 * transitions without any mutable member access.
 */
struct State
{
    QList<PlaylistItem> items;
    QList<int> shuffleOrder;  ///< Maps shuffled position to actual item index
    int currentIndex = -1;
    bool shuffle = false;
    RepeatMode repeatMode = RepeatMode::Off;
    int defaultDuration = 180;  ///< 3 minutes default
};

/**
 * @brief Result of removing an item from the playlist.
 */
struct RemoveResult
{
    State newState;
    bool wasCurrentlyPlaying = false;  ///< True if the removed item was the current index
};

/**
 * @brief Result of updating item durations from SID data.
 */
struct DurationUpdateResult
{
    QList<PlaylistItem> items;
    bool anyUpdated = false;
    QList<int> updatedIndices;
};

// ---------------------------------------------------------------------------
// Item creation
// ---------------------------------------------------------------------------

/**
 * @brief Creates a playlist item from a remote path and subsong number.
 * @param path Remote file path.
 * @param subsong Subsong number (1-indexed).
 * @param defaultDuration Default duration in seconds.
 * @return Populated PlaylistItem with title extracted from filename.
 */
[[nodiscard]] PlaylistItem createItem(const QString &path, int subsong, int defaultDuration);

// ---------------------------------------------------------------------------
// List mutations (all return new State)
// ---------------------------------------------------------------------------

/**
 * @brief Adds an item to the playlist and regenerates shuffle order if needed.
 * @param state Current state.
 * @param item Item to add.
 * @param seed Optional seed for shuffle order generation (for deterministic tests).
 * @return New state with item appended.
 */
[[nodiscard]] State addItem(const State &state, const PlaylistItem &item,
                            std::optional<quint32> seed = std::nullopt);

/**
 * @brief Removes the item at @p index and adjusts currentIndex accordingly.
 * @param state Current state.
 * @param index Index of the item to remove.
 * @return RemoveResult containing new state and whether the current item was removed.
 */
[[nodiscard]] RemoveResult removeItem(const State &state, int index);

/**
 * @brief Moves an item within the playlist, updating currentIndex as needed.
 * @param state Current state.
 * @param from Source index.
 * @param to Destination index.
 * @return New state after the move, or unchanged state if indices are invalid.
 */
[[nodiscard]] State moveItem(const State &state, int from, int to);

/**
 * @brief Clears all items from the playlist.
 * @param state Current state.
 * @return Empty state preserving shuffle/repeat/duration settings.
 */
[[nodiscard]] State clear(const State &state);

// ---------------------------------------------------------------------------
// Navigation
// ---------------------------------------------------------------------------

/**
 * @brief Computes the next item index, respecting shuffle mode.
 * @param state Current state.
 * @return Next index, or -1 if at the end of the list.
 */
[[nodiscard]] int nextIndex(const State &state);

/**
 * @brief Computes the previous item index, respecting shuffle mode.
 * @param state Current state.
 * @return Previous index, or -1 if at the beginning of the list.
 */
[[nodiscard]] int previousIndex(const State &state);

/**
 * @brief Computes the next index to advance to, applying repeat mode logic.
 *
 * - RepeatMode::One  → returns currentIndex (replay current)
 * - RepeatMode::All  → wraps around to first item when past the end
 * - RepeatMode::Off  → returns -1 when past the end
 *
 * @param state Current state.
 * @return The new currentIndex to advance to, or -1 to stop playback.
 */
[[nodiscard]] int advanceIndex(const State &state);

// ---------------------------------------------------------------------------
// Shuffle
// ---------------------------------------------------------------------------

/**
 * @brief Generates a Fisher-Yates shuffled order for @p itemCount items.
 * @param itemCount Number of items to shuffle.
 * @param seed Explicit seed for deterministic results in tests.
 * @return Shuffled list of indices 0..itemCount-1.
 */
[[nodiscard]] QList<int> generateShuffleOrder(int itemCount, quint32 seed);

// ---------------------------------------------------------------------------
// Serialization
// ---------------------------------------------------------------------------

/**
 * @brief Serializes a list of playlist items to a JSON object.
 * @param items Items to serialize.
 * @return JSON object with "version" and "items" fields.
 */
[[nodiscard]] QJsonObject serialize(const QList<PlaylistItem> &items);

/**
 * @brief Deserializes playlist items from a JSON object.
 * @param json JSON object as produced by serialize().
 * @param defaultDuration Duration used when the JSON entry lacks a "duration" field.
 * @return List of deserialized items (skips entries with empty paths).
 */
[[nodiscard]] QList<PlaylistItem> deserialize(const QJsonObject &json, int defaultDuration);

// ---------------------------------------------------------------------------
// Duration updates
// ---------------------------------------------------------------------------

/**
 * @brief Updates playlist item durations for a given path from subsong duration data.
 * @param items Current item list.
 * @param path Remote path of the SID file.
 * @param subsongDurations List of durations (seconds) indexed by 0-based subsong number.
 * @return DurationUpdateResult with the updated item list and metadata.
 */
[[nodiscard]] DurationUpdateResult updateItemDurations(const QList<PlaylistItem> &items,
                                                       const QString &path,
                                                       const QList<int> &subsongDurations);

}  // namespace playlist

#endif  // PLAYLISTCORE_H
