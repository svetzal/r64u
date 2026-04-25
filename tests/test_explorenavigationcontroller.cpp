#include "mocks/mocknavigationview.h"
#include "ui/explorenavigationcontroller.h"

#include <QSignalSpy>
#include <QtTest>

class TestExploreNavigationController : public QObject
{
    Q_OBJECT

private:
    ExploreNavigationController *ctrl_ = nullptr;
    MockNavigationView mock_;

private slots:
    void init()
    {
        mock_.pathHistory.clear();
        mock_.upEnabledHistory.clear();
        mock_.stubbedCurrentIndex = QModelIndex{};
        // No parent: cleanup() calls delete
        ctrl_ = new ExploreNavigationController(nullptr, nullptr, &mock_, nullptr);
    }

    void cleanup()
    {
        delete ctrl_;
        ctrl_ = nullptr;
    }

    // ======================================================================
    // Initial state
    // ======================================================================

    void testInitialDirectory_IsRoot() { QCOMPARE(ctrl_->currentDirectory(), QString("/")); }

    // ======================================================================
    // setCurrentDirectory
    // ======================================================================

    void testSetCurrentDirectory_UpdatesView()
    {
        ctrl_->setCurrentDirectory("/SD");

        QVERIFY(!mock_.pathHistory.isEmpty());
        QCOMPARE(mock_.pathHistory.last(), QString("/SD"));
    }

    void testSetCurrentDirectory_EmitsStatusMessage()
    {
        QSignalSpy spy(ctrl_, &ExploreNavigationController::statusMessage);

        ctrl_->setCurrentDirectory("/SD/games");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("/SD/games"));
    }

    void testSetCurrentDirectory_EnablesUpForSubPath()
    {
        ctrl_->setCurrentDirectory("/SD/games");

        QVERIFY(!mock_.upEnabledHistory.isEmpty());
        QVERIFY(mock_.upEnabledHistory.last() == true);
    }

    void testSetCurrentDirectory_DisablesUpAtRoot()
    {
        ctrl_->setCurrentDirectory("/");

        QVERIFY(!mock_.upEnabledHistory.isEmpty());
        QVERIFY(mock_.upEnabledHistory.last() == false);
    }

    void testSetCurrentDirectory_NullView_NoViewCallsButSignalEmitted()
    {
        // Controller with null view should still emit the signal
        auto *ctrlNoView = new ExploreNavigationController(nullptr, nullptr, nullptr, nullptr);
        QSignalSpy spy(ctrlNoView, &ExploreNavigationController::statusMessage);

        ctrlNoView->setCurrentDirectory("/SD");

        QCOMPARE(spy.count(), 1);
        delete ctrlNoView;
    }

    // ======================================================================
    // currentDirectory
    // ======================================================================

    void testCurrentDirectory_ReturnsLastSet()
    {
        ctrl_->setCurrentDirectory("/SD/music");

        QCOMPARE(ctrl_->currentDirectory(), QString("/SD/music"));
    }

    // ======================================================================
    // navigateToParent
    // ======================================================================

    void testNavigateToParent_FromDeepPath()
    {
        ctrl_->setCurrentDirectory("/SD/games/action");
        mock_.pathHistory.clear();

        ctrl_->navigateToParent();

        QVERIFY(!mock_.pathHistory.isEmpty());
        QCOMPARE(mock_.pathHistory.last(), QString("/SD/games"));
    }

    void testNavigateToParent_FromShallowPath_NavigatesToRoot()
    {
        ctrl_->setCurrentDirectory("/SD");
        mock_.pathHistory.clear();

        ctrl_->navigateToParent();

        QVERIFY(!mock_.pathHistory.isEmpty());
        QCOMPARE(mock_.pathHistory.last(), QString("/"));
    }

    void testNavigateToParent_AtRoot_NoOp()
    {
        ctrl_->setCurrentDirectory("/");
        mock_.pathHistory.clear();

        ctrl_->navigateToParent();

        // No further navigation call should be made
        QVERIFY(mock_.pathHistory.isEmpty());
        QCOMPARE(ctrl_->currentDirectory(), QString("/"));
    }

    void testNavigateToParent_Empty_NoOp()
    {
        ctrl_->setCurrentDirectory("");
        mock_.pathHistory.clear();

        ctrl_->navigateToParent();

        QVERIFY(mock_.pathHistory.isEmpty());
    }

    // ======================================================================
    // refresh — null device connection guard
    // ======================================================================

    void testRefresh_NullDeviceConnection_NoOp()
    {
        // With null DeviceConnection, refresh() should return early without crashing
        ctrl_->setCurrentDirectory("/SD");
        mock_.pathHistory.clear();

        ctrl_->refresh();  // Must not crash

        // No additional path changes expected
        QVERIFY(mock_.pathHistory.isEmpty());
    }

    // ======================================================================
    // refreshIfStale — null device connection guard
    // ======================================================================

    void testRefreshIfStale_NullDeviceConnection_NoOp()
    {
        ctrl_->setCurrentDirectory("/SD");
        mock_.pathHistory.clear();

        ctrl_->refreshIfStale();  // Must not crash

        QVERIFY(mock_.pathHistory.isEmpty());
    }
};

QTEST_MAIN(TestExploreNavigationController)
#include "test_explorenavigationcontroller.moc"
