/**
 * @file test_explorepanelcore.cpp
 * @brief Unit tests for the explorepanel pure core functions.
 *
 * Tests cover calculateActionEnablement() and calculateDriveDisplay() without
 * any I/O, model, or UI dependencies. Both functions are pure — same inputs
 * always produce same outputs — so they are straightforward to test directly.
 */

#include "services/devicetypes.h"
#include "core/filetypecore.h"
#include "ui/explorepanelcore.h"

#include <QtTest>

class TestExplorePanelCore : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // calculateActionEnablement — disconnected state
    // -----------------------------------------------------------------------

    void testEnablement_disconnected_allActionsDisabled()
    {
        auto e = explorepanel::calculateActionEnablement(false, true, filetype::FileType::SidMusic,
                                                         "/SD/Music");
        QVERIFY(!e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(!e.canDownload);
        QVERIFY(!e.canRefresh);
        QVERIFY(!e.canGoUp);
    }

    void testEnablement_disconnectedWithSelection_canRefreshIsFalse()
    {
        auto e = explorepanel::calculateActionEnablement(false, true, filetype::FileType::SidMusic,
                                                         "/SD");
        QVERIFY(!e.canRefresh);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — connected, no selection
    // -----------------------------------------------------------------------

    void testEnablement_connectedNoSelection_onlyRefreshAndGoUpEnabled()
    {
        auto e = explorepanel::calculateActionEnablement(true, false, filetype::FileType::Unknown,
                                                         "/SD/Music");
        QVERIFY(!e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(!e.canDownload);
        QVERIFY(e.canRefresh);
        QVERIFY(e.canGoUp);
    }

    void testEnablement_connectedNoSelection_rootDir_goUpDisabled()
    {
        auto e =
            explorepanel::calculateActionEnablement(true, false, filetype::FileType::Unknown, "/");
        QVERIFY(!e.canGoUp);
        QVERIFY(e.canRefresh);
    }

    void testEnablement_connectedNoSelection_emptyDir_goUpDisabled()
    {
        auto e =
            explorepanel::calculateActionEnablement(true, false, filetype::FileType::Unknown, "");
        QVERIFY(!e.canGoUp);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — canGoUp semantics
    // -----------------------------------------------------------------------

    void testEnablement_connected_nonRootDir_canGoUp()
    {
        auto e = explorepanel::calculateActionEnablement(true, false, filetype::FileType::Unknown,
                                                         "/SD");
        QVERIFY(e.canGoUp);
    }

    void testEnablement_disconnected_nonRootDir_cannotGoUp()
    {
        auto e = explorepanel::calculateActionEnablement(false, false, filetype::FileType::Unknown,
                                                         "/SD/Music");
        QVERIFY(!e.canGoUp);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — FileType::SidMusic
    // -----------------------------------------------------------------------

    void testEnablement_connectedSidSelected_canPlayAndDownload()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::SidMusic,
                                                         "/SD/Music");
        QVERIFY(e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
        QVERIFY(e.canRefresh);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — FileType::ModMusic
    // -----------------------------------------------------------------------

    void testEnablement_connectedModSelected_canPlayAndDownload()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::ModMusic,
                                                         "/SD/Mods");
        QVERIFY(e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — FileType::Program
    // -----------------------------------------------------------------------

    void testEnablement_connectedProgramSelected_canRunAndDownload()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::Program,
                                                         "/SD/Games");
        QVERIFY(!e.canPlay);
        QVERIFY(e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — FileType::Cartridge
    // -----------------------------------------------------------------------

    void testEnablement_connectedCartridgeSelected_canRunAndDownload()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::Cartridge,
                                                         "/SD/Carts");
        QVERIFY(!e.canPlay);
        QVERIFY(e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — FileType::DiskImage
    // -----------------------------------------------------------------------

    void testEnablement_connectedDiskImageSelected_canRunMountAndDownload()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::DiskImage,
                                                         "/SD/Disks");
        QVERIFY(!e.canPlay);
        QVERIFY(e.canRun);
        QVERIFY(e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — FileType::Config
    // -----------------------------------------------------------------------

    void testEnablement_connectedConfigSelected_canLoadConfigAndDownload()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::Config,
                                                         "/SD/Configs");
        QVERIFY(!e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    // -----------------------------------------------------------------------
    // calculateActionEnablement — FileType::Unknown / Directory / TapeImage / Rom
    // -----------------------------------------------------------------------

    void testEnablement_connectedUnknownSelected_onlyDownloadRefreshGoUp()
    {
        auto e =
            explorepanel::calculateActionEnablement(true, true, filetype::FileType::Unknown, "/SD");
        QVERIFY(!e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
        QVERIFY(e.canRefresh);
        QVERIFY(e.canGoUp);
    }

    void testEnablement_connectedDirectorySelected_onlyDownloadRefreshGoUp()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::Directory,
                                                         "/SD/Music");
        QVERIFY(!e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    void testEnablement_connectedTapeImageSelected_onlyDownloadRefreshGoUp()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::TapeImage,
                                                         "/SD/Tapes");
        QVERIFY(!e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    void testEnablement_connectedRomSelected_onlyDownloadRefreshGoUp()
    {
        auto e = explorepanel::calculateActionEnablement(true, true, filetype::FileType::Rom,
                                                         "/SD/Roms");
        QVERIFY(!e.canPlay);
        QVERIFY(!e.canRun);
        QVERIFY(!e.canMount);
        QVERIFY(!e.canLoadConfig);
        QVERIFY(e.canDownload);
    }

    // -----------------------------------------------------------------------
    // calculateDriveDisplay — disconnected
    // -----------------------------------------------------------------------

    void testDriveDisplay_disconnected_allFieldsEmpty()
    {
        QList<DriveInfo> drives;
        DriveInfo d;
        d.name = "A";
        d.imageFile = "game.d64";
        drives.append(d);

        auto state = explorepanel::calculateDriveDisplay(false, drives);
        QVERIFY(state.drive8Image.isEmpty());
        QVERIFY(!state.drive8Mounted);
        QVERIFY(state.drive9Image.isEmpty());
        QVERIFY(!state.drive9Mounted);
    }

    // -----------------------------------------------------------------------
    // calculateDriveDisplay — connected, empty list
    // -----------------------------------------------------------------------

    void testDriveDisplay_connectedEmptyList_allFieldsEmpty()
    {
        auto state = explorepanel::calculateDriveDisplay(true, {});
        QVERIFY(state.drive8Image.isEmpty());
        QVERIFY(!state.drive8Mounted);
        QVERIFY(state.drive9Image.isEmpty());
        QVERIFY(!state.drive9Mounted);
    }

    // -----------------------------------------------------------------------
    // calculateDriveDisplay — drive A (drive 8)
    // -----------------------------------------------------------------------

    void testDriveDisplay_connectedDriveAWithImage_drive8Populated()
    {
        QList<DriveInfo> drives;
        DriveInfo d;
        d.name = "A";
        d.imageFile = "game.d64";
        drives.append(d);

        auto state = explorepanel::calculateDriveDisplay(true, drives);
        QCOMPARE(state.drive8Image, QString("game.d64"));
        QVERIFY(state.drive8Mounted);
        QVERIFY(state.drive9Image.isEmpty());
        QVERIFY(!state.drive9Mounted);
    }

    void testDriveDisplay_connectedDriveAUppercase_drive8Populated()
    {
        QList<DriveInfo> drives;
        DriveInfo d;
        d.name = "A";
        d.imageFile = "disk.d81";
        drives.append(d);

        auto state = explorepanel::calculateDriveDisplay(true, drives);
        QCOMPARE(state.drive8Image, QString("disk.d81"));
        QVERIFY(state.drive8Mounted);
    }

    void testDriveDisplay_connectedDriveANoImage_drive8NotMounted()
    {
        QList<DriveInfo> drives;
        DriveInfo d;
        d.name = "A";
        d.imageFile = "";
        drives.append(d);

        auto state = explorepanel::calculateDriveDisplay(true, drives);
        QVERIFY(state.drive8Image.isEmpty());
        QVERIFY(!state.drive8Mounted);
    }

    // -----------------------------------------------------------------------
    // calculateDriveDisplay — drive B (drive 9)
    // -----------------------------------------------------------------------

    void testDriveDisplay_connectedDriveBWithImage_drive9Populated()
    {
        QList<DriveInfo> drives;
        DriveInfo d;
        d.name = "B";
        d.imageFile = "other.d71";
        drives.append(d);

        auto state = explorepanel::calculateDriveDisplay(true, drives);
        QVERIFY(state.drive8Image.isEmpty());
        QVERIFY(!state.drive8Mounted);
        QCOMPARE(state.drive9Image, QString("other.d71"));
        QVERIFY(state.drive9Mounted);
    }

    void testDriveDisplay_connectedDriveBNoImage_drive9NotMounted()
    {
        QList<DriveInfo> drives;
        DriveInfo d;
        d.name = "B";
        d.imageFile = "";
        drives.append(d);

        auto state = explorepanel::calculateDriveDisplay(true, drives);
        QVERIFY(state.drive9Image.isEmpty());
        QVERIFY(!state.drive9Mounted);
    }

    // -----------------------------------------------------------------------
    // calculateDriveDisplay — both drives
    // -----------------------------------------------------------------------

    void testDriveDisplay_connectedBothDrivesWithImages_bothPopulated()
    {
        QList<DriveInfo> drives;
        DriveInfo dA;
        dA.name = "A";
        dA.imageFile = "game.d64";
        drives.append(dA);

        DriveInfo dB;
        dB.name = "B";
        dB.imageFile = "utils.d81";
        drives.append(dB);

        auto state = explorepanel::calculateDriveDisplay(true, drives);
        QCOMPARE(state.drive8Image, QString("game.d64"));
        QVERIFY(state.drive8Mounted);
        QCOMPARE(state.drive9Image, QString("utils.d81"));
        QVERIFY(state.drive9Mounted);
    }

    void testDriveDisplay_connectedUnknownDriveName_ignoredNotPopulated()
    {
        QList<DriveInfo> drives;
        DriveInfo d;
        d.name = "C";
        d.imageFile = "mystery.d64";
        drives.append(d);

        auto state = explorepanel::calculateDriveDisplay(true, drives);
        QVERIFY(state.drive8Image.isEmpty());
        QVERIFY(!state.drive8Mounted);
        QVERIFY(state.drive9Image.isEmpty());
        QVERIFY(!state.drive9Mounted);
    }
};

QTEST_MAIN(TestExplorePanelCore)
#include "test_explorepanelcore.moc"
