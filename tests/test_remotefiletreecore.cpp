/**
 * @file test_remotefiletreecore.cpp
 * @brief Unit tests for pure remote file tree helper functions in remotefiletree namespace.
 *
 * Tests cover:
 *  - isStale(): TTL-disabled, invalid timestamp, age-below-TTL, age-at/over-TTL
 *  - sortEntries(): directories-first ordering, case-insensitive alphabetical sort
 *  - childPath(): path separator handling
 *  - standardPixmapFor(): file-type to standard-pixmap mapping
 */

#include "core/remotefiletreecore.h"

#include <QtTest>

class TestRemoteFileTreeCore : public QObject
{
    Q_OBJECT

private slots:

    // =========================================================================
    // isStale() — cache TTL staleness
    // =========================================================================

    void isStale_WhenNotFetched_ReturnsFalse()
    {
        QDateTime fetchedAt = QDateTime::currentDateTime().addSecs(-9999);
        QDateTime now = QDateTime::currentDateTime();
        // fetched = false => always false regardless of age
        QVERIFY(!remotefiletree::isStale(false, fetchedAt, now, 300));
    }

    void isStale_WhenTtlDisabled_ReturnsFalse()
    {
        // ttlSeconds <= 0 means TTL is disabled — never stale
        QDateTime fetchedAt = QDateTime::currentDateTime().addSecs(-9999);
        QDateTime now = QDateTime::currentDateTime();
        QVERIFY(!remotefiletree::isStale(true, fetchedAt, now, 0));
        QVERIFY(!remotefiletree::isStale(true, fetchedAt, now, -1));
    }

    void isStale_WhenFetchedAtInvalid_ReturnsTrue()
    {
        // An invalid timestamp with TTL enabled means we have no idea when it was fetched
        QDateTime now = QDateTime::currentDateTime();
        QVERIFY(remotefiletree::isStale(true, QDateTime(), now, 300));
    }

    void isStale_WhenAgeBelowTtl_ReturnsFalse()
    {
        // Fetched 100 seconds ago; TTL = 300 seconds → not stale
        QDateTime now = QDateTime::currentDateTime();
        QDateTime fetchedAt = now.addSecs(-100);
        QVERIFY(!remotefiletree::isStale(true, fetchedAt, now, 300));
    }

    void isStale_WhenAgeEqualsOrExceedsTtl_ReturnsTrue()
    {
        QDateTime now = QDateTime::currentDateTime();

        // Exactly at TTL boundary — stale
        QDateTime atBoundary = now.addSecs(-300);
        QVERIFY(remotefiletree::isStale(true, atBoundary, now, 300));

        // One second past TTL — stale
        QDateTime pastBoundary = now.addSecs(-301);
        QVERIFY(remotefiletree::isStale(true, pastBoundary, now, 300));
    }

    // =========================================================================
    // sortEntries() — directory-first, case-insensitive alpha
    // =========================================================================

    void sortEntries_PlacesDirectoriesBeforeFiles()
    {
        QList<FtpEntry> entries;
        entries << FtpEntry{"aardvark.prg", false, 0, "", {}};
        entries << FtpEntry{"ZebraFolder", true, 0, "", {}};
        entries << FtpEntry{"Mango.sid", false, 0, "", {}};
        entries << FtpEntry{"AppleDir", true, 0, "", {}};

        QList<FtpEntry> sorted = remotefiletree::sortEntries(entries);

        QCOMPARE(sorted.size(), 4);
        QVERIFY(sorted[0].isDirectory);
        QVERIFY(sorted[1].isDirectory);
        QVERIFY(!sorted[2].isDirectory);
        QVERIFY(!sorted[3].isDirectory);
    }

    void sortEntries_SortsDirectoriesAlphabeticallyCaseInsensitive()
    {
        QList<FtpEntry> entries;
        entries << FtpEntry{"Zebra", true, 0, "", {}};
        entries << FtpEntry{"apple", true, 0, "", {}};
        entries << FtpEntry{"Mango", true, 0, "", {}};

        QList<FtpEntry> sorted = remotefiletree::sortEntries(entries);

        QCOMPARE(sorted[0].name, QString("apple"));
        QCOMPARE(sorted[1].name, QString("Mango"));
        QCOMPARE(sorted[2].name, QString("Zebra"));
    }

    void sortEntries_SortsFilesAlphabeticallyCaseInsensitive()
    {
        QList<FtpEntry> entries;
        entries << FtpEntry{"Zebra.prg", false, 0, "", {}};
        entries << FtpEntry{"apple.sid", false, 0, "", {}};
        entries << FtpEntry{"Mango.d64", false, 0, "", {}};

        QList<FtpEntry> sorted = remotefiletree::sortEntries(entries);

        QCOMPARE(sorted[0].name, QString("apple.sid"));
        QCOMPARE(sorted[1].name, QString("Mango.d64"));
        QCOMPARE(sorted[2].name, QString("Zebra.prg"));
    }

    void sortEntries_EmptyListReturnsEmpty()
    {
        QList<FtpEntry> sorted = remotefiletree::sortEntries({});
        QVERIFY(sorted.isEmpty());
    }

    // =========================================================================
    // childPath() — path construction
    // =========================================================================

    void childPath_ParentWithoutTrailingSlash_InsertsSlash()
    {
        QCOMPARE(remotefiletree::childPath("/SD/Games", "myfile.prg"),
                 QString("/SD/Games/myfile.prg"));
    }

    void childPath_ParentWithTrailingSlash_DoesNotDuplicateSlash()
    {
        QCOMPARE(remotefiletree::childPath("/SD/Games/", "myfile.prg"),
                 QString("/SD/Games/myfile.prg"));
    }

    void childPath_RootParent_CorrectPath()
    {
        QCOMPARE(remotefiletree::childPath("/", "SD"), QString("/SD"));
    }

    void childPath_EmptyParent_PrependsSlash()
    {
        // Degenerate case — empty parent gets a leading slash injected
        QCOMPARE(remotefiletree::childPath("", "file.prg"), QString("/file.prg"));
    }

    // =========================================================================
    // standardPixmapFor() — file-type to QStyle::StandardPixmap
    // =========================================================================

    void standardPixmapFor_Directory_ReturnsDirIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::Directory),
                 QStyle::SP_DirIcon);
    }

    void standardPixmapFor_SidMusic_ReturnsMediaVolume()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::SidMusic),
                 QStyle::SP_MediaVolume);
    }

    void standardPixmapFor_ModMusic_ReturnsMediaVolume()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::ModMusic),
                 QStyle::SP_MediaVolume);
    }

    void standardPixmapFor_Program_ReturnsFileIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::Program),
                 QStyle::SP_FileIcon);
    }

    void standardPixmapFor_Cartridge_ReturnsDriveHDIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::Cartridge),
                 QStyle::SP_DriveHDIcon);
    }

    void standardPixmapFor_DiskImage_ReturnsDriveFDIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::DiskImage),
                 QStyle::SP_DriveFDIcon);
    }

    void standardPixmapFor_TapeImage_ReturnsDriveCDIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::TapeImage),
                 QStyle::SP_DriveCDIcon);
    }

    void standardPixmapFor_Rom_ReturnsDetailedViewIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::Rom),
                 QStyle::SP_FileDialogDetailedView);
    }

    void standardPixmapFor_Config_ReturnsInfoViewIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::Config),
                 QStyle::SP_FileDialogInfoView);
    }

    void standardPixmapFor_Unknown_ReturnsFileIcon()
    {
        QCOMPARE(remotefiletree::standardPixmapFor(filetype::FileType::Unknown),
                 QStyle::SP_FileIcon);
    }
};

QTEST_MAIN(TestRemoteFileTreeCore)
#include "test_remotefiletreecore.moc"
