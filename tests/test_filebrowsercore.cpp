/**
 * @file test_filebrowsercore.cpp
 * @brief Unit tests for the file browser pure core functions.
 *
 * Tests cover all pure functions in namespace filebrowser without any I/O
 * or Qt widget dependencies.
 */

#include "services/filebrowsercore.h"

#include <QtTest>

class TestFileBrowserCore : public QObject
{
    Q_OBJECT

private slots:

    // -----------------------------------------------------------------------
    // resolveDoubleClickAction
    // -----------------------------------------------------------------------

    void testResolveDoubleClickAction_directory_returnsNavigate()
    {
        // isDirectory=true should always return Navigate regardless of file type
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::Unknown, true),
                 filebrowser::DoubleClickAction::Navigate);
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::SidMusic, true),
                 filebrowser::DoubleClickAction::Navigate);
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::DiskImage, true),
                 filebrowser::DoubleClickAction::Navigate);
    }

    void testResolveDoubleClickAction_sidMusic_returnsPlay()
    {
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::SidMusic, false),
                 filebrowser::DoubleClickAction::Play);
    }

    void testResolveDoubleClickAction_modMusic_returnsPlay()
    {
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::ModMusic, false),
                 filebrowser::DoubleClickAction::Play);
    }

    void testResolveDoubleClickAction_program_returnsRun()
    {
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::Program, false),
                 filebrowser::DoubleClickAction::Run);
    }

    void testResolveDoubleClickAction_cartridge_returnsRun()
    {
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::Cartridge, false),
                 filebrowser::DoubleClickAction::Run);
    }

    void testResolveDoubleClickAction_diskImage_returnsMount()
    {
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::DiskImage, false),
                 filebrowser::DoubleClickAction::Mount);
    }

    void testResolveDoubleClickAction_config_returnsLoadConfig()
    {
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::Config, false),
                 filebrowser::DoubleClickAction::LoadConfig);
    }

    void testResolveDoubleClickAction_unknown_returnsNone()
    {
        QCOMPARE(filebrowser::resolveDoubleClickAction(filetype::FileType::Unknown, false),
                 filebrowser::DoubleClickAction::None);
    }

    // -----------------------------------------------------------------------
    // filterPlaylistCandidates
    // -----------------------------------------------------------------------

    void testFilterPlaylistCandidates_emptyList_returnsEmpty()
    {
        QList<QPair<QString, filetype::FileType>> items;
        auto result = filebrowser::filterPlaylistCandidates(items);
        QVERIFY(result.isEmpty());
    }

    void testFilterPlaylistCandidates_allSidFiles_returnsAll()
    {
        QList<QPair<QString, filetype::FileType>> items = {
            {"/C64/Music/track1.sid", filetype::FileType::SidMusic},
            {"/C64/Music/track2.sid", filetype::FileType::SidMusic},
        };
        auto result = filebrowser::filterPlaylistCandidates(items);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0].path, QString("/C64/Music/track1.sid"));
        QCOMPARE(result[1].path, QString("/C64/Music/track2.sid"));
    }

    void testFilterPlaylistCandidates_mixedTypes_returnsOnlySids()
    {
        QList<QPair<QString, filetype::FileType>> items = {
            {"/C64/Music/track.sid", filetype::FileType::SidMusic},
            {"/C64/Games/game.prg", filetype::FileType::Program},
            {"/C64/Music/other.sid", filetype::FileType::SidMusic},
            {"/C64/Disks/disk.d64", filetype::FileType::DiskImage},
        };
        auto result = filebrowser::filterPlaylistCandidates(items);
        QCOMPARE(result.size(), 2);
        QCOMPARE(result[0].path, QString("/C64/Music/track.sid"));
        QCOMPARE(result[1].path, QString("/C64/Music/other.sid"));
    }

    void testFilterPlaylistCandidates_noSidFiles_returnsEmpty()
    {
        QList<QPair<QString, filetype::FileType>> items = {
            {"/C64/Games/game.prg", filetype::FileType::Program},
            {"/C64/Disks/disk.d64", filetype::FileType::DiskImage},
            {"/C64/Carts/cart.crt", filetype::FileType::Cartridge},
        };
        auto result = filebrowser::filterPlaylistCandidates(items);
        QVERIFY(result.isEmpty());
    }
};

QTEST_MAIN(TestFileBrowserCore)
#include "test_filebrowsercore.moc"
