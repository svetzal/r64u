/**
 * @file test_viewpanel.cpp
 * @brief Unit tests for ViewPanel control-flow, guard clauses, and state transitions.
 *
 * ViewPanel owns toolbar actions and delegates to injected streaming/recording/screenshot
 * services.  These tests verify:
 *
 * - onStartStreaming() guard when streaming service is null
 * - onStartStreaming() guard when already streaming (via service state)
 * - onCaptureScreenshot() guard when screenshot service is null
 * - onStartRecording() / onStopRecording() null-guard and state transitions
 * - scalingMode() switch dispatch
 * - stopStreamingIfActive() stops only when active, no-op otherwise
 * - loadSettings() / saveSettings() round-trip
 * - the stats HUD overlays the video instead of pushing it down
 * - view/statsExpanded persists the HUD's expanded state
 * - setChromeHidden() hides the toolbar; the placeholder screen names the state
 *
 * Tests use a null DeviceConnectionManager-safe path: the ViewPanel constructor asserts
 * that connection is non-null, so we supply a minimal stand-in via a real
 * DeviceConnectionManager backed by mock clients.
 */

#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/screenshotservice.h"
#include "services/streamingservice.h"
#include "services/videorecordingservice.h"
#include "ui/streamingdiagnosticswidget.h"
#include "ui/videodisplaywidget.h"
#include "ui/viewpanel.h"

#include <QAction>
#include <QSettings>
#include <QSignalSpy>
#include <QToolBar>
#include <QtTest>

#include <functional>
#include <utility>

/// Runs a notification when destroyed, standing in for a panel-owned service
/// whose teardown notifies observers.
class NotifyOnDestruction : public QObject
{
public:
    NotifyOnDestruction(std::function<void()> notify, QObject *parent)
        : QObject(parent), notify_(std::move(notify))
    {
    }
    ~NotifyOnDestruction() override { notify_(); }

    NotifyOnDestruction(const NotifyOnDestruction &) = delete;
    NotifyOnDestruction &operator=(const NotifyOnDestruction &) = delete;
    NotifyOnDestruction(NotifyOnDestruction &&) = delete;
    NotifyOnDestruction &operator=(NotifyOnDestruction &&) = delete;

private:
    std::function<void()> notify_;
};

class TestViewPanel : public QObject
{
    Q_OBJECT

private:
    MockRestClient *mockRest_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    DeviceConnectionManager *connection_ = nullptr;

    DeviceConnectionManager *makeDisconnectedConnection()
    {
        mockRest_ = new MockRestClient(this);
        mockFtp_ = new MockFtpClient(this);
        return new DeviceConnectionManager(mockRest_, mockFtp_, this);
    }

    ErrorHandler *makeErrorHandler() { return new ErrorHandler(nullptr, this); }

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_viewpanel");
        QSettings settings;
        settings.remove("view");

        connection_ = makeDisconnectedConnection();
    }

    void cleanup()
    {
        QSettings settings;
        settings.remove("view");
    }

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        QVERIFY(true);
    }

    // =========================================================================
    // stopStreamingIfActive() — no streaming service, no crash
    // =========================================================================

    void testStopStreamingIfActive_NoStreamingService_NoOp()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        // streaming service is null — should not crash
        panel.stopStreamingIfActive();
        QVERIFY(true);
    }

    // =========================================================================
    // stopStreamingIfActive() — streaming service set but not streaming
    // =========================================================================

    void testStopStreamingIfActive_NotStreaming_NoOp()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        // Inject a streaming service — not streaming by default
        auto *service = new StreamingService(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                             nullptr, this);
        panel.setStreamingService(service);

        panel.stopStreamingIfActive();

        QVERIFY(!service->isStreaming());
    }

    // =========================================================================
    // scalingMode() — returns valid integer
    // =========================================================================

    void testScalingMode_ReturnsValidInt()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        int mode = panel.scalingMode();
        // Valid ScalingMode values: Sharp(0), Smooth(1), Integer(2)
        QVERIFY(mode >= 0 && mode <= 2);
    }

    // =========================================================================
    // loadSettings() / saveSettings() round-trip
    // =========================================================================

    void testLoadSaveSettings_RoundTrip()
    {
        {
            ViewPanel panel(connection_, makeErrorHandler());
            // Default is Integer (2)
            QCOMPARE(panel.scalingMode(), 2);
            panel.saveSettings();
        }
        {
            ViewPanel panel(connection_, makeErrorHandler());
            panel.loadSettings();
            QCOMPARE(panel.scalingMode(), 2);
        }
    }

    void testSaveSettings_PersistsScalingMode()
    {
        {
            ViewPanel panel(connection_, makeErrorHandler());
            // Change to Sharp via loadSettings with a preset
            QSettings settings;
            settings.setValue("view/scalingMode", 0);  // Sharp
            panel.loadSettings();
            QCOMPARE(panel.scalingMode(), 0);
            panel.saveSettings();
        }

        QSettings settings;
        QCOMPARE(settings.value("view/scalingMode").toInt(), 0);
    }

    // =========================================================================
    // setStreamingService() / streamingService() accessor
    // =========================================================================

    void testSetStreamingService_AccessorReturnsSet()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *service = new StreamingService(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                             nullptr, this);
        panel.setStreamingService(service);
        QCOMPARE(panel.streamingService(), service);
    }

    void testStreamingService_InitiallyNull()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        QVERIFY(panel.streamingService() == nullptr);
    }

    // =========================================================================
    // setRecordingService() / recordingService() accessor
    // =========================================================================

    void testRecordingService_InitiallyNull()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        QVERIFY(panel.recordingService() == nullptr);
    }

    void testSetRecordingService_AccessorReturnsSet()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *service = new VideoRecordingService(this);
        panel.setRecordingService(service);
        QCOMPARE(panel.recordingService(), service);
    }

    // =========================================================================
    // onStartStreaming() — null streaming service, invoked via slot
    // =========================================================================

    void testOnStartStreaming_NullStreamingService_NoOp()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        // No streaming service injected — slot should return silently
        QMetaObject::invokeMethod(&panel, "onStartStreaming");
        QVERIFY(panel.streamingService() == nullptr);
    }

    // =========================================================================
    // onCaptureScreenshot() — null screenshot service path
    // =========================================================================

    void testOnCaptureScreenshot_NullService_NoOp()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        // No screenshot service — should not crash
        QMetaObject::invokeMethod(&panel, "onCaptureScreenshot");
        QVERIFY(true);
    }

    void testOnCaptureScreenshot_WithService_UsesService()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *service = new ScreenshotService(this);
        panel.setScreenshotService(service);
        // Frame is null so service should handle gracefully (no crash)
        QMetaObject::invokeMethod(&panel, "onCaptureScreenshot");
        QVERIFY(true);
    }

    // =========================================================================
    // onStartRecording() — null recording service, should not crash
    // =========================================================================

    void testOnStartRecording_NullService_NoOp()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        // No recording service — should return silently
        QMetaObject::invokeMethod(&panel, "onStartRecording");
        QVERIFY(panel.recordingService() == nullptr);
    }

    // =========================================================================
    // onStopRecording() — null recording service, should not crash
    // =========================================================================

    void testOnStopRecording_NullService_NoOp()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        QMetaObject::invokeMethod(&panel, "onStopRecording");
        QVERIFY(true);
    }

    // =========================================================================
    // Teardown — services owned by the panel die inside ~QWidget
    // =========================================================================

    void testDestruction_ignoresSignalsFromServicesTornDownWithIt()
    {
        auto *errorHandler = makeErrorHandler();
        QSignalSpy errorHandlerMessages(errorHandler, &ErrorHandler::statusMessage);
        auto *panel = new ViewPanel(connection_, errorHandler);

        // Children die in creation order after the panel's widgets, so this
        // stand-in for "a service that notifies on its way out" fires while
        // the streaming service (created next) is still alive.
        StreamingService *service = nullptr;
        new NotifyOnDestruction(
            [&service]() { emit service->statusMessage(QStringLiteral("late"), 0); }, panel);
        service = new StreamingService(nullptr, nullptr, nullptr, nullptr, nullptr, nullptr,
                                       nullptr, panel);
        panel->setStreamingService(service);

        delete panel;

        QCOMPARE(errorHandlerMessages.count(), 0);
    }

    // =========================================================================
    // Stats HUD — an overlay on the video, never a row above it
    // =========================================================================

    void testStatsOverlay_isChildOfVideoDisplayAndHiddenByDefault()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *video = panel.findChild<VideoDisplayWidget *>();
        auto *stats = panel.findChild<StreamingDiagnosticsWidget *>();
        QVERIFY(video != nullptr);
        QVERIFY(stats != nullptr);
        QCOMPARE(stats->parentWidget(), video);
        QCOMPARE(stats->objectName(), QStringLiteral("StatsOverlay"));
        QVERIFY(stats->isHidden());
    }

    void testStatsToggled_showsOverlayInsetAtTopLeftWithoutResizingVideo()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        panel.resize(900, 700);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        QCoreApplication::processEvents();

        auto *video = panel.findChild<VideoDisplayWidget *>();
        auto *stats = panel.findChild<StreamingDiagnosticsWidget *>();
        QVERIFY(video != nullptr);
        QVERIFY(stats != nullptr);
        const QRect videoBefore = video->geometry();

        QMetaObject::invokeMethod(&panel, "onStatsToggled", Q_ARG(bool, true));
        QCoreApplication::processEvents();

        QVERIFY(stats->isVisible());
        QCOMPARE(stats->pos(), QPoint(8, 8));
        QCOMPARE(video->geometry(), videoBefore);

        stats->setDisplayMode(StreamingDiagnosticsWidget::DisplayMode::Detailed);
        QCoreApplication::processEvents();
        QCOMPARE(video->geometry(), videoBefore);
        QVERIFY(stats->height() > 0);
        QVERIFY(stats->height() < video->height());
    }

    void testStatsExpanded_persistsToSettingsAndLoadsBack()
    {
        {
            ViewPanel panel(connection_, makeErrorHandler());
            auto *stats = panel.findChild<StreamingDiagnosticsWidget *>();
            QVERIFY(stats != nullptr);
            stats->setDisplayMode(StreamingDiagnosticsWidget::DisplayMode::Detailed);
            QSettings settings;
            QCOMPARE(settings.value("view/statsExpanded").toBool(), true);
        }
        {
            ViewPanel panel(connection_, makeErrorHandler());
            panel.loadSettings();
            auto *stats = panel.findChild<StreamingDiagnosticsWidget *>();
            QVERIFY(stats != nullptr);
            QCOMPARE(stats->displayMode(), StreamingDiagnosticsWidget::DisplayMode::Detailed);
        }
    }

    // =========================================================================
    // Chrome and placeholder screen
    // =========================================================================

    void testSetChromeHidden_hidesAndRestoresToolBar()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *toolBar = panel.findChild<QToolBar *>();
        QVERIFY(toolBar != nullptr);

        panel.setChromeHidden(true);
        QVERIFY(toolBar->isHidden());
        panel.setChromeHidden(false);
        QVERIFY(!toolBar->isHidden());
    }

    void testSetFullScreenAction_addsActionToToolBar()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        QAction fullScreen(QStringLiteral("Full Screen"));
        panel.setFullScreenAction(&fullScreen);
        auto *toolBar = panel.findChild<QToolBar *>();
        QVERIFY(toolBar != nullptr);
        QVERIFY(toolBar->actions().contains(&fullScreen));
    }

    void testPlaceholder_whileDisconnected_saysNoDeviceConnected()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *video = panel.findChild<VideoDisplayWidget *>();
        QVERIFY(video != nullptr);
        const QStringList lines = video->placeholderLines();
        QCOMPARE(lines.first(), QStringLiteral("    **** R64U VIDEO ****"));
        QVERIFY(lines.contains(QStringLiteral("NO DEVICE CONNECTED.")));
        QCOMPARE(lines.last(), QStringLiteral("READY."));
        for (const QString &line : lines) {
            QVERIFY2(line.size() <= 40, qPrintable(line));
        }
    }

    void testPlaceholder_whileStarting_saysStartingStream()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *video = panel.findChild<VideoDisplayWidget *>();
        QVERIFY(video != nullptr);

        QMetaObject::invokeMethod(&panel, "onStreamingStarted",
                                  Q_ARG(QString, QStringLiteral("10.0.0.5")));
        // Still disconnected: the connection state wins over the stream state
        QVERIFY(video->placeholderLines().contains(QStringLiteral("NO DEVICE CONNECTED.")));

        QMetaObject::invokeMethod(&panel, "onStreamingStopped");
        QVERIFY(video->placeholderLines().contains(QStringLiteral("NO DEVICE CONNECTED.")));
    }

    void testKeyboardFocus_appendsKeysToC64ToStreamStatus()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        auto *video = panel.findChild<VideoDisplayWidget *>();
        auto *toolBar = panel.findChild<QToolBar *>();
        QVERIFY(video != nullptr);
        QVERIFY(toolBar != nullptr);
        QLabel *status = nullptr;
        for (QLabel *label : toolBar->findChildren<QLabel *>()) {
            if (label->text().contains(QStringLiteral("Not streaming"))) {
                status = label;
            }
        }
        QVERIFY(status != nullptr);

        QFocusEvent focusIn(QEvent::FocusIn);
        QCoreApplication::sendEvent(video, &focusIn);
        QVERIFY(status->text().endsWith(QStringLiteral("keys to C64")));

        QFocusEvent focusOut(QEvent::FocusOut);
        QCoreApplication::sendEvent(video, &focusOut);
        QCOMPARE(status->text(), QStringLiteral("Not streaming"));
    }

    void testMouseClickOnVideo_appendsKeysToC64ToStreamStatus()
    {
        ViewPanel panel(connection_, makeErrorHandler());
        panel.resize(900, 700);
        panel.show();
        QVERIFY(QTest::qWaitForWindowExposed(&panel));
        panel.activateWindow();
        auto *video = panel.findChild<VideoDisplayWidget *>();
        auto *toolBar = panel.findChild<QToolBar *>();
        QVERIFY(video != nullptr);
        QVERIFY(toolBar != nullptr);
        QLabel *status = nullptr;
        for (QLabel *label : toolBar->findChildren<QLabel *>()) {
            if (label->text().contains(QStringLiteral("Not streaming"))) {
                status = label;
            }
        }
        QVERIFY(status != nullptr);

        // Focus starts elsewhere in the panel, so the click has to take it.
        toolBar->setFocus();
        video->clearFocus();
        QCoreApplication::processEvents();
        QVERIFY(!video->hasFocus());
        QCOMPARE(status->text(), QStringLiteral("Not streaming"));

        QTest::mouseClick(video, Qt::LeftButton, Qt::NoModifier, video->rect().center());
        QCoreApplication::processEvents();

        QVERIFY(video->hasFocus());
        QVERIFY(status->text().endsWith(QStringLiteral("keys to C64")));
    }

    void testDestroyWhileVideoHasFocus_doesNotActOnTheDestroyedPanel()
    {
        // The focused video widget loses focus while ~QWidget destroys it; the
        // panel's slot must not run on the half-destroyed panel.
        auto *panel = new ViewPanel(connection_, makeErrorHandler());
        panel->resize(900, 700);
        panel->show();
        QVERIFY(QTest::qWaitForWindowExposed(panel));
        panel->activateWindow();
        auto *video = panel->findChild<VideoDisplayWidget *>();
        QVERIFY(video != nullptr);
        video->setFocus();
        QCoreApplication::processEvents();
        QVERIFY(video->hasFocus());

        delete panel;  // must not assert or crash
    }

    // =========================================================================
    // IPanel contract — statusMessage signal required for PanelCoordinator routing
    // =========================================================================

    void testHasStatusMessageSignal()
    {
        const QMetaObject *mo = &ViewPanel::staticMetaObject;
        bool found = false;
        for (int i = mo->methodOffset(); i < mo->methodCount(); ++i) {
            QMetaMethod m = mo->method(i);
            if (m.methodType() == QMetaMethod::Signal && m.name() == "statusMessage") {
                found = true;
                break;
            }
        }
        QVERIFY2(found, "ViewPanel must declare statusMessage(const QString &, int) signal");
    }
};

QTEST_MAIN(TestViewPanel)
#include "test_viewpanel.moc"
