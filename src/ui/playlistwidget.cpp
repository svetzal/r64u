/**
 * @file playlistwidget.cpp
 * @brief Implementation of the PlaylistWidget.
 */

#include "playlistwidget.h"

#include "pixelicons.h"

#include "core/dropcore.h"
#include "core/filebrowsercore.h"
#include "core/filetypecore.h"
#include "core/playlistcore.h"
#include "services/playlistservice.h"

#include <QDragMoveEvent>
#include <QDropEvent>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QMimeData>
#include <QStandardPaths>
#include <QStyle>
#include <QToolButton>
#include <QVBoxLayout>

PlaylistWidget::PlaylistWidget(PlaylistService *manager, QWidget *parent)
    : QWidget(parent), manager_(manager), elapsedTimer_(new QTimer(this))
{
    Q_ASSERT(manager_ && "PlaylistService is required");

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

    // Header: a disclosure button that folds the list away, and the duration spinner
    auto *headerLayout = new QHBoxLayout();

    headerButton_ = new QToolButton();
    headerButton_->setObjectName(QStringLiteral("PlaylistHeader"));
    headerButton_->setAutoRaise(true);
    headerButton_->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    headerButton_->setArrowType(Qt::DownArrow);
    headerButton_->setStyleSheet("font-weight: bold;");
    headerButton_->setToolTip(tr("Show or hide the playlist"));
    connect(headerButton_, &QToolButton::clicked, this, [this]() { setCollapsed(!collapsed_); });
    headerLayout->addWidget(headerButton_);

    headerLayout->addStretch();

    durationRow_ = new QWidget();
    auto *durationLayout = new QHBoxLayout(durationRow_);
    durationLayout->setContentsMargins(0, 0, 0, 0);
    durationLayout->addWidget(new QLabel(tr("Duration:")));

    durationSpinBox_ = new QSpinBox();
    durationSpinBox_->setRange(1, 60);  // 1-60 minutes
    durationSpinBox_->setSuffix(tr(" min"));
    durationSpinBox_->setValue(manager_->defaultDuration() / 60);
    durationSpinBox_->setToolTip(tr("Duration before auto-advancing to next track"));
    durationLayout->addWidget(durationSpinBox_);
    headerLayout->addWidget(durationRow_);

    layout->addLayout(headerLayout);

    // Control toolbar
    controlBar_ = new QToolBar();
    controlBar_->setIconSize(QSize(16, 16));
    controlBar_->setToolButtonStyle(Qt::ToolButtonIconOnly);

    using pixeliconcore::Icon;
    playPauseAction_ = controlBar_->addAction(pixelicons::icon(Icon::PlaylistPlay), tr("Play"));
    playPauseAction_->setToolTip(tr("Play"));
    connect(playPauseAction_, &QAction::triggered, this, &PlaylistWidget::onPlayPause);

    stopAction_ = controlBar_->addAction(pixelicons::icon(Icon::PlaylistStop), tr("Stop"));
    stopAction_->setToolTip(tr("Stop (resets C64)"));
    connect(stopAction_, &QAction::triggered, this, &PlaylistWidget::onStop);

    prevAction_ = controlBar_->addAction(pixelicons::icon(Icon::Previous), tr("Previous"));
    prevAction_->setToolTip(tr("Previous track"));
    connect(prevAction_, &QAction::triggered, this, &PlaylistWidget::onPrevious);

    nextAction_ = controlBar_->addAction(pixelicons::icon(Icon::Next), tr("Next"));
    nextAction_->setToolTip(tr("Next track"));
    connect(nextAction_, &QAction::triggered, this, &PlaylistWidget::onNext);

    controlBar_->addSeparator();

    // Elapsed time label
    elapsedTimeLabel_ = new QLabel(tr("--:-- / --:--"));
    elapsedTimeLabel_->setToolTip(tr("Elapsed / Total duration"));
    controlBar_->addWidget(elapsedTimeLabel_);

    controlBar_->addSeparator();

    shuffleAction_ = controlBar_->addAction(pixelicons::icon(Icon::Shuffle), tr("Shuffle"));
    shuffleAction_->setToolTip(tr("Toggle shuffle"));
    shuffleAction_->setCheckable(true);
    shuffleAction_->setChecked(manager_->shuffle());
    connect(shuffleAction_, &QAction::triggered, this, &PlaylistWidget::onShuffleToggle);

    repeatAction_ = controlBar_->addAction(pixelicons::icon(Icon::Repeat), tr("Repeat"));
    repeatAction_->setToolTip(tr("Cycle repeat mode (Off -> All -> One)"));
    connect(repeatAction_, &QAction::triggered, this, &PlaylistWidget::onRepeatCycle);
    updateRepeatButton();

    controlBar_->addSeparator();

    saveAction_ = controlBar_->addAction(pixelicons::icon(Icon::Save), tr("Save"));
    showTextBesideIcon(saveAction_);
    saveAction_->setToolTip(tr("Save playlist to file"));
    connect(saveAction_, &QAction::triggered, this, &PlaylistWidget::onSavePlaylist);

    loadAction_ = controlBar_->addAction(pixelicons::icon(Icon::Load), tr("Load"));
    showTextBesideIcon(loadAction_);
    loadAction_->setToolTip(tr("Load playlist from file"));
    connect(loadAction_, &QAction::triggered, this, &PlaylistWidget::onLoadPlaylist);

    controlBar_->addSeparator();

    clearAction_ = controlBar_->addAction(pixelicons::icon(Icon::Clear), tr("Clear"));
    showTextBesideIcon(clearAction_);
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
    treeWidget_->setIconSize(QSize(16, 16));
    treeWidget_->setColumnWidth(0, 24);  // Play marker
    treeWidget_->setColumnWidth(1, 30);  // Track number
    treeWidget_->setColumnWidth(3, 50);  // Length
    treeWidget_->header()->setStretchLastSection(false);
    treeWidget_->header()->setSectionResizeMode(2, QHeaderView::Stretch);  // Title stretches

    // Tracks can be dragged to reorder, and SID files dropped in from a device listing.
    // Every drop is answered as a copy: a drag that ends as a move has the view remove
    // the dragged row from its own items, but the service already moved the track and
    // the rebuilt list shows it in its new place.
    treeWidget_->setDragEnabled(true);
    treeWidget_->setAcceptDrops(true);
    treeWidget_->setDropIndicatorShown(true);
    treeWidget_->setDragDropMode(QAbstractItemView::DragDrop);
    treeWidget_->setDefaultDropAction(Qt::CopyAction);
    treeWidget_->viewport()->installEventFilter(this);

    connect(treeWidget_, &QTreeWidget::itemDoubleClicked, this,
            &PlaylistWidget::onItemDoubleClicked);
    connect(treeWidget_, &QTreeWidget::customContextMenuRequested, this,
            &PlaylistWidget::onContextMenu);

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
    connect(manager_, &PlaylistService::playlistChanged, this, &PlaylistWidget::onPlaylistChanged);
    connect(manager_, &PlaylistService::currentIndexChanged, this,
            &PlaylistWidget::onCurrentIndexChanged);
    connect(manager_, &PlaylistService::playbackStarted, this, &PlaylistWidget::onPlaybackStarted);
    connect(manager_, &PlaylistService::playbackStopped, this, &PlaylistWidget::onPlaybackStopped);
    connect(manager_, &PlaylistService::shuffleChanged, this, &PlaylistWidget::onShuffleChanged);
    connect(manager_, &PlaylistService::repeatModeChanged, this,
            &PlaylistWidget::onRepeatModeChanged);
    connect(manager_, &PlaylistService::statusMessage, this, &PlaylistWidget::statusMessage);

    // Duration spinner
    connect(durationSpinBox_, QOverload<int>::of(&QSpinBox::valueChanged), this,
            &PlaylistWidget::onDurationChanged);
}

bool PlaylistWidget::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != treeWidget_->viewport()) {
        return QWidget::eventFilter(watched, event);
    }

    switch (event->type()) {
    case QEvent::DragEnter:
    case QEvent::DragMove: {
        auto *dragEvent = static_cast<QDragMoveEvent *>(event);
        if (canAcceptDrag(dragEvent->mimeData(), dragEvent->source() == treeWidget_)) {
            dragEvent->setDropAction(Qt::CopyAction);
            dragEvent->accept();
            setDropActive(true);
        } else {
            dragEvent->ignore();
        }
        return true;
    }
    case QEvent::DragLeave:
        setDropActive(false);
        return true;
    case QEvent::Drop: {
        auto *dropEvent = static_cast<QDropEvent *>(event);
        setDropActive(false);
        if (handleDrop(dropEvent->mimeData(), dropEvent->position().toPoint(),
                       dropEvent->source() == treeWidget_)) {
            dropEvent->setDropAction(Qt::CopyAction);
            dropEvent->accept();
        } else {
            dropEvent->ignore();
        }
        return true;
    }
    default:
        return QWidget::eventFilter(watched, event);
    }
}

bool PlaylistWidget::canAcceptDrag(const QMimeData *mime, bool fromThisList) const
{
    if (fromThisList) {
        return treeWidget_->currentItem() != nullptr;
    }
    return mime && !sidPathsIn(*mime).isEmpty();
}

bool PlaylistWidget::handleDrop(const QMimeData *mime, const QPoint &viewportPos, bool fromThisList)
{
    if (fromThisList) {
        return moveDraggedTrack(viewportPos);
    }
    return mime && addDroppedTracks(*mime);
}

QStringList PlaylistWidget::sidPathsIn(const QMimeData &mime)
{
    const QString remoteType = QString::fromLatin1(dropcore::kRemotePathsMimeType);
    if (!mime.hasFormat(remoteType)) {
        return {};
    }

    QList<QPair<QString, filetype::FileType>> items;
    const QList<dropcore::DropEntry> entries = dropcore::decodeRemoteEntries(mime.data(remoteType));
    for (const dropcore::DropEntry &entry : entries) {
        items.append({entry.path, entry.isDirectory ? filetype::FileType::Directory
                                                    : filetype::detectFromFilename(
                                                          QFileInfo(entry.path).fileName())});
    }

    QStringList paths;
    const auto candidates = filebrowser::filterPlaylistCandidates(items);
    for (const auto &candidate : candidates) {
        paths.append(candidate.path);
    }
    return paths;
}

int PlaylistWidget::insertionRowAt(const QPoint &viewportPos) const
{
    QTreeWidgetItem *item = treeWidget_->itemAt(viewportPos);
    if (!item) {
        return treeWidget_->topLevelItemCount();
    }
    const int row = treeWidget_->indexOfTopLevelItem(item);
    const QRect rect = treeWidget_->visualItemRect(item);
    return viewportPos.y() >= rect.center().y() ? row + 1 : row;
}

bool PlaylistWidget::addDroppedTracks(const QMimeData &mime)
{
    const QStringList paths = sidPathsIn(mime);
    if (paths.isEmpty()) {
        emit statusMessage(tr("No SID music files in the drop"));
        return false;
    }
    for (const QString &path : paths) {
        manager_->addItem(path);
    }
    emit statusMessage(tr("Added %n track(s) to playlist", nullptr, paths.size()));
    return true;
}

bool PlaylistWidget::moveDraggedTrack(const QPoint &viewportPos)
{
    QTreeWidgetItem *dragged = treeWidget_->currentItem();
    if (!dragged) {
        return false;
    }
    const int from = treeWidget_->indexOfTopLevelItem(dragged);
    const int to = dropcore::reorderDestination(from, insertionRowAt(viewportPos));
    if (to == from) {
        return false;
    }
    manager_->moveItem(from, to);  // rebuilds the list through playlistChanged
    treeWidget_->setCurrentItem(treeWidget_->topLevelItem(to));
    return true;
}

void PlaylistWidget::setDropActive(bool active)
{
    if (treeWidget_->property("dropActive").toBool() == active) {
        return;
    }
    treeWidget_->setProperty("dropActive", active);
    treeWidget_->style()->unpolish(treeWidget_);
    treeWidget_->style()->polish(treeWidget_);
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
    setCollapsed(false);  // the playing track should be in view
    highlightCurrentItem();
    updateControlsState();

    // Start elapsed timer
    elapsedSeconds_ = 0;
    updateElapsedTimeDisplay();
    elapsedTimer_->start();
}

void PlaylistWidget::onPlaybackStopped()
{
    highlightCurrentItem();
    updateControlsState();

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
    // Just play - stop is a separate action
    manager_->play();
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
    case PlaylistService::RepeatMode::Off:
        manager_->setRepeatMode(PlaylistService::RepeatMode::All);
        break;
    case PlaylistService::RepeatMode::All:
        manager_->setRepeatMode(PlaylistService::RepeatMode::One);
        break;
    case PlaylistService::RepeatMode::One:
        manager_->setRepeatMode(PlaylistService::RepeatMode::Off);
        break;
    }
}

void PlaylistWidget::setMessagePresenter(IMessagePresenter *presenter)
{
    presenter_ = presenter ? presenter : &defaultPresenter_;
}

void PlaylistWidget::onClear()
{
    if (manager_->count() == 0) {
        return;
    }

    const QList<IMessagePresenter::DialogButton> buttons = {
        {tr("Clear"), IMessagePresenter::ButtonRole::Destructive},
        {tr("Cancel"), IMessagePresenter::ButtonRole::Reject},
    };
    constexpr int kCancelIndex = 1;  // Enter must never wipe the playlist
    const int result = presenter_->confirm(
        this, tr("Clear Playlist"),
        tr("Remove all %n track(s) from the playlist?", nullptr, manager_->count()), buttons,
        IMessagePresenter::MessageIcon::Warning, kCancelIndex);

    if (result == 0) {
        manager_->clear();
    }
}

void PlaylistWidget::onDurationChanged(int value)
{
    manager_->setDefaultDuration(value * 60);  // Convert minutes to seconds
}

void PlaylistWidget::onSavePlaylist()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath =
        QFileDialog::getSaveFileName(this, tr("Save Playlist"), defaultDir + "/playlist.json",
                                     tr("Playlist Files (*.json);;All Files (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    if (manager_->savePlaylist(filePath)) {
        emit statusMessage(tr("Playlist saved: %1").arg(filePath));
    } else {
        emit statusMessage(tr("Failed to save playlist"));
    }
}

void PlaylistWidget::onLoadPlaylist()
{
    QString defaultDir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    QString filePath = QFileDialog::getOpenFileName(this, tr("Load Playlist"), defaultDir,
                                                    tr("Playlist Files (*.json);;All Files (*)"));

    if (filePath.isEmpty()) {
        return;
    }

    if (manager_->loadPlaylist(filePath)) {
        emit statusMessage(tr("Playlist loaded: %1").arg(filePath));
    } else {
        emit statusMessage(tr("Failed to load playlist"));
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
        QString durationStr = playlist::formatDuration(item.durationSecs);

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
    updateHeaderText();
}

void PlaylistWidget::updateHeaderText()
{
    const int count = manager_->count();
    headerButton_->setText(count == 0 ? tr("Playlist") : tr("Playlist (%1 tracks)").arg(count));
}

void PlaylistWidget::setCollapsed(bool collapsed)
{
    if (collapsed_ == collapsed) {
        return;
    }
    collapsed_ = collapsed;

    headerButton_->setArrowType(collapsed ? Qt::RightArrow : Qt::DownArrow);
    durationRow_->setVisible(!collapsed);
    controlBar_->setVisible(!collapsed);
    treeWidget_->setVisible(!collapsed);

    // Collapsed, the widget is exactly its header row: a splitter cannot stretch it
    setMaximumHeight(collapsed ? headerButton_->sizeHint().height() : QWIDGETSIZE_MAX);
    updateGeometry();

    emit collapsedChanged(collapsed);
}

void PlaylistWidget::updateControlsState()
{
    bool hasItems = !manager_->isEmpty();
    bool isPlaying = manager_->isPlaying();

    // Play enabled when has items and not already playing
    playPauseAction_->setEnabled(hasItems && !isPlaying);
    // Stop enabled only when playing
    stopAction_->setEnabled(isPlaying);
    // Prev/next enabled when has items
    prevAction_->setEnabled(hasItems);
    nextAction_->setEnabled(hasItems);
    clearAction_->setEnabled(hasItems && !isPlaying);
    saveAction_->setEnabled(hasItems);
}

void PlaylistWidget::showTextBesideIcon(QAction *action)
{
    if (auto *button = qobject_cast<QToolButton *>(controlBar_->widgetForAction(action))) {
        button->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    }
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
    case PlaylistService::RepeatMode::Off:
        repeatAction_->setIcon(pixelicons::icon(pixeliconcore::Icon::Repeat));
        repeatAction_->setToolTip(tr("Repeat: OFF"));
        break;
    case PlaylistService::RepeatMode::All:
        repeatAction_->setIcon(pixelicons::icon(pixeliconcore::Icon::Repeat));
        repeatAction_->setToolTip(tr("Repeat: ALL"));
        break;
    case PlaylistService::RepeatMode::One:
        repeatAction_->setIcon(pixelicons::icon(pixeliconcore::Icon::RepeatOne));
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
            item->setIcon(0, isPlaying ? pixelicons::icon(pixeliconcore::Icon::PlaylistPlay)
                                       : QIcon());
        } else {
            font.setBold(false);
            item->setIcon(0, QIcon());
        }

        // Apply font to all columns
        for (int col = 0; col < 4; ++col) {
            item->setFont(col, font);
        }
    }
}

void PlaylistWidget::updateElapsedTimeDisplay()
{
    if (!manager_->isPlaying() || manager_->currentIndex() < 0) {
        elapsedTimeLabel_->setText(tr("--:-- / --:--"));
        return;
    }

    const auto &item = manager_->itemAt(manager_->currentIndex());
    elapsedTimeLabel_->setText(playlist::formatElapsed(elapsedSeconds_, item.durationSecs));
}

void PlaylistWidget::onElapsedTimerTick()
{
    elapsedSeconds_++;
    updateElapsedTimeDisplay();
}
