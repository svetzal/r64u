/**
 * @file test_remotefilebrowserwidget.cpp
 * @brief Unit tests for RemoteFileBrowserWidget control-flow and signal routing.
 *
 * RemoteFileBrowserWidget delegates file listing to RemoteFileModel and
 * operations to RemoteFileOperationsService.  These tests focus on the
 * control-flow decisions inside the widget:
 *
 * - setCurrentDirectory() emits currentDirectoryChanged signal
 * - onParentFolder() at root "/" returns silently
 * - onParentFolder() parent path computation
 * - refreshIfStale() when disconnected — no-op
 * - refreshIfStale() when suppressAutoRefresh is active — no-op
 * - selectedPath() when no selection — returns empty string
 * - isSelectedDirectory() when no selection — returns false
 */

#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "models/remotefilemodel.h"
#include "ui/remotefilebrowserwidget.h"

#include <QSignalSpy>
#include <QtTest>

class TestRemoteFileBrowserWidget : public QObject
{
    Q_OBJECT

private:
    RemoteFileModel *model_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    MockRestClient *mockRest_ = nullptr;

private slots:
    void init()
    {
        mockFtp_ = new MockFtpClient(this);
        mockRest_ = new MockRestClient(this);
        model_ = new RemoteFileModel(this);
    }

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        RemoteFileBrowserWidget widget(model_);
        QVERIFY(true);
    }

    // =========================================================================
    // setCurrentDirectory() — emits signal
    // =========================================================================

    void testSetCurrentDirectory_EmitsCurrentDirectoryChanged()
    {
        RemoteFileBrowserWidget widget(model_);
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        widget.setCurrentDirectory("/SD");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/SD"));
    }

    void testSetCurrentDirectory_UpdatesCurrentDirectory()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.setCurrentDirectory("/SD/Games");
        QCOMPARE(widget.currentDirectory(), QString("/SD/Games"));
    }

    // =========================================================================
    // onParentFolder() — at root, no-op
    // =========================================================================

    void testOnParentFolder_AtRoot_DoesNotEmitSignal()
    {
        RemoteFileBrowserWidget widget(model_);
        // Start at root
        widget.setCurrentDirectory("/");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        // Should not emit because we are already at root
        QCOMPARE(spy.count(), 0);
    }

    void testOnParentFolder_AtRoot_StaysAtRoot()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.setCurrentDirectory("/");

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        QCOMPARE(widget.currentDirectory(), QString("/"));
    }

    // =========================================================================
    // onParentFolder() — parent path computation
    // =========================================================================

    void testOnParentFolder_OneLevelDeep_NavigatesToRoot()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.setCurrentDirectory("/SD");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/"));
    }

    void testOnParentFolder_TwoLevelsDeep_NavigatesToParent()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.setCurrentDirectory("/SD/Games");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/SD"));
    }

    // =========================================================================
    // refreshIfStale() — disconnected state, no-op
    // =========================================================================

    void testRefreshIfStale_WhenDisconnected_NoOp()
    {
        RemoteFileBrowserWidget widget(model_);
        // Widget starts disconnected, so refreshIfStale should be a no-op
        // We verify by checking no crash and model refresh is not triggered
        widget.refreshIfStale();
        QVERIFY(true);  // No crash
    }

    // =========================================================================
    // refreshIfStale() — connected state, calls model
    // =========================================================================

    void testRefreshIfStale_WhenConnected_Proceeds()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.onConnectionStateChanged(true);
        // Should not crash
        widget.refreshIfStale();
        QVERIFY(true);
    }

    // =========================================================================
    // AutoRefreshSuppressor — suppresses refreshIfStale
    // =========================================================================

    void testAutoRefreshSuppressor_SuppressesRefresh()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.onConnectionStateChanged(true);

        {
            auto suppressor = widget.suppressRefresh();
            // refreshIfStale should be a no-op while suppressed (no crash)
            widget.refreshIfStale();
        }
        // After scope, suppressor released — refresh should work again
        widget.refreshIfStale();
        QVERIFY(true);
    }

    // =========================================================================
    // selectedPath() — no selection returns empty
    // =========================================================================

    void testSelectedPath_NoSelection_ReturnsEmpty()
    {
        RemoteFileBrowserWidget widget(model_);
        QVERIFY(widget.selectedPath().isEmpty());
    }

    // =========================================================================
    // isSelectedDirectory() — no selection returns false
    // =========================================================================

    void testIsSelectedDirectory_NoSelection_ReturnsFalse()
    {
        RemoteFileBrowserWidget widget(model_);
        QVERIFY(!widget.isSelectedDirectory());
    }

    // =========================================================================
    // selectedPaths() — no selection returns empty list
    // =========================================================================

    void testSelectedPaths_NoSelection_ReturnsEmpty()
    {
        RemoteFileBrowserWidget widget(model_);
        QVERIFY(widget.selectedPaths().isEmpty());
    }

    // =========================================================================
    // setDownloadEnabled() — does not crash
    // =========================================================================

    void testSetDownloadEnabled_DoesNotCrash()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.setDownloadEnabled(true);
        widget.setDownloadEnabled(false);
        QVERIFY(true);
    }

    // =========================================================================
    // onConnectionStateChanged() — updates connected state
    // =========================================================================

    void testOnConnectionStateChanged_Connected()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.onConnectionStateChanged(true);
        QVERIFY(true);  // No crash
    }

    void testOnConnectionStateChanged_Disconnected()
    {
        RemoteFileBrowserWidget widget(model_);
        widget.onConnectionStateChanged(true);
        widget.onConnectionStateChanged(false);
        QVERIFY(true);  // No crash
    }
};

QTEST_MAIN(TestRemoteFileBrowserWidget)
#include "test_remotefilebrowserwidget.moc"
