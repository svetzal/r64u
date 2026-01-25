/**
 * @file playlistwidget.h
 * @brief UI widget for displaying and controlling a SID music playlist.
 */

#ifndef PLAYLISTWIDGET_H
#define PLAYLISTWIDGET_H

#include <QWidget>
#include <QListWidget>
#include <QToolBar>
#include <QSpinBox>
#include <QMenu>
#include <QLabel>
#include <QPushButton>

class PlaylistManager;

/**
 * @brief Widget for displaying and controlling a SID music playlist.
 *
 * Features:
 * - List display with current track highlighting
 * - Playback controls (play/stop/prev/next)
 * - Shuffle and repeat mode toggles
 * - Duration spinner for auto-advance timing
 * - Context menu for item management
 * - Drag-and-drop reordering (future)
 */
class PlaylistWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(PlaylistManager *manager, QWidget *parent = nullptr);
    ~PlaylistWidget() override = default;

signals:
    /**
     * @brief Emitted for status messages to display in the status bar.
     */
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onPlaylistChanged();
    void onCurrentIndexChanged(int index);
    void onPlaybackStarted(int index);
    void onPlaybackStopped();
    void onShuffleChanged(bool enabled);
    void onRepeatModeChanged();

    // Control actions
    void onPlayPause();
    void onStop();
    void onPrevious();
    void onNext();
    void onShuffleToggle();
    void onRepeatCycle();
    void onClear();

    // Duration
    void onDurationChanged(int value);

    // File operations
    void onSavePlaylist();
    void onLoadPlaylist();

    // List interactions
    void onItemDoubleClicked(QListWidgetItem *item);
    void onContextMenu(const QPoint &pos);
    void onRemoveSelected();
    void onMoveUp();
    void onMoveDown();

private:
    void setupUi();
    void setupConnections();
    void updatePlaylistDisplay();
    void updateControlsState();
    void updateShuffleButton();
    void updateRepeatButton();
    void highlightCurrentItem();

    PlaylistManager *manager_ = nullptr;

    // UI Components
    QLabel *headerLabel_ = nullptr;
    QToolBar *controlBar_ = nullptr;
    QListWidget *listWidget_ = nullptr;
    QSpinBox *durationSpinBox_ = nullptr;

    // Control actions
    QAction *playPauseAction_ = nullptr;
    QAction *stopAction_ = nullptr;
    QAction *prevAction_ = nullptr;
    QAction *nextAction_ = nullptr;
    QAction *shuffleAction_ = nullptr;
    QAction *repeatAction_ = nullptr;
    QAction *clearAction_ = nullptr;
    QAction *saveAction_ = nullptr;
    QAction *loadAction_ = nullptr;

    // Context menu
    QMenu *contextMenu_ = nullptr;
    QAction *removeAction_ = nullptr;
    QAction *moveUpAction_ = nullptr;
    QAction *moveDownAction_ = nullptr;
};

#endif // PLAYLISTWIDGET_H
