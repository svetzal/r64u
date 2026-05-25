#include "ui/remotefilebrowsercontroller.h"

#include <QTest>

class TestRemoteFileBrowserController : public QObject
{
    Q_OBJECT

private:
    RemoteFileBrowserController *controller = nullptr;

private slots:
    void init() { controller = new RemoteFileBrowserController(this); }

    void cleanup()
    {
        delete controller;
        controller = nullptr;
    }

    void testBuildNewFolderPath_normalDir()
    {
        QCOMPARE(controller->buildNewFolderPath("/remote/dir", "NewFolder"),
                 QString("/remote/dir/NewFolder"));
    }

    void testBuildNewFolderPath_emptyDirUsesRoot()
    {
        QCOMPARE(controller->buildNewFolderPath("", "NewFolder"), QString("/NewFolder"));
    }

    void testBuildNewFolderPath_emptyNameReturnsEmpty()
    {
        QCOMPARE(controller->buildNewFolderPath("/remote/dir", ""), QString());
    }

    void testBuildNewFolderPath_noDoubleSlash()
    {
        QCOMPARE(controller->buildNewFolderPath("/remote/dir/", "NewFolder"),
                 QString("/remote/dir/NewFolder"));
    }

    void testBuildRenamePath_normalRename()
    {
        QCOMPARE(controller->buildRenamePath("/remote/dir/old.prg", "new.prg"),
                 QString("/remote/dir/new.prg"));
    }

    void testBuildRenamePath_emptyNameReturnsEmpty()
    {
        QCOMPARE(controller->buildRenamePath("/remote/dir/old.prg", ""), QString());
    }

    void testBuildRenamePath_nameWithSlashReturnsEmpty()
    {
        QCOMPARE(controller->buildRenamePath("/remote/dir/old.prg", "bad/name"), QString());
    }

    void testIsValidName_validName()
    {
        QVERIFY(RemoteFileBrowserController::isValidName("ValidName"));
    }

    void testIsValidName_emptyNameIsValid()
    {
        QVERIFY(RemoteFileBrowserController::isValidName(""));
    }

    void testIsValidName_forwardSlashIsInvalid()
    {
        QVERIFY(!RemoteFileBrowserController::isValidName("bad/name"));
    }

    void testIsValidName_backslashIsInvalid()
    {
        QVERIFY(!RemoteFileBrowserController::isValidName("bad\\name"));
    }

    void testBuildDeleteConfirmMessage_singleFile_containsFileName()
    {
        auto msg = controller->buildDeleteConfirmMessage({"/remote/dir/file.prg"}, false);
        QVERIFY(msg.contains("file.prg"));
        QVERIFY(!msg.contains("folder", Qt::CaseInsensitive));
    }

    void testBuildDeleteConfirmMessage_singleDir_containsFolderName()
    {
        auto msg = controller->buildDeleteConfirmMessage({"/remote/Games"}, true);
        QVERIFY(msg.contains("Games"));
        QVERIFY(msg.contains("folder", Qt::CaseInsensitive));
    }

    void testBuildDeleteConfirmMessage_multiplePaths_containsCount()
    {
        auto msg = controller->buildDeleteConfirmMessage({"/a", "/b", "/c"}, false);
        QVERIFY(msg.contains("3"));
    }
};

QTEST_MAIN(TestRemoteFileBrowserController)
#include "test_remotefilebrowsercontroller.moc"
