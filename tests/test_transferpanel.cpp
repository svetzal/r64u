/**
 * @file test_transferpanel.cpp
 * @brief Unit tests for TransferPanel settings and state propagation.
 *
 * TransferPanel is a composite widget that owns a RemoteFileBrowserWidget and a
 * LocalFileBrowserWidget.  These tests focus on the observable public API:
 *
 * - setCurrentLocalDir / setCurrentRemoteDir state propagation
 * - loadSettings / saveSettings round-trip
 * - selectedLocalPath / selectedRemotePath return empty when nothing selected
 *
 * Full dependency wiring is used (real DeviceConnectionManager, RemoteFileModel, etc.)
 * with disconnected mock clients so no network I/O occurs.
 */

#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/deviceconnectionmanager.h"
#include "services/errorhandler.h"
#include "services/remotefileoperationsservice.h"
#include "services/transferservice.h"
#include "ui/transferpanel.h"

#include <QSettings>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

class TestTransferPanel : public QObject
{
    Q_OBJECT

private:
    MockRestClient *mockRest_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    DeviceConnectionManager *connection_ = nullptr;
    RemoteFileModel *remoteModel_ = nullptr;
    TransferQueue *queue_ = nullptr;
    TransferService *transferService_ = nullptr;
    RemoteFileOperationsService *fileOperations_ = nullptr;

    ErrorHandler *makeErrorHandler() { return new ErrorHandler(nullptr, this); }

    TransferPanel *makePanel()
    {
        mockRest_ = new MockRestClient(this);
        mockFtp_ = new MockFtpClient(this);
        connection_ = new DeviceConnectionManager(mockRest_, mockFtp_, this);
        remoteModel_ = new RemoteFileModel(this);
        queue_ = new TransferQueue(this);
        transferService_ = new TransferService(connection_, queue_, this);
        fileOperations_ = new RemoteFileOperationsService(mockFtp_, this);

        return new TransferPanel(connection_, remoteModel_, transferService_, fileOperations_,
                                 makeErrorHandler());
    }

    TransferPanel *makePanelWithErrorHandler(ErrorHandler *&outErrorHandler)
    {
        mockRest_ = new MockRestClient(this);
        mockFtp_ = new MockFtpClient(this);
        connection_ = new DeviceConnectionManager(mockRest_, mockFtp_, this);
        remoteModel_ = new RemoteFileModel(this);
        queue_ = new TransferQueue(this);
        transferService_ = new TransferService(connection_, queue_, this);
        fileOperations_ = new RemoteFileOperationsService(mockFtp_, this);

        outErrorHandler = makeErrorHandler();
        return new TransferPanel(connection_, remoteModel_, transferService_, fileOperations_,
                                 outErrorHandler);
    }

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_transferpanel");
        QSettings settings;
        settings.remove("directories");
    }

    void cleanup()
    {
        QSettings settings;
        settings.remove("directories");
    }

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        auto *panel = makePanel();
        QVERIFY(panel != nullptr);
    }

    // =========================================================================
    // setCurrentRemoteDir / currentRemoteDir state propagation
    // =========================================================================

    void testSetCurrentRemoteDir_UpdatesCurrentDir()
    {
        auto *panel = makePanel();
        panel->setCurrentRemoteDir("/SD");
        QCOMPARE(panel->currentRemoteDir(), QString("/SD"));
    }

    void testSetCurrentRemoteDir_Root()
    {
        auto *panel = makePanel();
        panel->setCurrentRemoteDir("/");
        QCOMPARE(panel->currentRemoteDir(), QString("/"));
    }

    // =========================================================================
    // setCurrentLocalDir / currentLocalDir state propagation
    // =========================================================================

    void testSetCurrentLocalDir_UpdatesCurrentDir()
    {
        auto *panel = makePanel();
        QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        panel->setCurrentLocalDir(homePath);
        QCOMPARE(panel->currentLocalDir(), homePath);
    }

    // =========================================================================
    // selectedLocalPath / selectedRemotePath — empty when nothing selected
    // =========================================================================

    void testSelectedLocalPath_NothingSelected_ReturnsEmpty()
    {
        auto *panel = makePanel();
        QVERIFY(panel->selectedLocalPath().isEmpty());
    }

    void testSelectedRemotePath_NothingSelected_ReturnsEmpty()
    {
        auto *panel = makePanel();
        QVERIFY(panel->selectedRemotePath().isEmpty());
    }

    // =========================================================================
    // isSelectedRemoteDirectory — returns false when nothing selected
    // =========================================================================

    void testIsSelectedRemoteDirectory_NothingSelected_ReturnsFalse()
    {
        auto *panel = makePanel();
        QVERIFY(!panel->isSelectedRemoteDirectory());
    }

    // =========================================================================
    // loadSettings / saveSettings round-trip
    // =========================================================================

    void testSaveSettings_PersistsRemoteDir()
    {
        auto *panel = makePanel();
        panel->setCurrentRemoteDir("/Games");
        panel->saveSettings();

        QSettings settings;
        QCOMPARE(settings.value("directories/remote").toString(), QString("/Games"));
    }

    void testLoadSettings_RestoresRemoteDir()
    {
        {
            QSettings settings;
            settings.setValue("directories/remote", "/Music");
        }
        auto *panel = makePanel();
        panel->loadSettings();
        QCOMPARE(panel->currentRemoteDir(), QString("/Music"));
    }

    void testLoadSettings_DefaultRemoteDir_WhenNoneStored()
    {
        auto *panel = makePanel();
        panel->loadSettings();
        QCOMPARE(panel->currentRemoteDir(), QString("/"));
    }

    // =========================================================================
    // fileOperations() accessor
    // =========================================================================

    void testFileOperations_ReturnsInjected()
    {
        auto *panel = makePanel();
        QCOMPARE(panel->fileOperations(), fileOperations_);
    }

    // =========================================================================
    // Disconnected transfer operations — error routed through ErrorHandler
    // =========================================================================

    void testOnUploadRequested_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        ErrorHandler *errorHandler = nullptr;
        auto *panel = makePanelWithErrorHandler(errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(panel, "onUploadRequested", Q_ARG(QString, "/local/file.prg"),
                                  Q_ARG(bool, false));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("not connected"));
    }

    void testOnUploadDirectoryRequested_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        ErrorHandler *errorHandler = nullptr;
        auto *panel = makePanelWithErrorHandler(errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(panel, "onUploadRequested", Q_ARG(QString, "/local/mydir"),
                                  Q_ARG(bool, true));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("not connected"));
    }

    void testOnDownloadRequested_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        ErrorHandler *errorHandler = nullptr;
        auto *panel = makePanelWithErrorHandler(errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(panel, "onDownloadRequested", Q_ARG(QString, "/SD/file.prg"),
                                  Q_ARG(bool, false));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("not connected"));
    }

    void testOnDownloadDirectoryRequested_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        ErrorHandler *errorHandler = nullptr;
        auto *panel = makePanelWithErrorHandler(errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(panel, "onDownloadRequested", Q_ARG(QString, "/SD/Games"),
                                  Q_ARG(bool, true));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("not connected"));
    }

    void testOnDeleteRequested_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        ErrorHandler *errorHandler = nullptr;
        auto *panel = makePanelWithErrorHandler(errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(panel, "onDeleteRequested", Q_ARG(QString, "/SD/file.prg"),
                                  Q_ARG(bool, false));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("not connected"));
    }

    void testOnDeleteDirectoryRequested_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        ErrorHandler *errorHandler = nullptr;
        auto *panel = makePanelWithErrorHandler(errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(panel, "onDeleteRequested", Q_ARG(QString, "/SD/Games"),
                                  Q_ARG(bool, true));

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("not connected"));
    }
};

QTEST_MAIN(TestTransferPanel)
#include "test_transferpanel.moc"
