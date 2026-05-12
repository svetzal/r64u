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
 * Full dependency wiring is used (real DeviceConnection, RemoteFileModel, etc.)
 * with disconnected mock clients so no network I/O occurs.
 */

#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/deviceconnection.h"
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
    DeviceConnection *connection_ = nullptr;
    RemoteFileModel *remoteModel_ = nullptr;
    TransferQueue *queue_ = nullptr;
    TransferService *transferService_ = nullptr;
    RemoteFileOperationsService *fileOperations_ = nullptr;

    TransferPanel *makePanel()
    {
        mockRest_ = new MockRestClient(this);
        mockFtp_ = new MockFtpClient(this);
        connection_ = new DeviceConnection(mockRest_, mockFtp_, this);
        remoteModel_ = new RemoteFileModel(this);
        queue_ = new TransferQueue(this);
        transferService_ = new TransferService(connection_, queue_, this);
        fileOperations_ = new RemoteFileOperationsService(mockFtp_, this);

        return new TransferPanel(connection_, remoteModel_, transferService_, fileOperations_);
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
};

QTEST_MAIN(TestTransferPanel)
#include "test_transferpanel.moc"
