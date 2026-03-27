/**
 * @file test_filetypecore.cpp
 * @brief Unit tests for the filetype pure core functions.
 *
 * Tests cover all pure functions in namespace filetype without any I/O,
 * model, or UI dependencies.
 */

#include "services/filetypecore.h"

#include <QtTest>

class TestFileTypeCore : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // detectFromFilename
    // -----------------------------------------------------------------------

    void testDetect_sidExtension_returnsSidMusic()
    {
        QCOMPARE(filetype::detectFromFilename("track.sid"), filetype::FileType::SidMusic);
    }

    void testDetect_psidExtension_returnsSidMusic()
    {
        QCOMPARE(filetype::detectFromFilename("track.psid"), filetype::FileType::SidMusic);
    }

    void testDetect_rsidExtension_returnsSidMusic()
    {
        QCOMPARE(filetype::detectFromFilename("track.rsid"), filetype::FileType::SidMusic);
    }

    void testDetect_modExtension_returnsModMusic()
    {
        QCOMPARE(filetype::detectFromFilename("song.mod"), filetype::FileType::ModMusic);
    }

    void testDetect_xmExtension_returnsModMusic()
    {
        QCOMPARE(filetype::detectFromFilename("song.xm"), filetype::FileType::ModMusic);
    }

    void testDetect_s3mExtension_returnsModMusic()
    {
        QCOMPARE(filetype::detectFromFilename("song.s3m"), filetype::FileType::ModMusic);
    }

    void testDetect_itExtension_returnsModMusic()
    {
        QCOMPARE(filetype::detectFromFilename("song.it"), filetype::FileType::ModMusic);
    }

    void testDetect_prgExtension_returnsProgram()
    {
        QCOMPARE(filetype::detectFromFilename("game.prg"), filetype::FileType::Program);
    }

    void testDetect_p00Extension_returnsProgram()
    {
        QCOMPARE(filetype::detectFromFilename("game.p00"), filetype::FileType::Program);
    }

    void testDetect_crtExtension_returnsCartridge()
    {
        QCOMPARE(filetype::detectFromFilename("cart.crt"), filetype::FileType::Cartridge);
    }

    void testDetect_d64Extension_returnsDiskImage()
    {
        QCOMPARE(filetype::detectFromFilename("disk.d64"), filetype::FileType::DiskImage);
    }

    void testDetect_d71Extension_returnsDiskImage()
    {
        QCOMPARE(filetype::detectFromFilename("disk.d71"), filetype::FileType::DiskImage);
    }

    void testDetect_d81Extension_returnsDiskImage()
    {
        QCOMPARE(filetype::detectFromFilename("disk.d81"), filetype::FileType::DiskImage);
    }

    void testDetect_g64Extension_returnsDiskImage()
    {
        QCOMPARE(filetype::detectFromFilename("disk.g64"), filetype::FileType::DiskImage);
    }

    void testDetect_g71Extension_returnsDiskImage()
    {
        QCOMPARE(filetype::detectFromFilename("disk.g71"), filetype::FileType::DiskImage);
    }

    void testDetect_tapExtension_returnsTapeImage()
    {
        QCOMPARE(filetype::detectFromFilename("tape.tap"), filetype::FileType::TapeImage);
    }

    void testDetect_t64Extension_returnsTapeImage()
    {
        QCOMPARE(filetype::detectFromFilename("tape.t64"), filetype::FileType::TapeImage);
    }

    void testDetect_romExtension_returnsRom()
    {
        QCOMPARE(filetype::detectFromFilename("kernel.rom"), filetype::FileType::Rom);
    }

    void testDetect_binExtension_returnsRom()
    {
        QCOMPARE(filetype::detectFromFilename("kernel.bin"), filetype::FileType::Rom);
    }

    void testDetect_cfgExtension_returnsConfig()
    {
        QCOMPARE(filetype::detectFromFilename("device.cfg"), filetype::FileType::Config);
    }

    void testDetect_unknownExtension_returnsUnknown()
    {
        QCOMPARE(filetype::detectFromFilename("file.xyz"), filetype::FileType::Unknown);
    }

    void testDetect_noExtension_returnsUnknown()
    {
        QCOMPARE(filetype::detectFromFilename("filename"), filetype::FileType::Unknown);
    }

    void testDetect_caseInsensitive_returnsSidMusic()
    {
        QCOMPARE(filetype::detectFromFilename("track.SID"), filetype::FileType::SidMusic);
        QCOMPARE(filetype::detectFromFilename("track.Sid"), filetype::FileType::SidMusic);
    }

    void testDetect_pathWithDots_usesLastExtension()
    {
        QCOMPARE(filetype::detectFromFilename("my.file.name.sid"), filetype::FileType::SidMusic);
    }

    // -----------------------------------------------------------------------
    // displayName
    // -----------------------------------------------------------------------

    void testDisplayName_eachType_returnsNonEmptyString()
    {
        using FT = filetype::FileType;
        for (auto type : {FT::Unknown, FT::Directory, FT::SidMusic, FT::ModMusic, FT::Program,
                          FT::Cartridge, FT::DiskImage, FT::TapeImage, FT::Rom, FT::Config}) {
            QVERIFY(!filetype::displayName(type).isEmpty());
        }
    }

    void testDisplayName_unknown_returnsFile()
    {
        QCOMPARE(filetype::displayName(filetype::FileType::Unknown), QString("File"));
    }

    void testDisplayName_sidMusic_returnsSidMusic()
    {
        QCOMPARE(filetype::displayName(filetype::FileType::SidMusic), QString("SID Music"));
    }

    void testDisplayName_diskImage_returnsDiskImage()
    {
        QCOMPARE(filetype::displayName(filetype::FileType::DiskImage), QString("Disk Image"));
    }

    // -----------------------------------------------------------------------
    // capabilities
    // -----------------------------------------------------------------------

    void testCapabilities_sidMusic_canPlayOnly()
    {
        auto caps = filetype::capabilities(filetype::FileType::SidMusic);
        QVERIFY(caps.canPlay);
        QVERIFY(!caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_modMusic_canPlayOnly()
    {
        auto caps = filetype::capabilities(filetype::FileType::ModMusic);
        QVERIFY(caps.canPlay);
        QVERIFY(!caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_program_canRunOnly()
    {
        auto caps = filetype::capabilities(filetype::FileType::Program);
        QVERIFY(!caps.canPlay);
        QVERIFY(caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_cartridge_canRunOnly()
    {
        auto caps = filetype::capabilities(filetype::FileType::Cartridge);
        QVERIFY(!caps.canPlay);
        QVERIFY(caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_diskImage_canRunAndMount()
    {
        auto caps = filetype::capabilities(filetype::FileType::DiskImage);
        QVERIFY(!caps.canPlay);
        QVERIFY(caps.canRun);
        QVERIFY(caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_config_canLoadConfigOnly()
    {
        auto caps = filetype::capabilities(filetype::FileType::Config);
        QVERIFY(!caps.canPlay);
        QVERIFY(!caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(caps.canLoadConfig);
    }

    void testCapabilities_unknown_noCaps()
    {
        auto caps = filetype::capabilities(filetype::FileType::Unknown);
        QVERIFY(!caps.canPlay);
        QVERIFY(!caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_directory_noCaps()
    {
        auto caps = filetype::capabilities(filetype::FileType::Directory);
        QVERIFY(!caps.canPlay);
        QVERIFY(!caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_tapeImage_noCaps()
    {
        auto caps = filetype::capabilities(filetype::FileType::TapeImage);
        QVERIFY(!caps.canPlay);
        QVERIFY(!caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    void testCapabilities_rom_noCaps()
    {
        auto caps = filetype::capabilities(filetype::FileType::Rom);
        QVERIFY(!caps.canPlay);
        QVERIFY(!caps.canRun);
        QVERIFY(!caps.canMount);
        QVERIFY(!caps.canLoadConfig);
    }

    // -----------------------------------------------------------------------
    // defaultAction
    // -----------------------------------------------------------------------

    void testDefaultAction_sidMusic_play()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::SidMusic),
                 filetype::DefaultAction::Play);
    }

    void testDefaultAction_modMusic_play()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::ModMusic),
                 filetype::DefaultAction::Play);
    }

    void testDefaultAction_program_run()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::Program),
                 filetype::DefaultAction::Run);
    }

    void testDefaultAction_cartridge_run()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::Cartridge),
                 filetype::DefaultAction::Run);
    }

    void testDefaultAction_diskImage_mount()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::DiskImage),
                 filetype::DefaultAction::Mount);
    }

    void testDefaultAction_config_loadConfig()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::Config),
                 filetype::DefaultAction::LoadConfig);
    }

    void testDefaultAction_unknown_none()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::Unknown),
                 filetype::DefaultAction::None);
    }

    void testDefaultAction_directory_none()
    {
        QCOMPARE(filetype::defaultAction(filetype::FileType::Directory),
                 filetype::DefaultAction::None);
    }
};

QTEST_MAIN(TestFileTypeCore)
#include "test_filetypecore.moc"
