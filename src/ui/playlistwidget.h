#ifndef PLAYLISTWIDGET_H
#define PLAYLISTWIDGET_H

#include "ui/imessagepresenter.h"
#include "ui/qmessageboxpresenter.h"

#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSpinBox>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QWidget>

class PlaylistService;
class QMimeData;

/**
 * @brief Widget for displaying and controlling a SID music playlist.
 *
 * Features:
 * - List display with current track highlighting
 * - Playback controls (play/stop/prev/next)
 * - Shuffle and repeat mode toggles
 * - Duration spinner for auto-advance timing
 * - Context menu for item management
 * - A disclosure header that folds the list away to a single row
 * - Drag-and-drop: SID files dropped from a device listing are added, and a
 *   track dragged within the list is moved (see handleDrop())
 */
class PlaylistWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PlaylistWidget(PlaylistService *manager, QWidget *parent = nullptr);
    ~PlaylistWidget() override = default;

    /**
     * @brief Folds the controls and the track list away, leaving only the header row.
     *
     * Collapsed, the widget's size hint (and maximum height) is the header row,
     * so a splitter hands the freed space to its neighbour. Playback expands
     * the list again so the playing track is visible.
     */
    void setCollapsed(bool collapsed);
    [[nodiscard]] bool isCollapsed() const { return collapsed_; }

    /**
     * @brief Replaces the message presenter used for the Clear confirmation.
     *
     * The default presenter shows real QMessageBox dialogs. Inject a test
     * double to verify confirmation behaviour without blocking the UI.
     *
     * @param presenter Non-owning pointer; must outlive this widget. Pass nullptr to
     *                  restore the default QMessageBoxPresenter.
     */
    void setMessagePresenter(IMessagePresenter *presenter);

    /**
     * @brief Whether a drag carrying @p mime may be dropped on the track list.
     *
     * A track from the list itself always may; remote paths may when at
     * least one of them is a SID file.
     * @param fromThisList The drag started in the track list.
     */
    [[nodiscard]] bool canAcceptDrag(const QMimeData *mime, bool fromThisList) const;

    /**
     * @brief Handles a drop on the track list.
     *
     * Remote paths (dropcore::kRemotePathsMimeType) add their SID files to the
     * playlist the way "Add to Playlist" does; anything else in them is
     * ignored. A track dragged from the list itself (@p fromThisList) is moved
     * to the gap at @p viewportPos, above or below the row there.
     * @return true when the drop changed the playlist.
     */
    bool handleDrop(const QMimeData *mime, const QPoint &viewportPos, bool fromThisList);

signals:
    /**
     * @brief Emitted for status messages to display in the status bar.
     */
    void statusMessage(const QString &message, int timeout = 0);

    /// Emitted when the list is folded away or opened again, by the user or by playback.
    void collapsedChanged(bool collapsed);

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
    void onItemDoubleClicked(QTreeWidgetItem *item, int column);
    void onContextMenu(const QPoint &pos);
    void onRemoveSelected();
    void onMoveUp();
    void onMoveDown();

    // Timer
    void onElapsedTimerTick();

protected:
    /**
     * @brief Watches the track list's viewport for drag and drop.
     *
     * The list is rebuilt on every playlist change, so the view must not move
     * its own items: the drop goes to PlaylistService and the rebuild repaints.
     */
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    void setupUi();
    void setupConnections();
    /// The SID files among the remote paths @p mime carries.
    static QStringList sidPathsIn(const QMimeData &mime);
    /// The gap a drop at @p viewportPos points at: before the row there, or after it (lower half).
    [[nodiscard]] int insertionRowAt(const QPoint &viewportPos) const;
    bool addDroppedTracks(const QMimeData &mime);
    bool moveDraggedTrack(const QPoint &viewportPos);
    /// Marks the list as a live drop target (the theme draws the border).
    void setDropActive(bool active);
    void updatePlaylistDisplay();
    void updateControlsState();
    void updateHeaderText();
    /// Transport buttons are icon-only; the file actions keep their text beside the icon.
    void showTextBesideIcon(QAction *action);
    void updateShuffleButton();
    void updateRepeatButton();
    void highlightCurrentItem();
    void updateElapsedTimeDisplay();

    PlaylistService *manager_ = nullptr;

    // Message presenter — owned default, swappable for tests (non-owning pointer).
    QMessageBoxPresenter defaultPresenter_;
    IMessagePresenter *presenter_ = &defaultPresenter_;

    // UI Components
    QToolButton *headerButton_ = nullptr;
    QWidget *durationRow_ = nullptr;
    QToolBar *controlBar_ = nullptr;
    QTreeWidget *treeWidget_ = nullptr;
    QSpinBox *durationSpinBox_ = nullptr;
    QLabel *elapsedTimeLabel_ = nullptr;
    QTimer *elapsedTimer_ = nullptr;
    int elapsedSeconds_ = 0;
    bool collapsed_ = false;

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

#endif  // PLAYLISTWIDGET_H
