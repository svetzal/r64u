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
 *
 * PlaylistService is constructed with a null DeviceConnectionManager.  All guard
 * clauses for null connections are in PlaylistService and prevent any real
 * hardware calls from being made during the tests.
 */

#include "services/playlistservice.h"
#include "ui/playlistwidget.h"

#include <QSettings>
#include <QSignalSpy>
#include <QSpinBox>
#include <QTimer>
#include <QtTest>

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

        // play() on an empty playlist causes PlaylistService to emit statusMessage
        manager->play();

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

    void testOnNext_whenEmpty_emitsStatusMessage()
    {
        // next() on an empty playlist emits a statusMessage via manager
        QSignalSpy spy(widget, &PlaylistWidget::statusMessage);

        QMetaObject::invokeMethod(widget, "onNext");

        QCOMPARE(spy.count(), 1);
    }

    void testOnPrevious_whenEmpty_emitsStatusMessage()
    {
        QSignalSpy spy(widget, &PlaylistWidget::statusMessage);

        QMetaObject::invokeMethod(widget, "onPrevious");

        QCOMPARE(spy.count(), 1);
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
    // Clear playlist
    // -----------------------------------------------------------------------

    void testOnClear_delegatesClearToManager()
    {
        addTestItem(manager);
        QCOMPARE(manager->count(), 1);

        QMetaObject::invokeMethod(widget, "onClear");

        QCOMPARE(manager->count(), 0);
    }
};

QTEST_MAIN(TestPlaylistWidget)
#include "test_playlistwidget.moc"
