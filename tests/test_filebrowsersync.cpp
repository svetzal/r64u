/**
 * @file test_filebrowsersync.cpp
 * @brief Integration tests for file browser synchronization components.
 *
 * Tests verify that RemoteFileModel and PathNavigationWidget components
 * behave correctly and can be synchronized by a parent widget.
 *
 * Note: RemoteFileBrowserWidget itself is tightly coupled to C64UFtpClient
 * (not IFtpClient), so we test the underlying components that it coordinates.
 */

#include <QtTest>
#include <QSignalSpy>

#include "mocks/mockftpclient.h"
#include "ui/pathnavigationwidget.h"
#include "models/remotefilemodel.h"

class TestFileBrowserSync : public QObject
{
    Q_OBJECT

private:
    MockFtpClient *mockFtp = nullptr;
    RemoteFileModel *model = nullptr;
    PathNavigationWidget *navWidget = nullptr;

private slots:
    void init()
    {
        mockFtp = new MockFtpClient(this);
        mockFtp->mockSetConnected(true);

        model = new RemoteFileModel(this);
        model->setFtpClient(mockFtp);

        navWidget = new PathNavigationWidget("Remote:");
    }

    void cleanup()
    {
        delete navWidget;
        delete model;
        delete mockFtp;
        navWidget = nullptr;
        model = nullptr;
        mockFtp = nullptr;
    }

    // =========================================================================
    // RemoteFileModel Path Synchronization Tests
    // =========================================================================

    // Test initial model state
    void testModelInitialState()
    {
        QCOMPARE(model->rootPath(), QString("/"));
        QCOMPARE(model->rowCount(), 0);  // Empty until populated
    }

    // Test setRootPath updates model state
    void testModelSetRootPath()
    {
        model->setRootPath("/test/path");
        QCOMPARE(model->rootPath(), QString("/test/path"));
    }

    // Test multiple setRootPath calls
    void testModelMultipleSetRootPath()
    {
        model->setRootPath("/first");
        QCOMPARE(model->rootPath(), QString("/first"));

        model->setRootPath("/second");
        QCOMPARE(model->rootPath(), QString("/second"));

        model->setRootPath("/third");
        QCOMPARE(model->rootPath(), QString("/third"));
    }

    // Test setRootPath to root
    void testModelSetRootPathToRoot()
    {
        model->setRootPath("/some/path");
        model->setRootPath("/");
        QCOMPARE(model->rootPath(), QString("/"));
    }

    // Test clear resets model
    void testModelClear()
    {
        // Setup some entries
        QList<FtpEntry> entries;
        FtpEntry file; file.name = "test.txt"; file.isDirectory = false;
        entries << file;
        mockFtp->mockSetDirectoryListing("/", entries);

        model->setRootPath("/populated");
        model->clear();

        // After clear, row count should be 0
        QCOMPARE(model->rowCount(), 0);
    }

    // Test model signals loading state
    void testModelLoadingSignals()
    {
        QSignalSpy loadingStartedSpy(model, &RemoteFileModel::loadingStarted);
        QSignalSpy loadingFinishedSpy(model, &RemoteFileModel::loadingFinished);

        // Just verify signals exist and can be spied
        QVERIFY(loadingStartedSpy.isValid());
        QVERIFY(loadingFinishedSpy.isValid());
    }

    // Test model error signal
    void testModelErrorSignal()
    {
        QSignalSpy errorSpy(model, &RemoteFileModel::errorOccurred);
        QVERIFY(errorSpy.isValid());
    }

    // =========================================================================
    // PathNavigationWidget Tests
    // =========================================================================

    // Test nav widget initial state
    void testNavWidgetInitialState()
    {
        // PathNavigationWidget initializes to "/" by default
        QCOMPARE(navWidget->path(), QString("/"));
    }

    // Test nav widget setPath
    void testNavWidgetSetPath()
    {
        navWidget->setPath("/test/path");
        QCOMPARE(navWidget->path(), QString("/test/path"));
    }

    // Test nav widget multiple setPath calls
    void testNavWidgetMultipleSetPath()
    {
        navWidget->setPath("/first");
        QCOMPARE(navWidget->path(), QString("/first"));

        navWidget->setPath("/second");
        QCOMPARE(navWidget->path(), QString("/second"));

        navWidget->setPath("/third");
        QCOMPARE(navWidget->path(), QString("/third"));
    }

    // Test nav widget setPath to root
    void testNavWidgetSetPathToRoot()
    {
        navWidget->setPath("/some/path");
        navWidget->setPath("/");
        QCOMPARE(navWidget->path(), QString("/"));
    }

    // Test nav widget upClicked signal
    void testNavWidgetUpClickedSignal()
    {
        QSignalSpy upSpy(navWidget, &PathNavigationWidget::upClicked);
        QVERIFY(upSpy.isValid());

        // Signal should be connectable
        // Actual emission requires button click which needs event loop
    }

    // Test nav widget setUpEnabled
    void testNavWidgetSetUpEnabled()
    {
        navWidget->setUpEnabled(true);
        navWidget->setUpEnabled(false);
        // Just verify no crash - up button state is internal
    }

    // Test nav widget empty path
    void testNavWidgetEmptyPath()
    {
        navWidget->setPath("");
        QCOMPARE(navWidget->path(), QString(""));
    }

    // =========================================================================
    // Synchronized State Tests
    // =========================================================================

    // Test that model and nav can be kept in sync by parent
    void testModelAndNavCanSync()
    {
        QString path = "/sync/test";

        // A parent widget would do both of these
        model->setRootPath(path);
        navWidget->setPath(path);

        // Both should have matching paths
        QCOMPARE(model->rootPath(), path);
        QCOMPARE(navWidget->path(), path);
    }

    // Test rapid path changes keep components in sync
    void testRapidPathChangesSync()
    {
        for (int i = 0; i < 10; ++i) {
            QString path = QString("/rapid/%1").arg(i);
            model->setRootPath(path);
            navWidget->setPath(path);

            QCOMPARE(model->rootPath(), path);
            QCOMPARE(navWidget->path(), path);
        }
    }

    // Test deep path hierarchy sync
    void testDeepPathSync()
    {
        QString deepPath = "/a/b/c/d/e/f/g/h/i/j";

        model->setRootPath(deepPath);
        navWidget->setPath(deepPath);

        QCOMPARE(model->rootPath(), deepPath);
        QCOMPARE(navWidget->path(), deepPath);
    }

    // Test special characters in path sync
    void testSpecialCharacterPathSync()
    {
        QString path = "/path with spaces";

        model->setRootPath(path);
        navWidget->setPath(path);

        QCOMPARE(model->rootPath(), path);
        QCOMPARE(navWidget->path(), path);
    }

    // Test parent folder path calculation
    void testParentFolderPathCalculation()
    {
        // Test how a widget would calculate parent paths
        QString currentPath = "/level1/level2/level3";
        QString parentPath;

        int lastSlash = currentPath.lastIndexOf('/');
        if (lastSlash > 0) {
            parentPath = currentPath.left(lastSlash);
        } else {
            parentPath = "/";
        }

        QCOMPARE(parentPath, QString("/level1/level2"));

        // Test root case
        currentPath = "/level1";
        lastSlash = currentPath.lastIndexOf('/');
        if (lastSlash > 0) {
            parentPath = currentPath.left(lastSlash);
        } else {
            parentPath = "/";
        }

        QCOMPARE(parentPath, QString("/"));

        // Test at root
        currentPath = "/";
        lastSlash = currentPath.lastIndexOf('/');
        if (lastSlash > 0) {
            parentPath = currentPath.left(lastSlash);
        } else {
            parentPath = "/";
        }

        QCOMPARE(parentPath, QString("/"));
    }

    // =========================================================================
    // Connection State Tests
    // =========================================================================

    // Test model handles disconnection gracefully
    void testModelDisconnectionState()
    {
        model->setRootPath("/before/disconnect");
        mockFtp->mockSimulateDisconnect();

        // Path state should be preserved
        QCOMPARE(model->rootPath(), QString("/before/disconnect"));
    }

    // Test model handles reconnection
    void testModelReconnectionState()
    {
        model->setRootPath("/test/path");
        mockFtp->mockSimulateDisconnect();
        mockFtp->mockSimulateConnect();

        // Path should still be preserved
        QCOMPARE(model->rootPath(), QString("/test/path"));
    }

    // Test nav widget state after connection cycle
    void testNavWidgetConnectionCycle()
    {
        navWidget->setPath("/preserved/path");

        // Nav widget doesn't track connection state itself
        // Just verify path is preserved
        QCOMPARE(navWidget->path(), QString("/preserved/path"));
    }

    // =========================================================================
    // Model Data Consistency Tests
    // =========================================================================

    // Test model filePath for invalid index returns root
    void testModelFilePathInvalidIndex()
    {
        QModelIndex invalid;
        QString path = model->filePath(invalid);
        // Invalid index returns root node path, which is "/"
        QCOMPARE(path, QString("/"));
    }

    // Test model isDirectory for invalid index (root is directory)
    void testModelIsDirectoryInvalidIndex()
    {
        QModelIndex invalid;
        bool isDir = model->isDirectory(invalid);
        // Invalid index returns root node which is a directory
        QVERIFY(isDir);
    }

    // Test model fileSize for invalid index
    void testModelFileSizeInvalidIndex()
    {
        QModelIndex invalid;
        qint64 size = model->fileSize(invalid);
        QCOMPARE(size, qint64(0));
    }

    // Test model fileType for invalid index (root is directory)
    void testModelFileTypeInvalidIndex()
    {
        QModelIndex invalid;
        RemoteFileModel::FileType type = model->fileType(invalid);
        // Invalid index returns root node which is a directory
        QCOMPARE(type, RemoteFileModel::FileType::Directory);
    }

    // =========================================================================
    // File Type Detection Tests
    // =========================================================================

    // Test SID file detection
    void testFileTypeDetectionSid()
    {
        QCOMPARE(RemoteFileModel::detectFileType("music.sid"),
                 RemoteFileModel::FileType::SidMusic);
        QCOMPARE(RemoteFileModel::detectFileType("MUSIC.SID"),
                 RemoteFileModel::FileType::SidMusic);
    }

    // Test PRG file detection
    void testFileTypeDetectionPrg()
    {
        QCOMPARE(RemoteFileModel::detectFileType("game.prg"),
                 RemoteFileModel::FileType::Program);
        QCOMPARE(RemoteFileModel::detectFileType("GAME.PRG"),
                 RemoteFileModel::FileType::Program);
    }

    // Test D64 file detection
    void testFileTypeDetectionD64()
    {
        QCOMPARE(RemoteFileModel::detectFileType("disk.d64"),
                 RemoteFileModel::FileType::DiskImage);
        QCOMPARE(RemoteFileModel::detectFileType("DISK.D64"),
                 RemoteFileModel::FileType::DiskImage);
    }

    // Test CRT file detection
    void testFileTypeDetectionCrt()
    {
        QCOMPARE(RemoteFileModel::detectFileType("cart.crt"),
                 RemoteFileModel::FileType::Cartridge);
        QCOMPARE(RemoteFileModel::detectFileType("CART.CRT"),
                 RemoteFileModel::FileType::Cartridge);
    }

    // Test unknown file type
    void testFileTypeDetectionUnknown()
    {
        QCOMPARE(RemoteFileModel::detectFileType("file.xyz"),
                 RemoteFileModel::FileType::Unknown);
        QCOMPARE(RemoteFileModel::detectFileType("noextension"),
                 RemoteFileModel::FileType::Unknown);
    }
};

QTEST_MAIN(TestFileBrowserSync)
#include "test_filebrowsersync.moc"
