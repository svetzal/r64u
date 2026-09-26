/**
 * @file test_playlistwidget.cpp
 * @brief Unit tests for PlaylistWidget control-flow and signal routing.
 *
 * PlaylistWidget delegates most business logic to PlaylistService (which has
 * its own test suite). These tests focus on the control-flow decisions that
 * live inside the widget itself:
 *
 * - Repeat-mode cycling (Off -> All -> One -> Off)
 * - Shuffle toggling
 * - Duration-spinner changes propagated to PlaylistService
 * - statusMessage signal forwarded from PlaylistService to the widget
 * - Playback-control slots delegate to PlaylistService
 * - Elapsed-timer starts on playbackStarted, stops on playbackStopped
 * - Drops: SID files from a device listing are added, other files ignored,
 *   and a track dragged within the list is moved
 *
 * PlaylistService is constructed with a null DeviceConnectionManager.  All guard
 * clauses for null connections are in PlaylistService and prevent any real
 * hardware calls from being made during the tests.
 */

#include "core/dropcore.h"
#include "mocks/mockmessagepresenter.h"
#include "services/playlistservice.h"
#include "ui/playlistwidget.h"

#include <QDropEvent>
#include <QMimeData>
#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTimer>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QtTest>

#include <memory>

class TestPlaylistWidget : public QObject
{
    Q_OBJECT

private:
    PlaylistService *manager = nullptr;
    PlaylistWidget *widget = nullptr;

    static void addTestItem(PlaylistService *m, const QString &path = "/SD/test.sid",
                            int durationSecs = 60)
    {
        playlist::PlaylistItem item;
        item.path = path;
        item.title = "Test";
        item.subsong = 1;
        item.durationSecs = durationSecs;
        m->addItem(item);
    }

    QStringList itemPaths() const
    {
        QStringList paths;
        for (const auto &item : manager->items()) {
            paths.append(item.path);
        }
        return paths;
    }

    /// A payload as RemoteFileModel writes it for the given device paths (files).
    static QMimeData *remotePathsMime(const QStringList &paths)
    {
        QList<dropcore::DropEntry> entries;
        for (const QString &path : paths) {
            entries.append({path, false, 0});
        }
        auto *mime = new QMimeData();
        mime->setData(QString::fromLatin1(dropcore::kRemotePathsMimeType),
                      dropcore::encodeRemoteEntries(entries));
        return mime;
    }

    /// Delivers a drag of @p mime entering the track list and dropping on it, as Qt would.
    void dropOntoList(QMimeData *mime)
    {
        auto *tree = widget->findChild<QTreeWidget *>();
        QVERIFY(tree != nullptr);
        QDragEnterEvent enter(QPoint(5, 5), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &enter);
        QDropEvent drop(QPoint(5, 5), Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &drop);
    }

    /// Shows the list with three tracks a, b, c and returns it.
    QTreeWidget *showListOfThree()
    {
        addTestItem(manager, "/SD/a.sid");
        addTestItem(manager, "/SD/b.sid");
        addTestItem(manager, "/SD/c.sid");
        widget->resize(400, 300);
        widget->show();
        auto *tree = widget->findChild<QTreeWidget *>();
        return tree;
    }

    /// A point just inside the top or bottom edge of @p row in the list.
    static QPoint edgeOfRow(QTreeWidget *tree, int row, bool bottom)
    {
        const QRect rect = tree->visualItemRect(tree->topLevelItem(row));
        return QPoint(rect.center().x(), bottom ? rect.bottom() - 1 : rect.top() + 1);
    }

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_playlistwidget");

        // Clear any leftover settings from previous runs
        QSettings settings;
        settings.remove("playlist");

        manager = new PlaylistService(nullptr);
        widget = new PlaylistWidget(manager);
    }

    void cleanup()
    {
        // Delete widget before manager: widget is connected to manager's signals
        delete widget;
        widget = nullptr;
        delete manager;
        manager = nullptr;

        QSettings settings;
        settings.remove("playlist");
    }

    // -----------------------------------------------------------------------
    // Construction
    // -----------------------------------------------------------------------

    void testConstruct_doesNotCrash() { QVERIFY(widget != nullptr); }

    // -----------------------------------------------------------------------
    // statusMessage forwarding
    // -----------------------------------------------------------------------

    void testStatusMessage_forwardedFromManager()
    {
        QSignalSpy spy(widget, &PlaylistWidget::statusMessage);

        // clear() on an already-empty playlist causes PlaylistService to emit statusMessage
        manager->clear();

        QCOMPARE(spy.count(), 1);
        QVERIFY(!spy.at(0).at(0).toString().isEmpty());
    }

    // -----------------------------------------------------------------------
    // Repeat-mode cycling (widget-side logic)
    // -----------------------------------------------------------------------

    void testOnRepeatCycle_cyclesOffToAll()
    {
        QCOMPARE(manager->repeatMode(), PlaylistService::RepeatMode::Off);

        QMetaObject::invokeMethod(widget, "onRepeatCycle");

        QCOMPARE(manager->repeatMode(), PlaylistService::RepeatMode::All);
    }

    void testOnRepeatCycle_cyclesAllToOne()
    {
        manager->setRepeatMode(PlaylistService::RepeatMode::All);

        QMetaObject::invokeMethod(widget, "onRepeatCycle");

        QCOMPARE(manager->repeatMode(), PlaylistService::RepeatMode::One);
    }

    void testOnRepeatCycle_cyclesOneToOff()
    {
        manager->setRepeatMode(PlaylistService::RepeatMode::One);

        QMetaObject::invokeMethod(widget, "onRepeatCycle");

        QCOMPARE(manager->repeatMode(), PlaylistService::RepeatMode::Off);
    }

    void testOnRepeatCycle_fullCycleReturnsToOff()
    {
        QCOMPARE(manager->repeatMode(), PlaylistService::RepeatMode::Off);

        QMetaObject::invokeMethod(widget, "onRepeatCycle");
        QMetaObject::invokeMethod(widget, "onRepeatCycle");
        QMetaObject::invokeMethod(widget, "onRepeatCycle");

        QCOMPARE(manager->repeatMode(), PlaylistService::RepeatMode::Off);
    }

    // -----------------------------------------------------------------------
    // Shuffle toggling (widget-side logic)
    // -----------------------------------------------------------------------

    void testOnShuffleToggle_enablesShuffle()
    {
        QVERIFY(!manager->shuffle());

        QMetaObject::invokeMethod(widget, "onShuffleToggle");

        QVERIFY(manager->shuffle());
    }

    void testOnShuffleToggle_disablesShuffle()
    {
        manager->setShuffle(true);

        QMetaObject::invokeMethod(widget, "onShuffleToggle");

        QVERIFY(!manager->shuffle());
    }

    // -----------------------------------------------------------------------
    // Duration spinner — widget to manager propagation
    // -----------------------------------------------------------------------

    void testOnDurationChanged_updatesManagerDuration()
    {
        auto *spinBox = widget->findChild<QSpinBox *>();
        QVERIFY(spinBox != nullptr);

        spinBox->setValue(5);  // 5 minutes

        QCOMPARE(manager->defaultDuration(), 300);  // 5 * 60 seconds
    }

    // -----------------------------------------------------------------------
    // Playback-control delegation
    // -----------------------------------------------------------------------

    void testOnPlayPause_delegatesPlayToManager()
    {
        addTestItem(manager);

        QSignalSpy spy(manager, &PlaylistService::playbackStarted);

        QMetaObject::invokeMethod(widget, "onPlayPause");

        QCOMPARE(spy.count(), 1);
    }

    void testOnStop_delegatesStopToManager()
    {
        addTestItem(manager);
        manager->play(0);

        QSignalSpy spy(manager, &PlaylistService::playbackStopped);

        QMetaObject::invokeMethod(widget, "onStop");

        QCOMPARE(spy.count(), 1);
    }

    void testOnNext_whenEmpty_doesNotCrash()
    {
        // next() on an empty playlist reports error via errorReported (to ErrorHandler),
        // not via statusMessage on the widget
        QSignalSpy statusSpy(widget, &PlaylistWidget::statusMessage);

        QMetaObject::invokeMethod(widget, "onNext");

        // No statusMessage emitted — errors now route through IErrorEmitter chain
        QCOMPARE(statusSpy.count(), 0);
    }

    void testOnPrevious_whenEmpty_doesNotCrash()
    {
        QSignalSpy statusSpy(widget, &PlaylistWidget::statusMessage);

        QMetaObject::invokeMethod(widget, "onPrevious");

        // No statusMessage emitted — errors now route through IErrorEmitter chain
        QCOMPARE(statusSpy.count(), 0);
    }

    // -----------------------------------------------------------------------
    // Elapsed timer lifecycle
    // -----------------------------------------------------------------------

    void testOnPlaybackStarted_startsElapsedTimer()
    {
        addTestItem(manager);

        // No timer should be running before playback
        const auto timers = widget->findChildren<QTimer *>();
        bool wasActive =
            std::any_of(timers.begin(), timers.end(), [](QTimer *t) { return t->isActive(); });
        QVERIFY(!wasActive);

        // play() emits playbackStarted → widget's onPlaybackStarted starts the timer
        manager->play(0);

        bool isNowActive =
            std::any_of(timers.begin(), timers.end(), [](QTimer *t) { return t->isActive(); });
        QVERIFY(isNowActive);
    }

    void testOnPlaybackStopped_stopsElapsedTimer()
    {
        addTestItem(manager);
        manager->play(0);  // starts the elapsed timer

        // Verify the timer is running before stop
        const auto timers = widget->findChildren<QTimer *>();
        bool wasActive =
            std::any_of(timers.begin(), timers.end(), [](QTimer *t) { return t->isActive(); });
        QVERIFY(wasActive);

        // stop() emits playbackStopped → widget's onPlaybackStopped stops the timer
        manager->stop();

        bool stillActive =
            std::any_of(timers.begin(), timers.end(), [](QTimer *t) { return t->isActive(); });
        QVERIFY(!stillActive);
    }

    // -----------------------------------------------------------------------
    // Collapsing — the header row stays, everything under it folds away
    // -----------------------------------------------------------------------

    void testCollapse_hidesControlsAndListAndShrinksToTheHeader()
    {
        widget->show();
        QVERIFY(QTest::qWaitForWindowExposed(widget));
        auto *header = widget->findChild<QToolButton *>(QStringLiteral("PlaylistHeader"));
        auto *tree = widget->findChild<QTreeWidget *>();
        auto *bar = widget->findChild<QToolBar *>();
        QVERIFY(header && tree && bar);
        QVERIFY(!widget->isCollapsed());
        QCOMPARE(header->arrowType(), Qt::DownArrow);
        QSignalSpy spy(widget, &PlaylistWidget::collapsedChanged);

        widget->setCollapsed(true);

        QVERIFY(widget->isCollapsed());
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toBool(), true);
        QCOMPARE(header->arrowType(), Qt::RightArrow);
        QVERIFY(tree->isHidden());
        QVERIFY(bar->isHidden());
        QVERIFY(!header->isHidden());
        QCOMPARE(widget->maximumHeight(), header->sizeHint().height());
        QVERIFY(widget->sizeHint().height() <= header->sizeHint().height());
    }

    void testCollapse_headerClickToggles_andExpandRestoresTheList()
    {
        auto *header = widget->findChild<QToolButton *>(QStringLiteral("PlaylistHeader"));
        auto *tree = widget->findChild<QTreeWidget *>();
        QVERIFY(header && tree);
        widget->show();

        header->click();
        QVERIFY(widget->isCollapsed());

        header->click();
        QVERIFY(!widget->isCollapsed());
        QVERIFY(!tree->isHidden());
        QCOMPARE(widget->maximumHeight(), QWIDGETSIZE_MAX);
    }

    void testCollapse_sameStateTwice_emitsNothing()
    {
        QSignalSpy spy(widget, &PlaylistWidget::collapsedChanged);
        widget->setCollapsed(false);
        QCOMPARE(spy.count(), 0);
    }

    void testCollapse_headerKeepsTheTrackCount()
    {
        addTestItem(manager);
        addTestItem(manager, "/SD/other.sid");
        auto *header = widget->findChild<QToolButton *>(QStringLiteral("PlaylistHeader"));
        QVERIFY(header != nullptr);

        widget->setCollapsed(true);

        QCOMPARE(header->text(), QStringLiteral("Playlist (2 tracks)"));
    }

    void testPlaybackStarted_expandsACollapsedPlaylist()
    {
        addTestItem(manager);
        widget->setCollapsed(true);

        manager->play(0);

        QVERIFY(!widget->isCollapsed());
    }

    // -----------------------------------------------------------------------
    // Clear playlist
    // -----------------------------------------------------------------------

    void testOnClear_whenConfirmed_clearsManager()
    {
        addTestItem(manager);
        QCOMPARE(manager->count(), 1);
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;  // index 0 = "Clear"
        widget->setMessagePresenter(&mock);

        QMetaObject::invokeMethod(widget, "onClear");

        QCOMPARE(mock.confirmCalls.size(), 1);
        QCOMPARE(manager->count(), 0);
    }

    void testOnClear_whenCancelled_keepsPlaylist()
    {
        addTestItem(manager);
        MockMessagePresenter mock;
        mock.nextConfirmResult = 1;  // index 1 = "Cancel"
        widget->setMessagePresenter(&mock);

        QMetaObject::invokeMethod(widget, "onClear");

        QCOMPARE(mock.confirmCalls.size(), 1);
        QCOMPARE(manager->count(), 1);
    }

    void testOnClear_whenDismissed_keepsPlaylist()
    {
        addTestItem(manager);
        MockMessagePresenter mock;
        mock.nextConfirmResult = -1;
        widget->setMessagePresenter(&mock);

        QMetaObject::invokeMethod(widget, "onClear");

        QCOMPARE(manager->count(), 1);
    }

    void testOnClear_defaultButtonIsCancel()
    {
        addTestItem(manager);
        MockMessagePresenter mock;
        mock.nextConfirmResult = -1;
        widget->setMessagePresenter(&mock);

        QMetaObject::invokeMethod(widget, "onClear");

        QCOMPARE(mock.confirmCalls.size(), 1);
        const ConfirmCall &call = mock.confirmCalls[0];
        QVERIFY(call.defaultIndex >= 0 && call.defaultIndex < call.buttons.size());
        QCOMPARE(call.buttons[call.defaultIndex].role, IMessagePresenter::ButtonRole::Reject);
    }

    void testOnClear_whenEmpty_doesNotPrompt()
    {
        MockMessagePresenter mock;
        widget->setMessagePresenter(&mock);

        QMetaObject::invokeMethod(widget, "onClear");

        QCOMPARE(mock.confirmCalls.size(), 0);
    }

    // -----------------------------------------------------------------------
    // Drops from a device listing
    // -----------------------------------------------------------------------

    void testDrop_remoteSidPath_addsATrack()
    {
        QMimeData *mime = remotePathsMime({"/SD/Music/tune.sid"});

        dropOntoList(mime);

        QCOMPARE(manager->count(), 1);
        QCOMPARE(manager->itemAt(0).path, QString("/SD/Music/tune.sid"));
        delete mime;
    }

    void testDrop_remotePrgPath_addsNothing()
    {
        QMimeData *mime = remotePathsMime({"/SD/Games/game.prg"});

        dropOntoList(mime);

        QCOMPARE(manager->count(), 0);
        delete mime;
    }

    void testHandleDrop_withoutASidFile_refusesAndSaysSo()
    {
        QSignalSpy status(widget, &PlaylistWidget::statusMessage);
        std::unique_ptr<QMimeData> mime(remotePathsMime({"/SD/Games/game.prg"}));

        QVERIFY(!widget->handleDrop(mime.get(), QPoint(5, 5), false));

        QCOMPARE(manager->count(), 0);
        QCOMPARE(status.count(), 1);
        QVERIFY(status.at(0).at(0).toString().contains("No SID"));
    }

    void testDrop_mixedPaths_addsOnlyTheSidFiles()
    {
        QMimeData *mime =
            remotePathsMime({"/SD/a.prg", "/SD/one.sid", "/SD/disk.d64", "/SD/two.SID"});

        dropOntoList(mime);

        QCOMPARE(itemPaths(), QStringList({"/SD/one.sid", "/SD/two.SID"}));
        delete mime;
    }

    void testDrag_isAcceptedOnlyWhenItCarriesASidFile()
    {
        std::unique_ptr<QMimeData> sid(remotePathsMime({"/SD/one.sid"}));
        std::unique_ptr<QMimeData> prg(remotePathsMime({"/SD/game.prg"}));
        QMimeData files;
        files.setUrls({QUrl::fromLocalFile("/Users/someone/one.sid")});

        QVERIFY(widget->canAcceptDrag(sid.get(), false));
        QVERIFY(!widget->canAcceptDrag(prg.get(), false));
        QVERIFY(!widget->canAcceptDrag(&files, false));
        QVERIFY(!widget->canAcceptDrag(nullptr, false));
    }

    // -----------------------------------------------------------------------
    // Reordering by dragging within the list
    // -----------------------------------------------------------------------

    void testInternalDrop_belowALaterRow_movesTheTrackThere()
    {
        QTreeWidget *tree = showListOfThree();
        QVERIFY(tree != nullptr);
        tree->setCurrentItem(tree->topLevelItem(0));

        QVERIFY(widget->handleDrop(nullptr, edgeOfRow(tree, 2, true), true));

        QCOMPARE(itemPaths(), QStringList({"/SD/b.sid", "/SD/c.sid", "/SD/a.sid"}));
        QCOMPARE(tree->indexOfTopLevelItem(tree->currentItem()), 2);
    }

    void testInternalDrop_aboveAnEarlierRow_movesTheTrackThere()
    {
        QTreeWidget *tree = showListOfThree();
        QVERIFY(tree != nullptr);
        tree->setCurrentItem(tree->topLevelItem(2));

        QVERIFY(widget->handleDrop(nullptr, edgeOfRow(tree, 0, false), true));

        QCOMPARE(itemPaths(), QStringList({"/SD/c.sid", "/SD/a.sid", "/SD/b.sid"}));
        QCOMPARE(tree->indexOfTopLevelItem(tree->currentItem()), 0);
    }

    void testInternalDrop_belowTheLastRow_movesTheTrackToTheEnd()
    {
        QTreeWidget *tree = showListOfThree();
        QVERIFY(tree != nullptr);
        tree->setCurrentItem(tree->topLevelItem(0));
        const QPoint belowAll(10, tree->visualItemRect(tree->topLevelItem(2)).bottom() + 20);

        QVERIFY(widget->handleDrop(nullptr, belowAll, true));

        QCOMPARE(itemPaths(), QStringList({"/SD/b.sid", "/SD/c.sid", "/SD/a.sid"}));
    }

    void testInternalDrop_ontoItsOwnPlace_changesNothing()
    {
        QTreeWidget *tree = showListOfThree();
        QVERIFY(tree != nullptr);
        tree->setCurrentItem(tree->topLevelItem(1));
        QSignalSpy changed(manager, &PlaylistService::playlistChanged);

        QVERIFY(!widget->handleDrop(nullptr, edgeOfRow(tree, 1, true), true));

        QCOMPARE(changed.count(), 0);
        QCOMPARE(itemPaths(), QStringList({"/SD/a.sid", "/SD/b.sid", "/SD/c.sid"}));
    }

    void testInternalDrop_withNothingCurrent_isRefused()
    {
        QTreeWidget *tree = showListOfThree();
        QVERIFY(tree != nullptr);
        tree->setCurrentItem(nullptr);

        QVERIFY(!widget->canAcceptDrag(nullptr, true));
        QVERIFY(!widget->handleDrop(nullptr, QPoint(5, 5), true));
    }
};

QTEST_MAIN(TestPlaylistWidget)
#include "test_playlistwidget.moc"
