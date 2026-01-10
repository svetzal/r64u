#include <QtTest>

#include "services/diskimagereader.h"

class TestDiskImageReader : public QObject
{
    Q_OBJECT

private:
    // Helper to create a minimal D64 image (174848 bytes)
    QByteArray createMinimalD64(const QString &diskName = "TEST DISK",
                                 const QString &diskId = "01")
    {
        // D64: 683 sectors * 256 bytes = 174848 bytes
        QByteArray data(174848, '\0');

        // Track 18, sector 0 is the BAM
        // Calculate offset: tracks 1-17 have 21 sectors each = 17*21 = 357 sectors
        // So track 18, sector 0 starts at 357 * 256 = 91392
        int bamOffset = 91392;

        // BAM structure for D64:
        // Offset 0x00: Track/sector of first directory sector (18, 1)
        data[bamOffset + 0x00] = 18;
        data[bamOffset + 0x01] = 1;

        // Offset 0x02: DOS version type ('A')
        data[bamOffset + 0x02] = 0x41;

        // Offset 0x04-0x8F: BAM entries (4 bytes per track, 35 tracks)
        // Each entry: 1 byte free sectors, 3 bytes bitmap
        for (int t = 0; t < 35; t++) {
            int entryOff = bamOffset + 0x04 + (t * 4);
            // Set free block count (varies by track zone)
            int sectors = 21; // Simplified - tracks 1-17
            if (t >= 17 && t <= 23) sectors = 19;
            else if (t >= 24 && t <= 29) sectors = 18;
            else if (t >= 30) sectors = 17;
            data[entryOff] = static_cast<char>(sectors);
            // Set bitmap to all free (0xFF, 0xFF, 0x1F for 21 sectors)
            data[entryOff + 1] = static_cast<char>(0xFF);
            data[entryOff + 2] = static_cast<char>(0xFF);
            data[entryOff + 3] = static_cast<char>(0x1F);
        }

        // Offset 0x90-0x9F: Disk name (16 bytes, padded with 0xA0)
        QByteArray nameBytes = diskName.toLatin1().left(16);
        for (int i = 0; i < 16; i++) {
            if (i < nameBytes.size()) {
                data[bamOffset + 0x90 + i] = nameBytes[i];
            } else {
                data[bamOffset + 0x90 + i] = static_cast<char>(0xA0);
            }
        }

        // Offset 0xA0-0xA1: 0xA0 padding
        data[bamOffset + 0xA0] = static_cast<char>(0xA0);
        data[bamOffset + 0xA1] = static_cast<char>(0xA0);

        // Offset 0xA2-0xA3: Disk ID (2 bytes)
        if (diskId.size() >= 2) {
            data[bamOffset + 0xA2] = diskId[0].toLatin1();
            data[bamOffset + 0xA3] = diskId[1].toLatin1();
        }

        // Offset 0xA4: 0xA0 padding
        data[bamOffset + 0xA4] = static_cast<char>(0xA0);

        // Offset 0xA5-0xA6: DOS type (e.g., "2A")
        data[bamOffset + 0xA5] = '2';
        data[bamOffset + 0xA6] = 'A';

        // Track 18, sector 1 is the first directory sector
        // Offset = 91392 + 256 = 91648
        int dirOffset = bamOffset + 256;

        // Directory sector header: next track=0 (end of chain), next sector=0xFF
        data[dirOffset + 0x00] = 0;
        data[dirOffset + 0x01] = static_cast<char>(0xFF);

        return data;
    }

    // Helper to add a directory entry to a D64 image
    void addD64DirectoryEntry(QByteArray &data, int entryIndex,
                               const QString &filename,
                               DiskImageReader::FileType type,
                               quint16 blocks,
                               bool closed = true,
                               bool locked = false)
    {
        int bamOffset = 91392;
        int dirOffset = bamOffset + 256;
        int entryOffset = dirOffset + (entryIndex * 32);

        // Offset 2: File type byte
        quint8 typeByte = static_cast<quint8>(type);
        if (closed) typeByte |= 0x80;
        if (locked) typeByte |= 0x40;
        data[entryOffset + 2] = static_cast<char>(typeByte);

        // Offset 3-4: First track/sector (use dummy values)
        data[entryOffset + 3] = 17;  // First track
        data[entryOffset + 4] = 0;   // First sector

        // Offset 5-20: Filename (16 bytes, padded with 0xA0)
        QByteArray nameBytes = filename.toLatin1().left(16);
        for (int i = 0; i < 16; i++) {
            if (i < nameBytes.size()) {
                data[entryOffset + 5 + i] = nameBytes[i];
            } else {
                data[entryOffset + 5 + i] = static_cast<char>(0xA0);
            }
        }

        // Offset 29-30: File size in blocks (little-endian)
        data[entryOffset + 29] = static_cast<char>(blocks & 0xFF);
        data[entryOffset + 30] = static_cast<char>((blocks >> 8) & 0xFF);
    }

private slots:
    // ========== isDiskImage tests ==========

    void testIsDiskImageD64()
    {
        QVERIFY(DiskImageReader::isDiskImage("game.d64"));
        QVERIFY(DiskImageReader::isDiskImage("GAME.D64"));
        QVERIFY(DiskImageReader::isDiskImage("/path/to/game.d64"));
    }

    void testIsDiskImageD71()
    {
        QVERIFY(DiskImageReader::isDiskImage("game.d71"));
        QVERIFY(DiskImageReader::isDiskImage("GAME.D71"));
    }

    void testIsDiskImageD81()
    {
        QVERIFY(DiskImageReader::isDiskImage("game.d81"));
        QVERIFY(DiskImageReader::isDiskImage("GAME.D81"));
    }

    void testIsDiskImageInvalid()
    {
        QVERIFY(!DiskImageReader::isDiskImage("game.prg"));
        QVERIFY(!DiskImageReader::isDiskImage("game.sid"));
        QVERIFY(!DiskImageReader::isDiskImage("d64"));
        QVERIFY(!DiskImageReader::isDiskImage(""));
    }

    // ========== parse - format detection ==========

    void testParseEmptyData()
    {
        DiskImageReader reader;
        auto dir = reader.parse(QByteArray());
        QCOMPARE(dir.format, DiskImageReader::Format::Unknown);
    }

    void testParseD64BySize()
    {
        DiskImageReader reader;
        QByteArray data(174848, '\0'); // Standard D64 size
        auto dir = reader.parse(data, ""); // No filename hint
        QCOMPARE(dir.format, DiskImageReader::Format::D64);
    }

    void testParseD64WithErrorBytes()
    {
        DiskImageReader reader;
        QByteArray data(175531, '\0'); // D64 with error bytes
        auto dir = reader.parse(data, "");
        QCOMPARE(dir.format, DiskImageReader::Format::D64);
    }

    void testParseD64Extended40Track()
    {
        DiskImageReader reader;
        QByteArray data(196608, '\0'); // 40-track D64
        auto dir = reader.parse(data, "");
        QCOMPARE(dir.format, DiskImageReader::Format::D64);
    }

    void testParseD71BySize()
    {
        DiskImageReader reader;
        QByteArray data(349696, '\0'); // D71 size
        auto dir = reader.parse(data, "");
        QCOMPARE(dir.format, DiskImageReader::Format::D71);
    }

    void testParseD81BySize()
    {
        DiskImageReader reader;
        QByteArray data(819200, '\0'); // D81 size
        auto dir = reader.parse(data, "");
        QCOMPARE(dir.format, DiskImageReader::Format::D81);
    }

    void testParseByFilename()
    {
        DiskImageReader reader;
        // Even with wrong size, filename takes precedence
        QByteArray data(100, '\0');

        auto dirD64 = reader.parse(data, "game.d64");
        QCOMPARE(dirD64.format, DiskImageReader::Format::D64);

        auto dirD71 = reader.parse(data, "game.d71");
        QCOMPARE(dirD71.format, DiskImageReader::Format::D71);

        auto dirD81 = reader.parse(data, "game.d81");
        QCOMPARE(dirD81.format, DiskImageReader::Format::D81);
    }

    void testParseUnknownSize()
    {
        DiskImageReader reader;
        QByteArray data(100000, '\0'); // Unknown size
        auto dir = reader.parse(data, "");
        QCOMPARE(dir.format, DiskImageReader::Format::Unknown);
    }

    // ========== parse - D64 BAM ==========

    void testParseD64DiskName()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64("HELLO WORLD", "AB");
        auto dir = reader.parse(data, "test.d64");

        // Disk name should be raw PETSCII
        QString asciiName = QString::fromLatin1(dir.diskName);
        QCOMPARE(asciiName, QString("HELLO WORLD"));
    }

    void testParseD64DiskId()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64("TEST", "XY");
        auto dir = reader.parse(data, "test.d64");

        QString asciiId = QString::fromLatin1(dir.diskId);
        QCOMPARE(asciiId, QString("XY"));
    }

    void testParseD64DosType()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        auto dir = reader.parse(data, "test.d64");

        QString dosType = QString::fromLatin1(dir.dosType);
        QCOMPARE(dosType, QString("2A"));
    }

    void testParseD64FreeBlocks()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        auto dir = reader.parse(data, "test.d64");

        // Our test image sets all blocks as free
        // D64 has 664 blocks usable (683 total - 19 for track 18 directory)
        QVERIFY(dir.freeBlocks > 0);
    }

    // ========== parse - directory entries ==========

    void testParseD64EmptyDirectory()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        auto dir = reader.parse(data, "test.d64");

        QVERIFY(dir.entries.isEmpty());
    }

    void testParseD64SingleFile()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        addD64DirectoryEntry(data, 0, "GAME", DiskImageReader::FileType::PRG, 100);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries.size(), 1);
        QCOMPARE(QString::fromLatin1(dir.entries[0].filename), QString("GAME"));
        QCOMPARE(dir.entries[0].type, DiskImageReader::FileType::PRG);
        QCOMPARE(dir.entries[0].sizeInBlocks, static_cast<quint16>(100));
        QVERIFY(dir.entries[0].isClosed);
        QVERIFY(!dir.entries[0].isLocked);
    }

    void testParseD64MultipleFiles()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        addD64DirectoryEntry(data, 0, "FILE1", DiskImageReader::FileType::PRG, 10);
        addD64DirectoryEntry(data, 1, "FILE2", DiskImageReader::FileType::SEQ, 20);
        addD64DirectoryEntry(data, 2, "FILE3", DiskImageReader::FileType::USR, 30);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries.size(), 3);
        QCOMPARE(QString::fromLatin1(dir.entries[0].filename), QString("FILE1"));
        QCOMPARE(QString::fromLatin1(dir.entries[1].filename), QString("FILE2"));
        QCOMPARE(QString::fromLatin1(dir.entries[2].filename), QString("FILE3"));
    }

    void testParseD64FileTypes()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        addD64DirectoryEntry(data, 0, "DELETED", DiskImageReader::FileType::DEL, 1);
        addD64DirectoryEntry(data, 1, "SEQUENTIAL", DiskImageReader::FileType::SEQ, 2);
        addD64DirectoryEntry(data, 2, "PROGRAM", DiskImageReader::FileType::PRG, 3);
        addD64DirectoryEntry(data, 3, "USER", DiskImageReader::FileType::USR, 4);
        addD64DirectoryEntry(data, 4, "RELATIVE", DiskImageReader::FileType::REL, 5);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries.size(), 5);
        QCOMPARE(dir.entries[0].type, DiskImageReader::FileType::DEL);
        QCOMPARE(dir.entries[1].type, DiskImageReader::FileType::SEQ);
        QCOMPARE(dir.entries[2].type, DiskImageReader::FileType::PRG);
        QCOMPARE(dir.entries[3].type, DiskImageReader::FileType::USR);
        QCOMPARE(dir.entries[4].type, DiskImageReader::FileType::REL);
    }

    void testParseD64LockedFile()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        addD64DirectoryEntry(data, 0, "LOCKED", DiskImageReader::FileType::PRG, 50, true, true);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries.size(), 1);
        QVERIFY(dir.entries[0].isLocked);
    }

    void testParseD64SplatFile()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        addD64DirectoryEntry(data, 0, "SPLAT", DiskImageReader::FileType::PRG, 25, false, false);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries.size(), 1);
        QVERIFY(!dir.entries[0].isClosed);
    }

    void testParseD64LargeBlockCount()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        // Max blocks in D64 is around 664, but field can hold up to 65535
        addD64DirectoryEntry(data, 0, "BIG", DiskImageReader::FileType::PRG, 500);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries[0].sizeInBlocks, static_cast<quint16>(500));
    }

    // ========== fileTypeString ==========

    void testFileTypeStringDel()
    {
        QCOMPARE(DiskImageReader::fileTypeString(DiskImageReader::FileType::DEL),
                 QString("DEL"));
    }

    void testFileTypeStringSeq()
    {
        QCOMPARE(DiskImageReader::fileTypeString(DiskImageReader::FileType::SEQ),
                 QString("SEQ"));
    }

    void testFileTypeStringPrg()
    {
        QCOMPARE(DiskImageReader::fileTypeString(DiskImageReader::FileType::PRG),
                 QString("PRG"));
    }

    void testFileTypeStringUsr()
    {
        QCOMPARE(DiskImageReader::fileTypeString(DiskImageReader::FileType::USR),
                 QString("USR"));
    }

    void testFileTypeStringRel()
    {
        QCOMPARE(DiskImageReader::fileTypeString(DiskImageReader::FileType::REL),
                 QString("REL"));
    }

    void testFileTypeStringCbm()
    {
        QCOMPARE(DiskImageReader::fileTypeString(DiskImageReader::FileType::CBM),
                 QString("CBM"));
    }

    void testFileTypeStringDir()
    {
        QCOMPARE(DiskImageReader::fileTypeString(DiskImageReader::FileType::DIR),
                 QString("DIR"));
    }

    // ========== formatDirectoryListing ==========

    void testFormatDirectoryListingEmpty()
    {
        DiskImageReader::DiskDirectory dir;
        dir.format = DiskImageReader::Format::D64;
        dir.diskName = "TEST DISK";
        dir.diskId = "AB";
        dir.dosType = "2A";
        dir.freeBlocks = 664;

        QString listing = DiskImageReader::formatDirectoryListing(dir);

        QVERIFY(listing.contains("664 BLOCKS FREE"));
    }

    void testFormatDirectoryListingWithFile()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64("MY DISK", "99");
        addD64DirectoryEntry(data, 0, "HELLO", DiskImageReader::FileType::PRG, 42);
        auto dir = reader.parse(data, "test.d64");

        QString listing = DiskImageReader::formatDirectoryListing(dir);

        QVERIFY(listing.contains("PRG"));
        QVERIFY(listing.contains("42"));
        QVERIFY(listing.contains("BLOCKS FREE"));
    }

    void testFormatDirectoryListingLockedFile()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        addD64DirectoryEntry(data, 0, "LOCKED", DiskImageReader::FileType::PRG, 10, true, true);
        auto dir = reader.parse(data, "test.d64");

        QString listing = DiskImageReader::formatDirectoryListing(dir);

        QVERIFY(listing.contains("PRG<")); // < indicates locked
    }

    void testFormatDirectoryListingSplatFile()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        addD64DirectoryEntry(data, 0, "SPLAT", DiskImageReader::FileType::PRG, 10, false, false);
        auto dir = reader.parse(data, "test.d64");

        QString listing = DiskImageReader::formatDirectoryListing(dir);

        QVERIFY(listing.contains("*PRG")); // * indicates splat (unclosed)
    }

    // ========== asciiToC64Font ==========

    void testAsciiToC64FontPassthrough()
    {
        // The function just returns text as-is
        QCOMPARE(DiskImageReader::asciiToC64Font("HELLO"),
                 QString("HELLO"));
        QCOMPARE(DiskImageReader::asciiToC64Font("Test 123"),
                 QString("Test 123"));
    }

    // ========== Edge cases ==========

    void testParseTruncatedData()
    {
        DiskImageReader reader;
        // Create a D64 but truncate it
        QByteArray data = createMinimalD64();
        data.truncate(1000); // Way too small

        // Should handle gracefully
        auto dir = reader.parse(data, "test.d64");
        QCOMPARE(dir.format, DiskImageReader::Format::D64);
        // May have empty/partial data but shouldn't crash
    }

    void testParseFilenameWithPadding()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        // Short filename - padding should be trimmed
        addD64DirectoryEntry(data, 0, "A", DiskImageReader::FileType::PRG, 1);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries.size(), 1);
        // Filename should be just "A", not "A" followed by padding
        QCOMPARE(dir.entries[0].filename.size(), 1);
        QCOMPARE(QString::fromLatin1(dir.entries[0].filename), QString("A"));
    }

    void testParseLongFilename()
    {
        DiskImageReader reader;
        QByteArray data = createMinimalD64();
        // Maximum 16 character filename
        addD64DirectoryEntry(data, 0, "1234567890123456", DiskImageReader::FileType::PRG, 1);
        auto dir = reader.parse(data, "test.d64");

        QCOMPARE(dir.entries.size(), 1);
        QCOMPARE(dir.entries[0].filename.size(), 16);
    }
};

QTEST_MAIN(TestDiskImageReader)
#include "test_diskimagereader.moc"
