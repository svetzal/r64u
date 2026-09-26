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
#include "ui/panelcoordinator.h"
#include "ui/transferpanel.h"
#include "ui/viewpanel.h"

#include <QApplication>
#include <QMenuBar>
#include <QSettings>
#include <QTabWidget>
#include <QTimer>
#include <QToolBar>
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

    void testWindowTitle_containsModeName()
    {
        MainWindow window;
        QVERIFY(window.windowTitle().contains("Explore/Run"));
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
