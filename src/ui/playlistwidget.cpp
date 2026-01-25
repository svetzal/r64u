/**
 * @file playlistwidget.cpp
 * @brief Implementation of the PlaylistWidget.
 */

#include "playlistwidget.h"
#include "services/playlistmanager.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QToolButton>
#include <QFileInfo>
#include <QRegularExpression>
#include <QFileDialog>
#include <QStandardPaths>

PlaylistWidget::PlaylistWidget(PlaylistManager *manager, QWidget *parent)
    : QWidget(parent)
    , manager_(manager)
{
    Q_ASSERT(manager_ && "PlaylistManager is required");

    setupUi();
    setupConnections();
    updatePlaylistDisplay();
    updateControlsState();
}

void PlaylistWidget::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    // Header with duration spinner
    auto *headerLayout = new QHBoxLayout();

    headerLabel_ = new QLabel(tr("Playlist"));
    headerLabel_->setStyleSheet("font-weight: bold;");
    headerLayout->addWidget(headerLabel_);

    headerLayout->addStretch();

    auto *durationLabel = new QLabel(tr("Duration:"));
    headerLayout->addWidget(durationLabel);

    durationSpinBox_ = new QSpinBox();
    durationSpinBox_->setRange(1, 60);  // 1-60 minutes
    durationSpinBox_->setSuffix(tr(" min"));
    durationSpinBox_->setValue(manager_->defaultDuration() / 60);
    durationSpinBox_->setToolTip(tr("Duration before auto-advancing to next track"));
    headerLayout->addWidget(durationSpinBox_);

    layout->addLayout(headerLayout);

    // Control toolbar
    controlBar_ = new QToolBar();
    controlBar_->setIconSize(QSize(16, 16));
    controlBar_->setToolButtonStyle(Qt::ToolButtonTextOnly);

    playPauseAction_ = controlBar_->addAction(QString::fromUtf8("\u25B6"));  // Play triangle
    playPauseAction_->setToolTip(tr("Play"));
    connect(playPauseAction_, &QAction::triggered, this, &PlaylistWidget::onPlayPause);

    stopAction_ = controlBar_->addAction(QString::fromUtf8("\u25A0"));  // Stop square
    stopAction_->setToolTip(tr("Stop"));
    connect(stopAction_, &QAction::triggered, this, &PlaylistWidget::onStop);

    prevAction_ = controlBar_->addAction(QString::fromUtf8("\u23EE"));  // Previous
    prevAction_->setToolTip(tr("Previous track"));
    connect(prevAction_, &QAction::triggered, this, &PlaylistWidget::onPrevious);

    nextAction_ = controlBar_->addAction(QString::fromUtf8("\u23ED"));  // Next
    nextAction_->setToolTip(tr("Next track"));
    connect(nextAction_, &QAction::triggered, this, &PlaylistWidget::onNext);

    controlBar_->addSeparator();

    shuffleAction_ = controlBar_->addAction(QString::fromUtf8("\U0001F500"));  // Shuffle
    shuffleAction_->setToolTip(tr("Toggle shuffle"));
    shuffleAction_->setCheckable(true);
    shuffleAction_->setChecked(manager_->shuffle());
    connect(shuffleAction_, &QAction::triggered, this, &PlaylistWidget::onShuffleToggle);

    repeatAction_ = controlBar_->addAction(QString::fromUtf8("\U0001F501"));  // Repeat
    repeatAction_->setToolTip(tr("Cycle repeat mode (Off -> All -> One)"));
    connect(repeatAction_, &QAction::triggered, this, &PlaylistWidget::onRepeatCycle);
    updateRepeatButton();

    controlBar_->addSeparator();

    saveAction_ = controlBar_->addAction(tr("Save"));
    saveAction_->setToolTip(tr("Save playlist to file"));
    connect(saveAction_, &QAction::triggered, this, &PlaylistWidget::onSavePlaylist);

    loadAction_ = controlBar_->addAction(tr("Load"));
    loadAction_->setToolTip(tr("Load playlist from file"));
    connect(loadAction_, &QAction::triggered, this, &PlaylistWidget::onLoadPlaylist);

    controlBar_->addSeparator();

    clearAction_ = controlBar_->addAction(tr("Clear"));
    clearAction_->setToolTip(tr("Clear playlist"));
    connect(clearAction_, &QAction::triggered, this, &PlaylistWidget::onClear);

    layout->addWidget(controlBar_);

    // List widget
    listWidget_ = new QListWidget();
    listWidget_->setAlternatingRowColors(true);
    listWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    listWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    connect(listWidget_, &QListWidget::itemDoubleClicked,
            this, &PlaylistWidget::onItemDoubleClicked);
    connect(listWidget_, &QListWidget::customContextMenuRequested,
            this, &PlaylistWidget::onContextMenu);

    layout->addWidget(listWidget_);

    // Context menu
    contextMenu_ = new QMenu(this);
    removeAction_ = contextMenu_->addAction(tr("Remove"), this, &PlaylistWidget::onRemoveSelected);
    contextMenu_->addSeparator();
    moveUpAction_ = contextMenu_->addAction(tr("Move Up"), this, &PlaylistWidget::onMoveUp);
    moveDownAction_ = contextMenu_->addAction(tr("Move Down"), this, &PlaylistWidget::onMoveDown);
}

void PlaylistWidget::setupConnections()
{
    // Connect to manager signals
    connect(manager_, &PlaylistManager::playlistChanged,
            this, &PlaylistWidget::onPlaylistChanged);
    connect(manager_, &PlaylistManager::currentIndexChanged,
            this, &PlaylistWidget::onCurrentIndexChanged);
    connect(manager_, &PlaylistManager::playbackStarted,
            this, &PlaylistWidget::onPlaybackStarted);
    connect(manager_, &PlaylistManager::playbackStopped,
            this, &PlaylistWidget::onPlaybackStopped);
    connect(manager_, &PlaylistManager::shuffleChanged,
            this, &PlaylistWidget::onShuffleChanged);
    connect(manager_, &PlaylistManager::repeatModeChanged,
            this, &PlaylistWidget::onRepeatModeChanged);
    connect(manager_, &PlaylistManager::statusMessage,
            this, &PlaylistWidget::statusMessage);

    // Duration spinner
    connect(durationSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged),
            this, &PlaylistWidget::onDurationChanged);
}

void PlaylistWidget::onPlaylistChanged()
{
    updatePlaylistDisplay();
    updateControlsState();
}

void PlaylistWidget::onCurrentIndexChanged(int index)
{
    Q_UNUSED(index)
    highlightCurrentItem();
}

void PlaylistWidget::onPlaybackStarted(int index)
{
    Q_UNUSED(index)
    playPauseAction_->setText(QString::fromUtf8("\u23F8"));  // Pause
    playPauseAction_->setToolTip(tr("Pause (stop)"));
    highlightCurrentItem();
}

void PlaylistWidget::onPlaybackStopped()
{
    playPauseAction_->setText(QString::fromUtf8("\u25B6"));  // Play
    playPauseAction_->setToolTip(tr("Play"));
    highlightCurrentItem();
}

void PlaylistWidget::onShuffleChanged(bool enabled)
{
    shuffleAction_->setChecked(enabled);
    updateShuffleButton();
}

void PlaylistWidget::onRepeatModeChanged()
{
    updateRepeatButton();
}

void PlaylistWidget::onPlayPause()
{
    if (manager_->isPlaying()) {
        manager_->stop();
    } else {
        manager_->play();
    }
}

void PlaylistWidget::onStop()
{
    manager_->stop();
}

void PlaylistWidget::onPrevious()
{
    manager_->previous();
}

void PlaylistWidget::onNext()
{
    manager_->next();
}

void PlaylistWidget::onShuffleToggle()
{
    manager_->setShuffle(!manager_->shuffle());
}

void PlaylistWidget::onRepeatCycle()
{
    // Cycle: Off -> All -> One -> Off
    switch (manager_->repeatMode()) {
    case PlaylistManager::RepeatMode::Off:
        manager_->setRepeatMode(PlaylistManager::RepeatMode::All);
        break;
    case PlaylistManager::RepeatMode::All:
        manager_->setRepeatMode(PlaylistManager::RepeatMode::One);
        break;
    case PlaylistManager::RepeatMode::One:
        manager_->setRepeatMode(PlaylistManager::RepeatMode::Off);
        break;
    }
}

void PlaylistWidget::onClear()
{
    manager_->clear();
}

void PlaylistWidget::onDurationChanged(int value)
{
    manager_->setDefaultDuration(value * 60);  // Convert minutes to seconds
}

void PlaylistWidget::onSavePlaylist()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getSaveFileName(
        this,
        tr("Save Playlist"),
        defaultDir + "/playlist.json",
        tr("Playlist Files (*.json);;All Files (*)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    if (manager_->savePlaylist(filePath)) {
        emit statusMessage(tr("Playlist saved: %1").arg(filePath), 3000);
    } else {
        emit statusMessage(tr("Failed to save playlist"), 3000);
    }
}

void PlaylistWidget::onLoadPlaylist()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getOpenFileName(
        this,
        tr("Load Playlist"),
        defaultDir,
        tr("Playlist Files (*.json);;All Files (*)")
    );

    if (filePath.isEmpty()) {
        return;
    }

    if (manager_->loadPlaylist(filePath)) {
        emit statusMessage(tr("Playlist loaded: %1").arg(filePath), 3000);
    } else {
        emit statusMessage(tr("Failed to load playlist"), 3000);
    }
}

void PlaylistWidget::onItemDoubleClicked(QListWidgetItem *item)
{
    int index = listWidget_->row(item);
    manager_->play(index);
}

void PlaylistWidget::onContextMenu(const QPoint &pos)
{
    QListWidgetItem *item = listWidget_->itemAt(pos);
    if (!item) {
        return;
    }

    int index = listWidget_->row(item);
    moveUpAction_->setEnabled(index > 0);
    moveDownAction_->setEnabled(index < listWidget_->count() - 1);

    contextMenu_->exec(listWidget_->mapToGlobal(pos));
}

void PlaylistWidget::onRemoveSelected()
{
    QListWidgetItem *item = listWidget_->currentItem();
    if (item) {
        int index = listWidget_->row(item);
        manager_->removeItem(index);
    }
}

void PlaylistWidget::onMoveUp()
{
    QListWidgetItem *item = listWidget_->currentItem();
    if (item) {
        int index = listWidget_->row(item);
        if (index > 0) {
            manager_->moveItem(index, index - 1);
            listWidget_->setCurrentRow(index - 1);
        }
    }
}

void PlaylistWidget::onMoveDown()
{
    QListWidgetItem *item = listWidget_->currentItem();
    if (item) {
        int index = listWidget_->row(item);
        if (index < listWidget_->count() - 1) {
            manager_->moveItem(index, index + 1);
            listWidget_->setCurrentRow(index + 1);
        }
    }
}

void PlaylistWidget::updatePlaylistDisplay()
{
    listWidget_->clear();

    const auto items = manager_->items();
    for (int i = 0; i < items.count(); ++i) {
        const auto &item = items[i];

        QString displayText;
        if (!item.title.isEmpty()) {
            displayText = item.title;
            if (!item.author.isEmpty()) {
                displayText += QString(" - %1").arg(item.author);
            }
        } else {
            displayText = QFileInfo(item.path).fileName();
        }

        if (item.totalSubsongs > 1) {
            displayText += QString(" [%1/%2]").arg(item.subsong).arg(item.totalSubsongs);
        }

        auto *listItem = new QListWidgetItem(QString("%1. %2").arg(i + 1).arg(displayText));
        listItem->setData(Qt::UserRole, i);
        listWidget_->addItem(listItem);
    }

    highlightCurrentItem();

    // Update header
    int count = manager_->count();
    if (count == 0) {
        headerLabel_->setText(tr("Playlist"));
    } else {
        headerLabel_->setText(tr("Playlist (%1 tracks)").arg(count));
    }
}

void PlaylistWidget::updateControlsState()
{
    bool hasItems = !manager_->isEmpty();
    bool isPlaying = manager_->isPlaying();

    stopAction_->setEnabled(isPlaying);
    prevAction_->setEnabled(hasItems);
    nextAction_->setEnabled(hasItems);
    clearAction_->setEnabled(hasItems);
    saveAction_->setEnabled(hasItems);
    playPauseAction_->setEnabled(hasItems);
}

void PlaylistWidget::updateShuffleButton()
{
    if (manager_->shuffle()) {
        shuffleAction_->setToolTip(tr("Shuffle: ON"));
    } else {
        shuffleAction_->setToolTip(tr("Shuffle: OFF"));
    }
}

void PlaylistWidget::updateRepeatButton()
{
    switch (manager_->repeatMode()) {
    case PlaylistManager::RepeatMode::Off:
        repeatAction_->setText(QString::fromUtf8("\U0001F501"));  // Normal repeat icon
        repeatAction_->setToolTip(tr("Repeat: OFF"));
        break;
    case PlaylistManager::RepeatMode::All:
        repeatAction_->setText(QString::fromUtf8("\U0001F501"));  // Repeat icon
        repeatAction_->setToolTip(tr("Repeat: ALL"));
        break;
    case PlaylistManager::RepeatMode::One:
        repeatAction_->setText(QString::fromUtf8("\U0001F502"));  // Repeat one icon
        repeatAction_->setToolTip(tr("Repeat: ONE"));
        break;
    }
}

void PlaylistWidget::highlightCurrentItem()
{
    int currentIndex = manager_->currentIndex();
    bool isPlaying = manager_->isPlaying();

    for (int i = 0; i < listWidget_->count(); ++i) {
        QListWidgetItem *item = listWidget_->item(i);
        QFont font = item->font();

        if (i == currentIndex) {
            font.setBold(true);
            if (isPlaying) {
                // Add play indicator to current item
                QString text = item->text();
                if (!text.startsWith(QString::fromUtf8("\u25B6 "))) {
                    // Remove any existing indicator and add new one
                    text = text.remove(QRegularExpression("^[0-9]+\\."));
                    item->setText(QString::fromUtf8("\u25B6 %1.%2").arg(i + 1).arg(text));
                }
            } else {
                // Remove play indicator if stopped
                QString text = item->text();
                if (text.startsWith(QString::fromUtf8("\u25B6 "))) {
                    text = text.mid(2);
                    item->setText(text);
                }
            }
        } else {
            font.setBold(false);
            // Ensure no play indicator on non-current items
            QString text = item->text();
            if (text.startsWith(QString::fromUtf8("\u25B6 "))) {
                text = text.mid(2);
                item->setText(text);
            }
        }
        item->setFont(font);
    }
}
