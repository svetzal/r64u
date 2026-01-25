/**
 * @file playlistmanager.h
 * @brief Service for managing SID music playlists with playback control.
 *
 * Provides a jukebox-style playlist for SID files with timer-based
 * auto-advance, shuffle, and repeat modes.
 */

#ifndef PLAYLISTMANAGER_H
#define PLAYLISTMANAGER_H

#include <QObject>
#include <QString>
#include <QList>
#include <QTimer>

class DeviceConnection;

/**
 * @brief Manages SID music playlists with playback control.
 *
 * Features:
 * - Queue multiple SID files for playback
 * - Timer-based auto-advance (since SIDs loop indefinitely)
 * - Shuffle and repeat modes
 * - Settings persistence
 * - Playlist file save/load (.json format)
 */
class PlaylistManager : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Single item in the playlist.
     */
    struct PlaylistItem {
        QString path;           ///< Remote file path on the Ultimate device
        QString title;          ///< Song title (from SID metadata or filename)
        QString author;         ///< Composer name (from SID metadata)
        int subsong = 1;        ///< Which subsong to play (1-indexed for display)
        int totalSubsongs = 1;  ///< Total subsongs in the file
        int durationSecs = 180; ///< Duration before advancing (default 3 minutes)
    };

    /**
     * @brief Repeat mode for playlist playback.
     */
    enum class RepeatMode {
        Off,        ///< Stop after last track
        All,        ///< Restart from beginning after last track
        One         ///< Repeat current track indefinitely
    };

    explicit PlaylistManager(DeviceConnection *connection, QObject *parent = nullptr);
    ~PlaylistManager() override = default;

    /// @name Playlist Management
    /// @{

    /**
     * @brief Adds a SID file to the playlist.
     * @param path Remote file path.
     * @param subsong Subsong number (1-indexed, default 1).
     *
     * The title and author are extracted from the file metadata
     * when the item is first played or when fetchMetadata() is called.
     */
    void addItem(const QString &path, int subsong = 1);

    /**
     * @brief Adds a SID file with pre-populated metadata.
     */
    void addItem(const PlaylistItem &item);

    /**
     * @brief Removes an item from the playlist by index.
     */
    void removeItem(int index);

    /**
     * @brief Moves an item within the playlist.
     */
    void moveItem(int from, int to);

    /**
     * @brief Clears all items from the playlist.
     */
    void clear();

    /**
     * @brief Returns all items in the playlist.
     */
    [[nodiscard]] QList<PlaylistItem> items() const { return items_; }

    /**
     * @brief Returns the number of items in the playlist.
     */
    [[nodiscard]] int count() const { return items_.count(); }

    /**
     * @brief Returns true if the playlist is empty.
     */
    [[nodiscard]] bool isEmpty() const { return items_.isEmpty(); }

    /**
     * @brief Returns the item at the specified index.
     */
    [[nodiscard]] PlaylistItem itemAt(int index) const;

    /// @}

    /// @name Playback Control
    /// @{

    /**
     * @brief Starts playback.
     * @param index Index to start from (-1 = current or first).
     */
    void play(int index = -1);

    /**
     * @brief Stops playback.
     */
    void stop();

    /**
     * @brief Advances to the next track.
     */
    void next();

    /**
     * @brief Goes back to the previous track.
     */
    void previous();

    /**
     * @brief Returns the currently playing index (-1 if none).
     */
    [[nodiscard]] int currentIndex() const { return currentIndex_; }

    /**
     * @brief Returns true if playback is active.
     */
    [[nodiscard]] bool isPlaying() const { return playing_; }

    /// @}

    /// @name Settings
    /// @{

    /**
     * @brief Enables or disables shuffle mode.
     */
    void setShuffle(bool enabled);

    /**
     * @brief Returns true if shuffle mode is enabled.
     */
    [[nodiscard]] bool shuffle() const { return shuffle_; }

    /**
     * @brief Sets the repeat mode.
     */
    void setRepeatMode(RepeatMode mode);

    /**
     * @brief Returns the current repeat mode.
     */
    [[nodiscard]] RepeatMode repeatMode() const { return repeatMode_; }

    /**
     * @brief Sets the default track duration in seconds.
     */
    void setDefaultDuration(int seconds);

    /**
     * @brief Returns the default track duration in seconds.
     */
    [[nodiscard]] int defaultDuration() const { return defaultDuration_; }

    /**
     * @brief Sets the duration for a specific item.
     */
    void setItemDuration(int index, int seconds);

    /// @}

    /// @name Persistence
    /// @{

    /**
     * @brief Saves the playlist to a JSON file.
     * @param filePath Path to save to.
     * @return True if successful.
     */
    bool savePlaylist(const QString &filePath);

    /**
     * @brief Loads a playlist from a JSON file.
     * @param filePath Path to load from.
     * @return True if successful.
     */
    bool loadPlaylist(const QString &filePath);

    /**
     * @brief Saves settings (shuffle, repeat, duration) to QSettings.
     */
    void saveSettings();

    /**
     * @brief Loads settings from QSettings.
     */
    void loadSettings();

    /// @}

signals:
    /**
     * @brief Emitted when the playlist contents change.
     */
    void playlistChanged();

    /**
     * @brief Emitted when the current track index changes.
     */
    void currentIndexChanged(int index);

    /**
     * @brief Emitted when playback starts.
     */
    void playbackStarted(int index);

    /**
     * @brief Emitted when playback stops.
     */
    void playbackStopped();

    /**
     * @brief Emitted when auto-advancing to the next track.
     */
    void trackAdvanced(int newIndex);

    /**
     * @brief Emitted when shuffle mode changes.
     */
    void shuffleChanged(bool enabled);

    /**
     * @brief Emitted when repeat mode changes.
     */
    void repeatModeChanged(RepeatMode mode);

    /**
     * @brief Emitted when default duration changes.
     */
    void defaultDurationChanged(int seconds);

    /**
     * @brief Emitted for status messages.
     */
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onAdvanceTimer();

private:
    void startTimer();
    void stopTimer();
    void playCurrentItem();
    void generateShuffleOrder();
    [[nodiscard]] int nextIndex() const;
    [[nodiscard]] int previousIndex() const;
    [[nodiscard]] int shuffledIndex(int index) const;
    [[nodiscard]] int unshuffledIndex(int shuffledIdx) const;

    DeviceConnection *deviceConnection_ = nullptr;

    QList<PlaylistItem> items_;
    QList<int> shuffleOrder_;  // Maps shuffled position to actual index
    int currentIndex_ = -1;
    bool playing_ = false;
    bool shuffle_ = false;
    RepeatMode repeatMode_ = RepeatMode::Off;
    int defaultDuration_ = 180;  // 3 minutes default

    QTimer *advanceTimer_ = nullptr;
};

#endif // PLAYLISTMANAGER_H
