#include "mainwindow.h"

#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/errortypes.h"
#include "services/playlistservice.h"
#include "services/servicefactory.h"
#include "services/statusmessageservice.h"
#include "services/streamingservice.h"
#include "services/transferservice.h"
#include "ui/configpanel.h"
#include "ui/connectionstatuswidget.h"
#include "ui/connectionuicontroller.h"
#include "ui/explorepanel.h"
#include "ui/filedetailspanel.h"
#include "ui/panelcoordinator.h"
#include "ui/playlistwidget.h"
#include "ui/transferpanel.h"
#include "ui/viewpanel.h"

#include <QApplication>
#include <QHeaderView>
#include <QLayout>
#include <QMenu>
#include <QMenuBar>
#include <QSettings>
#include <QSplitter>
#include <QStatusBar>
#include <QTabBar>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
#include <QTreeView>
#include <QtTest>

#include <algorithm>

class TestMainWindow : public QObject
{
    Q_OBJECT

    /// Records the order in which watched objects are destroyed.
    class DestructionLog
    {
    public:
        void watch(QObject *object, QObject *context)
        {
            QVERIFY(object != nullptr);
            const QString name = QString::fromLatin1(object->metaObject()->className());
            QObject::connect(object, &QObject::destroyed, context,
                             [this, name]() { order_.append(name); });
        }

        [[nodiscard]] qsizetype position(const QString &name) const { return order_.indexOf(name); }

        [[nodiscard]] const QStringList &order() const { return order_; }

    private:
        QStringList order_;
    };

    /// Objects that hold pointers to the shared services.
    static QList<QObject *> serviceUsers(MainWindow &window)
    {
        return {window.findChild<ExplorePanel *>(),
                window.findChild<TransferPanel *>(),
                window.findChild<ViewPanel *>(),
                window.findChild<ConfigPanel *>(),
                window.findChild<StreamingService *>(),
                window.findChild<PanelCoordinator *>(),
                window.findChild<ConnectionUIController *>(),
                window.findChild<ConnectionStatusWidget *>()};
    }

    static QStringList classNames(const QList<QObject *> &objects)
    {
        QStringList names;
        for (const QObject *object : objects) {
            names.append(object ? QString::fromLatin1(object->metaObject()->className())
                                : QStringLiteral("<missing>"));
        }
        return names;
    }

    /// The shared services MainWindow's ServiceFactory owns.
    static QList<QObject *> sharedServices(MainWindow &window)
    {
        return {window.findChild<ServiceFactory *>(),
                window.findChild<DeviceConnectionManager *>(),
                window.findChild<ErrorHandler *>(),
                window.findChild<TransferService *>(),
                window.findChild<TransferQueue *>(),
                window.findChild<RemoteFileModel *>(),
                window.findChild<StatusMessageService *>(),
                window.findChild<PlaylistService *>()};
    }

    /// The toolbar MainWindow adds to itself (not those inside the panels).
    static QToolBar *systemToolBar(MainWindow &window)
    {
        for (QToolBar *toolBar : window.findChildren<QToolBar *>()) {
            if (toolBar->parentWidget() == &window) {
                return toolBar;
            }
        }
        return nullptr;
    }

    static void showAtKnownSize(MainWindow &window)
    {
        window.resize(1200, 800);
        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();
    }

    /// Asserts the System toolbar spans the window with nothing overflowing.
    static void verifySystemToolBarSpansWindow(MainWindow &window)
    {
        QToolBar *toolBar = systemToolBar(window);
        QVERIFY(toolBar != nullptr);
        QCOMPARE(window.toolBarArea(toolBar), Qt::TopToolBarArea);
        QCOMPARE(toolBar->x(), 0);
        QCOMPARE(toolBar->width(), window.width());

        auto *connectionStatus = toolBar->findChild<ConnectionStatusWidget *>();
        QVERIFY(connectionStatus != nullptr);
        QVERIFY2(connectionStatus->isVisible(),
                 "connection status was pushed into the toolbar's extension menu");
    }

    /// The Explore panel's splitter of the given orientation.
    static QSplitter *exploreSplitter(MainWindow &window, Qt::Orientation orientation)
    {
        auto *explore = window.findChild<ExplorePanel *>();
        if (!explore) {
            return nullptr;
        }
        for (QSplitter *splitter : explore->findChildren<QSplitter *>()) {
            if (splitter->orientation() == orientation) {
                return splitter;
            }
        }
        return nullptr;
    }

    /// The Explore panel's remote file tree (not the playlist's QTreeWidget).
    static QTreeView *exploreTreeView(MainWindow &window)
    {
        auto *explore = window.findChild<ExplorePanel *>();
        if (!explore) {
            return nullptr;
        }
        for (QTreeView *view : explore->findChildren<QTreeView *>()) {
            if (qobject_cast<RemoteFileModel *>(view->model())) {
                return view;
            }
        }
        return nullptr;
    }

    /// A window/state saved by an earlier r64u (QMainWindow::saveState version
    /// 0): three unnamed top-area toolbar entries at positions 0, 366 and 1192.
    static QByteArray staleUnnamedToolBarState()
    {
        return QByteArray::fromHex("000000ff00000000fd00000000000006c0000003a5000000040000000400"
                                   "00000800000008fc000000010000000200000003ffffffff0100000000ff"
                                   "ffffff0000000000000000ffffffff010000016effffffff000000000000"
                                   "0000ffffffff01000004a8ffffffff0000000000000000");
    }

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_mainwindow");
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    void cleanup()
    {
        QSettings settings;
        settings.clear();
        settings.sync();
    }

    void testConstruct_doesNotCrash()
    {
        MainWindow window;
        QVERIFY(true);
    }

    void testTabWidget_hasFourTabs()
    {
        MainWindow window;
        QTabWidget *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->count(), 4);
    }

    void testMenuBar_exists()
    {
        MainWindow window;
        QVERIFY(window.menuBar() != nullptr);
        QVERIFY(!window.menuBar()->actions().isEmpty());
    }

    void testSystemToolBar_exists()
    {
        MainWindow window;
        const auto toolbars = window.findChildren<QToolBar *>();
        QVERIFY(!toolbars.isEmpty());
    }

    void testWindowTitle_isJustTheAppNameWhileDisconnected()
    {
        MainWindow window;
        QCOMPARE(window.windowTitle(), QStringLiteral("r64u"));
    }

    void testWindowTitle_namesTheDeviceOnceConnected()
    {
        // Firmware and mode are the toolbar's business; the title carries the device name only
        QCOMPARE(MainWindow::titleFor(QString()), QStringLiteral("r64u"));
        QCOMPARE(MainWindow::titleFor(QStringLiteral("c64u-lounge")),
                 QStringLiteral("r64u \u2014 c64u-lounge"));
    }

    void testTabWidget_firstTabLabelIsExploreRun()
    {
        MainWindow window;
        QTabWidget *tabs = window.findChild<QTabWidget *>();
        QVERIFY(tabs != nullptr);
        QCOMPARE(tabs->tabText(0), QString("Explore/Run"));
        QCOMPARE(tabs->tabText(1), QString("Transfer"));
        QCOMPARE(tabs->tabText(2), QString("View"));
        QCOMPARE(tabs->tabText(3), QString("Config"));
    }

    void testWithNoDevice_remoteBrowserDoesNotKeepRetryingTheListing()
    {
        MainWindow window;
        auto *remoteFileModel = window.findChild<RemoteFileModel *>();
        QVERIFY(remoteFileModel != nullptr);
        QSignalSpy listingErrorSpy(remoteFileModel, &RemoteFileModel::errorOccurred);

        showAtKnownSize(window);
        QTest::qWait(200);

        QVERIFY2(listingErrorSpy.count() <= 1,
                 qPrintable(QStringLiteral("listing of / failed %1 times while disconnected")
                                .arg(listingErrorSpy.count())));
    }

    void testClose_errorsDuringQuitShowNoDialog()
    {
        MainWindow window;
        auto *errorHandler = window.findChild<ErrorHandler *>();
        QVERIFY(errorHandler != nullptr);

        window.close();

        // Quitting stops streaming and waits for the device, so connection
        // errors can arrive now; nobody is left to dismiss a dialog.
        bool dialogShown = false;
        QTimer::singleShot(0, &window, [&dialogShown]() {
            if (QWidget *modal = QApplication::activeModalWidget()) {
                dialogShown = true;
                modal->close();
            }
        });
        errorHandler->handleError(ErrorCategory::Connection, ErrorSeverity::Critical,
                                  QStringLiteral("Connection Error"),
                                  QStringLiteral("Connection refused"));
        QCoreApplication::processEvents();

        QVERIFY(!dialogShown);
    }

    // =========================================================================
    // Full screen — View mode with every piece of chrome hidden
    // =========================================================================

    void testFullScreen_hidesChromeAndSwitchesToViewMode()
    {
        MainWindow window;
        showAtKnownSize(window);
        auto *fullScreen = window.findChild<QAction *>(QStringLiteral("FullScreenAction"));
        QTabWidget *tabs = window.findChild<QTabWidget *>();
        QToolBar *toolBar = systemToolBar(window);
        QVERIFY(fullScreen != nullptr);
        QVERIFY(tabs != nullptr);
        QVERIFY(toolBar != nullptr);
        QCOMPARE(fullScreen->text(), QStringLiteral("Enter Full Screen"));

        fullScreen->trigger();
        QCoreApplication::processEvents();

        QCOMPARE(tabs->currentIndex(), 2);
        QVERIFY(tabs->tabBar()->isHidden());
        QVERIFY(toolBar->isHidden());
        QVERIFY(window.statusBar()->isHidden());
        QCOMPARE(window.centralWidget()->layout()->contentsMargins(), QMargins());
        QCOMPARE(fullScreen->text(), QStringLiteral("Exit Full Screen"));

        // The offscreen platform may not honour showFullScreen; leave via the
        // window state the action restores rather than a real fullscreen exit.
        window.showNormal();
        fullScreen->trigger();
        QCoreApplication::processEvents();

        QVERIFY(!tabs->tabBar()->isHidden());
        QVERIFY(!toolBar->isHidden());
        QVERIFY(!window.statusBar()->isHidden());
        QCOMPARE(window.centralWidget()->layout()->contentsMargins(), QMargins(8, 16, 8, 8));
        QCOMPARE(fullScreen->text(), QStringLiteral("Enter Full Screen"));
    }

    void testFullScreen_escapeRestoresChrome()
    {
        MainWindow window;
        showAtKnownSize(window);
        auto *fullScreen = window.findChild<QAction *>(QStringLiteral("FullScreenAction"));
        QToolBar *toolBar = systemToolBar(window);
        QVERIFY(fullScreen != nullptr);
        QVERIFY(toolBar != nullptr);

        // Escape does nothing outside full screen
        QTest::keyClick(&window, Qt::Key_Escape);
        QVERIFY(!toolBar->isHidden());

        fullScreen->trigger();
        QCoreApplication::processEvents();
        QVERIFY(toolBar->isHidden());

        window.activateWindow();
        QTest::keyClick(&window, Qt::Key_Escape);
        QCoreApplication::processEvents();

        QVERIFY(!toolBar->isHidden());
        QCOMPARE(fullScreen->text(), QStringLiteral("Enter Full Screen"));
    }

    void testFullScreen_menuAndViewToolBarShareOneAction()
    {
        MainWindow window;
        auto *fullScreen = window.findChild<QAction *>(QStringLiteral("FullScreenAction"));
        auto *viewPanel = window.findChild<ViewPanel *>();
        QVERIFY(fullScreen != nullptr);
        QVERIFY(viewPanel != nullptr);

        bool inMenu = false;
        for (QAction *menuAction : window.menuBar()->actions()) {
            if (menuAction->menu() && menuAction->menu()->actions().contains(fullScreen)) {
                inMenu = true;
            }
        }
        QVERIFY(inMenu);

        auto *viewToolBar = viewPanel->findChild<QToolBar *>();
        QVERIFY(viewToolBar != nullptr);
        QVERIFY(viewToolBar->actions().contains(fullScreen));
    }

    // =========================================================================
    // Window layout — saved state
    // =========================================================================

    void testStaleSavedState_systemToolBarStillSpansWindow()
    {
        QCOMPARE(staleUnnamedToolBarState().size(), 113);
        {
            QSettings settings;
            settings.setValue("window/state", staleUnnamedToolBarState());
            settings.sync();
        }

        MainWindow window;
        showAtKnownSize(window);

        verifySystemToolBarSpansWindow(window);
    }

    void testSavedState_roundTripKeepsSystemToolBarSpanningWindow()
    {
        {
            MainWindow first;
            showAtKnownSize(first);
        }  // saves window/state on destruction

        MainWindow second;
        showAtKnownSize(second);

        QToolBar *toolBar = systemToolBar(second);
        QVERIFY(toolBar != nullptr);
        QCOMPARE(toolBar->objectName(), QStringLiteral("SystemToolBar"));
        verifySystemToolBarSpansWindow(second);
    }

    void testExploreLayout_fileListGetsTheWidthByDefault()
    {
        MainWindow window;
        showAtKnownSize(window);
        QSplitter *horizontal = exploreSplitter(window, Qt::Horizontal);
        QVERIFY(horizontal != nullptr);

        const QList<int> sizes = horizontal->sizes();
        QCOMPARE(sizes.size(), 2);
        QVERIFY2(
            sizes[0] > sizes[1],
            qPrintable(QStringLiteral("file list %1 < details %2").arg(sizes[0]).arg(sizes[1])));
    }

    void testPlaylistCollapsed_freesTheSpaceForTheDetailsScreen_andPersists()
    {
        {
            MainWindow window;
            showAtKnownSize(window);
            auto *playlist = window.findChild<PlaylistWidget *>();
            auto *details = window.findChild<FileDetailsPanel *>();
            QVERIFY(playlist != nullptr);
            QVERIFY(details != nullptr);
            QVERIFY(!playlist->isCollapsed());
            const int detailsBefore = details->height();
            const int playlistBefore = playlist->height();

            playlist->setCollapsed(true);
            QCoreApplication::processEvents();

            QVERIFY2(
                details->height() > detailsBefore,
                qPrintable(
                    QStringLiteral("details %1 -> %2").arg(detailsBefore).arg(details->height())));
            QVERIFY(playlist->height() < playlistBefore);
            QCOMPARE(playlist->height(), playlist->maximumHeight());
        }  // saves layout on destruction

        MainWindow second;
        showAtKnownSize(second);
        auto *playlist = second.findChild<PlaylistWidget *>();
        QVERIFY(playlist != nullptr);
        QVERIFY(playlist->isCollapsed());
    }

    void testSavedLayout_exploreSplittersAndHeaderSurviveRestart()
    {
        QList<int> savedHorizontal;
        QList<int> savedVertical;
        {
            MainWindow first;
            showAtKnownSize(first);
            QSplitter *horizontal = exploreSplitter(first, Qt::Horizontal);
            QSplitter *vertical = exploreSplitter(first, Qt::Vertical);
            QTreeView *tree = exploreTreeView(first);
            QVERIFY(horizontal != nullptr);
            QVERIFY(vertical != nullptr);
            QVERIFY(tree != nullptr);

            horizontal->setSizes({300, 900});
            vertical->setSizes({500, 100});
            tree->header()->setSortIndicator(1, Qt::DescendingOrder);
            QCoreApplication::processEvents();
            savedHorizontal = horizontal->sizes();
            savedVertical = vertical->sizes();
        }  // saves layout on destruction

        MainWindow second;
        showAtKnownSize(second);

        QSplitter *horizontal = exploreSplitter(second, Qt::Horizontal);
        QSplitter *vertical = exploreSplitter(second, Qt::Vertical);
        QTreeView *tree = exploreTreeView(second);
        QVERIFY(horizontal != nullptr);
        QVERIFY(vertical != nullptr);
        QVERIFY(tree != nullptr);
        QCOMPARE(horizontal->sizes(), savedHorizontal);
        QCOMPARE(vertical->sizes(), savedVertical);
        QCOMPARE(tree->header()->sortIndicatorSection(), 1);
        QCOMPARE(tree->header()->sortIndicatorOrder(), Qt::DescendingOrder);
    }

    void testExploreDirectory_roundTripsThroughSettings()
    {
        {
            MainWindow first;
            auto *explore = first.findChild<ExplorePanel *>();
            QVERIFY(explore != nullptr);
            explore->setCurrentDirectory("/USB0/MUSIC");
            QCOMPARE(explore->currentDirectory(), QString("/USB0/MUSIC"));
        }  // saves the folder on destruction

        QSettings settings;
        QCOMPARE(settings.value("directories/exploreRemote").toString(), QString("/USB0/MUSIC"));

        MainWindow second;
        auto *explore = second.findChild<ExplorePanel *>();
        QVERIFY(explore != nullptr);
        QCOMPARE(explore->currentDirectory(), QString("/USB0/MUSIC"));
    }

    void testExploreDirectory_defaultsToRootWhenUnset()
    {
        QSettings settings;
        QVERIFY(!settings.contains("directories/exploreRemote"));

        MainWindow window;
        auto *explore = window.findChild<ExplorePanel *>();
        QVERIFY(explore != nullptr);
        QCOMPARE(explore->currentDirectory(), QString("/"));
    }

    void testFailedListing_leavesCurrentDirectoryUnchanged()
    {
        MainWindow window;
        auto *explore = window.findChild<ExplorePanel *>();
        auto *remoteFileModel = window.findChild<RemoteFileModel *>();
        QVERIFY(explore != nullptr);
        QVERIFY(remoteFileModel != nullptr);
        explore->setCurrentDirectory("/USB0/MUSIC");

        // The listing of the restored folder fails, as it would when the device
        // still holds a previous session's FTP connection. The error reaches the
        // status bar; the folder must stay where it was.
        QSignalSpy errorSpy(remoteFileModel, &RemoteFileModel::errorOccurred);
        QVERIFY(QMetaObject::invokeMethod(remoteFileModel, "onListingFailed",
                                          Q_ARG(QString, QStringLiteral("/USB0/MUSIC")),
                                          Q_ARG(QString, QStringLiteral("530 Login incorrect"))));
        QCoreApplication::processEvents();

        QCOMPARE(errorSpy.count(), 1);
        QCOMPARE(explore->currentDirectory(), QString("/USB0/MUSIC"));
    }

    // =========================================================================
    // Teardown — everything using the shared services dies before them
    // =========================================================================

    void testDestruction_destroysServiceUsersBeforeServices()
    {
        auto *window = new MainWindow();
        const QStringList users = classNames(serviceUsers(*window));
        const QStringList services = classNames(sharedServices(*window));
        DestructionLog log;
        for (QObject *object : serviceUsers(*window) + sharedServices(*window)) {
            log.watch(object, this);
        }

        delete window;

        QCOMPARE(log.order().size(), users.size() + services.size());
        qsizetype lastUserDeath = -1;
        for (const QString &user : users) {
            lastUserDeath = std::max(lastUserDeath, log.position(user));
        }
        qsizetype firstServiceDeath = log.order().size();
        for (const QString &service : services) {
            firstServiceDeath = std::min(firstServiceDeath, log.position(service));
        }
        QVERIFY2(lastUserDeath < firstServiceDeath,
                 qPrintable(QStringLiteral("Destruction order: %1")
                                .arg(log.order().join(QStringLiteral(", ")))));
    }

    void testDestruction_servicesNotifyingOnTheirWayOut_reachNoDeadWindowOrPanel()
    {
        auto *window = new MainWindow();
        auto *services = window->findChild<ServiceFactory *>();
        auto *connection = window->findChild<DeviceConnectionManager *>();
        QVERIFY(services != nullptr);
        QVERIFY(connection != nullptr);

        // ServiceFactory announces its destruction before its services die, so
        // the connection manager can still notify: as a real disconnect would.
        connect(services, &QObject::destroyed, this, [connection]() {
            emit connection->stateChanged(DeviceConnectionManager::ConnectionState::Disconnected);
            emit connection->driveInfoUpdated({});
        });

        delete window;  // must not run window or panel slots against deleted panels

        QVERIFY(true);
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
