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
#include <QHeaderView>

PlaylistWidget::PlaylistWidget(PlaylistManager *manager, QWidget *parent)
    : QWidget(parent)
    , manager_(manager)
    , elapsedTimer_(new QTimer(this))
{
    Q_ASSERT(manager_ && "PlaylistManager is required");

    elapsedTimer_->setInterval(1000);  // 1 second updates
    connect(elapsedTimer_, &QTimer::timeout, this, &PlaylistWidget::onElapsedTimerTick);

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

    // Elapsed time label
    elapsedTimeLabel_ = new QLabel(tr("--:-- / --:--"));
    elapsedTimeLabel_->setToolTip(tr("Elapsed / Total duration"));
    controlBar_->addWidget(elapsedTimeLabel_);

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

    // Tree widget with columns
    treeWidget_ = new QTreeWidget();
    treeWidget_->setAlternatingRowColors(true);
    treeWidget_->setContextMenuPolicy(Qt::CustomContextMenu);
    treeWidget_->setSelectionMode(QAbstractItemView::SingleSelection);
    treeWidget_->setRootIsDecorated(false);
    treeWidget_->setHeaderLabels({QString(), tr("#"), tr("Title"), tr("Length")});
    treeWidget_->setColumnWidth(0, 24);   // Play marker
    treeWidget_->setColumnWidth(1, 30);   // Track number
    treeWidget_->setColumnWidth(3, 50);   // Length
    treeWidget_->header()->setStretchLastSection(false);
    treeWidget_->header()->setSectionResizeMode(2, QHeaderView::Stretch);  // Title stretches

    connect(treeWidget_, &QTreeWidget::itemDoubleClicked,
            this, &PlaylistWidget::onItemDoubleClicked);
    connect(treeWidget_, &QTreeWidget::customContextMenuRequested,
            this, &PlaylistWidget::onContextMenu);

    layout->addWidget(treeWidget_);

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

    // Start elapsed timer
    elapsedSeconds_ = 0;
    updateElapsedTimeDisplay();
    elapsedTimer_->start();
}

void PlaylistWidget::onPlaybackStopped()
{
    playPauseAction_->setText(QString::fromUtf8("\u25B6"));  // Play
    playPauseAction_->setToolTip(tr("Play"));
    highlightCurrentItem();

    // Stop elapsed timer and reset display
    elapsedTimer_->stop();
    elapsedTimeLabel_->setText(tr("--:-- / --:--"));
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

void PlaylistWidget::onItemDoubleClicked(QTreeWidgetItem *item, int column)
{
    Q_UNUSED(column)
    int index = treeWidget_->indexOfTopLevelItem(item);
    manager_->play(index);
}

void PlaylistWidget::onContextMenu(const QPoint &pos)
{
    QTreeWidgetItem *item = treeWidget_->itemAt(pos);
    if (!item) {
        return;
    }

    int index = treeWidget_->indexOfTopLevelItem(item);
    moveUpAction_->setEnabled(index > 0);
    moveDownAction_->setEnabled(index < treeWidget_->topLevelItemCount() - 1);

    contextMenu_->exec(treeWidget_->mapToGlobal(pos));
}

void PlaylistWidget::onRemoveSelected()
{
    QTreeWidgetItem *item = treeWidget_->currentItem();
    if (item) {
        int index = treeWidget_->indexOfTopLevelItem(item);
        manager_->removeItem(index);
    }
}

void PlaylistWidget::onMoveUp()
{
    QTreeWidgetItem *item = treeWidget_->currentItem();
    if (item) {
        int index = treeWidget_->indexOfTopLevelItem(item);
        if (index > 0) {
            manager_->moveItem(index, index - 1);
            treeWidget_->setCurrentItem(treeWidget_->topLevelItem(index - 1));
        }
    }
}

void PlaylistWidget::onMoveDown()
{
    QTreeWidgetItem *item = treeWidget_->currentItem();
    if (item) {
        int index = treeWidget_->indexOfTopLevelItem(item);
        if (index < treeWidget_->topLevelItemCount() - 1) {
            manager_->moveItem(index, index + 1);
            treeWidget_->setCurrentItem(treeWidget_->topLevelItem(index + 1));
        }
    }
}

void PlaylistWidget::updatePlaylistDisplay()
{
    treeWidget_->clear();

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

        // Format duration as mm:ss
        QString durationStr = formatTime(item.durationSecs);

        auto *treeItem = new QTreeWidgetItem();
        treeItem->setText(0, QString());  // Play marker (set in highlightCurrentItem)
        treeItem->setText(1, QString::number(i + 1));
        treeItem->setText(2, displayText);
        treeItem->setText(3, durationStr);
        treeItem->setData(0, Qt::UserRole, i);
        treeItem->setTextAlignment(1, Qt::AlignRight | Qt::AlignVCenter);
        treeItem->setTextAlignment(3, Qt::AlignRight | Qt::AlignVCenter);
        treeWidget_->addTopLevelItem(treeItem);
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

    for (int i = 0; i < treeWidget_->topLevelItemCount(); ++i) {
        QTreeWidgetItem *item = treeWidget_->topLevelItem(i);
        QFont font = item->font(0);

        if (i == currentIndex) {
            font.setBold(true);
            // Show play indicator in first column
            item->setText(0, isPlaying ? QString::fromUtf8("\u25B6") : QString());
        } else {
            font.setBold(false);
            item->setText(0, QString());
        }

        // Apply font to all columns
        for (int col = 0; col < 4; ++col) {
            item->setFont(col, font);
        }
    }
}

QString PlaylistWidget::formatTime(int seconds)
{
    int mins = seconds / 60;
    int secs = seconds % 60;
    return QString("%1:%2").arg(mins).arg(secs, 2, 10, QChar('0'));
}

void PlaylistWidget::updateElapsedTimeDisplay()
{
    if (!manager_->isPlaying() || manager_->currentIndex() < 0) {
        elapsedTimeLabel_->setText(tr("--:-- / --:--"));
        return;
    }

    const auto &item = manager_->itemAt(manager_->currentIndex());
    QString elapsed = formatTime(elapsedSeconds_);
    QString total = formatTime(item.durationSecs);
    elapsedTimeLabel_->setText(QString("%1 / %2").arg(elapsed).arg(total));
}

void PlaylistWidget::onElapsedTimerTick()
{
    elapsedSeconds_++;
    updateElapsedTimeDisplay();
}
