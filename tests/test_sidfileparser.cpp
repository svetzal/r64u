#include <QtTest>

#include "services/sidfileparser.h"

class TestSidFileParser : public QObject
{
    Q_OBJECT

private:
    // Helper to create a minimal valid PSID v1 header
    QByteArray createPsidV1Header(const QString &title = "Test Title",
                                   const QString &author = "Test Author",
                                   const QString &released = "2024 Test")
    {
        QByteArray data(0x76, '\0'); // MinHeaderSize

        // Magic ID
        data[0] = 'P'; data[1] = 'S'; data[2] = 'I'; data[3] = 'D';

        // Version (big-endian)
        data[0x04] = 0x00; data[0x05] = 0x01;

        // Data offset (0x76 for v1)
        data[0x06] = 0x00; data[0x07] = 0x76;

        // Load address
        data[0x08] = 0x10; data[0x09] = 0x00; // $1000

        // Init address
        data[0x0A] = 0x10; data[0x0B] = 0x00; // $1000

        // Play address
        data[0x0C] = 0x10; data[0x0D] = 0x03; // $1003

        // Number of songs
        data[0x0E] = 0x00; data[0x0F] = 0x01;

        // Start song
        data[0x10] = 0x00; data[0x11] = 0x01;

        // Speed
        data[0x12] = 0x00; data[0x13] = 0x00;
        data[0x14] = 0x00; data[0x15] = 0x00;

        // Title (offset 0x16, 32 bytes)
        QByteArray titleBytes = title.toLatin1().left(32);
        for (int i = 0; i < titleBytes.size(); i++) {
            data[0x16 + i] = titleBytes[i];
        }

        // Author (offset 0x36, 32 bytes)
        QByteArray authorBytes = author.toLatin1().left(32);
        for (int i = 0; i < authorBytes.size(); i++) {
            data[0x36 + i] = authorBytes[i];
        }

        // Released (offset 0x56, 32 bytes)
        QByteArray releasedBytes = released.toLatin1().left(32);
        for (int i = 0; i < releasedBytes.size(); i++) {
            data[0x56 + i] = releasedBytes[i];
        }

        return data;
    }

    // Helper to create a PSID v2 header with flags
    QByteArray createPsidV2Header(quint16 flags = 0)
    {
        QByteArray data = createPsidV1Header();
        data.resize(0x7C); // V2HeaderSize

        // Version 2
        data[0x04] = 0x00; data[0x05] = 0x02;

        // Data offset (0x7C for v2)
        data[0x06] = 0x00; data[0x07] = 0x7C;

        // Flags (big-endian)
        data[0x76] = static_cast<char>((flags >> 8) & 0xFF);
        data[0x77] = static_cast<char>(flags & 0xFF);

        return data;
    }

    // Helper to create a PSID v3 header with multi-SID
    QByteArray createPsidV3Header(quint8 secondSidAddr = 0)
    {
        QByteArray data = createPsidV2Header();

        // Version 3
        data[0x04] = 0x00; data[0x05] = 0x03;

        // Second SID address
        data[0x7A] = static_cast<char>(secondSidAddr);

        return data;
    }

    // Helper to create a PSID v4 header with 3 SIDs
    QByteArray createPsidV4Header(quint8 secondSidAddr = 0, quint8 thirdSidAddr = 0)
    {
        QByteArray data = createPsidV3Header(secondSidAddr);

        // Version 4
        data[0x04] = 0x00; data[0x05] = 0x04;

        // Third SID address
        data[0x7B] = static_cast<char>(thirdSidAddr);

        return data;
    }

private slots:
    // ========== isSidFile tests ==========

    void testIsSidFileValid()
    {
        QVERIFY(SidFileParser::isSidFile("song.sid"));
        QVERIFY(SidFileParser::isSidFile("SONG.SID"));
        QVERIFY(SidFileParser::isSidFile("My Song.sid"));
        QVERIFY(SidFileParser::isSidFile("/path/to/song.sid"));
    }

    void testIsSidFileInvalid()
    {
        QVERIFY(!SidFileParser::isSidFile("song.prg"));
        QVERIFY(!SidFileParser::isSidFile("song.d64"));
        QVERIFY(!SidFileParser::isSidFile("song.mod"));
        QVERIFY(!SidFileParser::isSidFile("sid"));
        QVERIFY(!SidFileParser::isSidFile(""));
    }

    // ========== parse - invalid files ==========

    void testParseEmptyData()
    {
        auto info = SidFileParser::parse(QByteArray());
        QVERIFY(!info.valid);
        QCOMPARE(info.format, SidFileParser::Format::Unknown);
    }

    void testParseTooSmall()
    {
        QByteArray data(0x75, '\0'); // One byte less than MinHeaderSize
        auto info = SidFileParser::parse(data);
        QVERIFY(!info.valid);
    }

    void testParseBadMagic()
    {
        QByteArray data(0x76, '\0');
        data[0] = 'X'; data[1] = 'S'; data[2] = 'I'; data[3] = 'D';
        auto info = SidFileParser::parse(data);
        QVERIFY(!info.valid);
    }

    void testParseInvalidVersion()
    {
        QByteArray data = createPsidV1Header();
        data[0x04] = 0x00; data[0x05] = 0x05; // Version 5 (invalid)
        auto info = SidFileParser::parse(data);
        QVERIFY(!info.valid);
    }

    void testParseVersionZero()
    {
        QByteArray data = createPsidV1Header();
        data[0x04] = 0x00; data[0x05] = 0x00; // Version 0 (invalid)
        auto info = SidFileParser::parse(data);
        QVERIFY(!info.valid);
    }

    // ========== parse - PSID format ==========

    void testParsePsidMagic()
    {
        QByteArray data = createPsidV1Header();
        auto info = SidFileParser::parse(data);
        QVERIFY(info.valid);
        QCOMPARE(info.format, SidFileParser::Format::PSID);
    }

    void testParseRsidMagic()
    {
        QByteArray data = createPsidV1Header();
        data[0] = 'R'; // Change to RSID
        auto info = SidFileParser::parse(data);
        QVERIFY(info.valid);
        QCOMPARE(info.format, SidFileParser::Format::RSID);
    }

    // ========== parse - version detection ==========

    void testParseVersion1()
    {
        QByteArray data = createPsidV1Header();
        auto info = SidFileParser::parse(data);
        QVERIFY(info.valid);
        QCOMPARE(info.version, static_cast<quint16>(1));
    }

    void testParseVersion2()
    {
        QByteArray data = createPsidV2Header();
        auto info = SidFileParser::parse(data);
        QVERIFY(info.valid);
        QCOMPARE(info.version, static_cast<quint16>(2));
    }

    void testParseVersion3()
    {
        QByteArray data = createPsidV3Header();
        auto info = SidFileParser::parse(data);
        QVERIFY(info.valid);
        QCOMPARE(info.version, static_cast<quint16>(3));
    }

    void testParseVersion4()
    {
        QByteArray data = createPsidV4Header();
        auto info = SidFileParser::parse(data);
        QVERIFY(info.valid);
        QCOMPARE(info.version, static_cast<quint16>(4));
    }

    // ========== parse - core fields ==========

    void testParseDataOffset()
    {
        QByteArray data = createPsidV1Header();
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.dataOffset, static_cast<quint16>(0x76));
    }

    void testParseLoadAddress()
    {
        QByteArray data = createPsidV1Header();
        data[0x08] = 0x08; data[0x09] = 0x00; // $0800
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.loadAddress, static_cast<quint16>(0x0800));
    }

    void testParseInitAddress()
    {
        QByteArray data = createPsidV1Header();
        data[0x0A] = 0xC0; data[0x0B] = 0x00; // $C000
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.initAddress, static_cast<quint16>(0xC000));
    }

    void testParsePlayAddress()
    {
        QByteArray data = createPsidV1Header();
        data[0x0C] = 0x10; data[0x0D] = 0x03; // $1003
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.playAddress, static_cast<quint16>(0x1003));
    }

    void testParsePlayAddressZero()
    {
        QByteArray data = createPsidV1Header();
        data[0x0C] = 0x00; data[0x0D] = 0x00; // Uses IRQ
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.playAddress, static_cast<quint16>(0));
    }

    void testParseSongCount()
    {
        QByteArray data = createPsidV1Header();
        data[0x0E] = 0x00; data[0x0F] = 0x0A; // 10 songs
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.songs, static_cast<quint16>(10));
    }

    void testParseStartSong()
    {
        QByteArray data = createPsidV1Header();
        data[0x10] = 0x00; data[0x11] = 0x03; // Start song 3
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.startSong, static_cast<quint16>(3));
    }

    void testParseSpeed()
    {
        QByteArray data = createPsidV1Header();
        data[0x12] = 0x12; data[0x13] = 0x34;
        data[0x14] = 0x56; data[0x15] = 0x78;
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.speed, static_cast<quint32>(0x12345678));
    }

    // ========== parse - text fields ==========

    void testParseTitle()
    {
        QByteArray data = createPsidV1Header("Commando", "Rob Hubbard", "1985");
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.title, QString("Commando"));
    }

    void testParseAuthor()
    {
        QByteArray data = createPsidV1Header("Commando", "Rob Hubbard", "1985");
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.author, QString("Rob Hubbard"));
    }

    void testParseReleased()
    {
        QByteArray data = createPsidV1Header("Commando", "Rob Hubbard", "1985 Elite");
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.released, QString("1985 Elite"));
    }

    void testParseEmptyStrings()
    {
        QByteArray data = createPsidV1Header("", "", "");
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.title, QString(""));
        QCOMPARE(info.author, QString(""));
        QCOMPARE(info.released, QString(""));
    }

    void testParseLongStrings()
    {
        QString longString = "This is a very long string that exceeds 32 characters";
        QByteArray data = createPsidV1Header(longString, longString, longString);
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.title.length(), 32);
        QCOMPARE(info.author.length(), 32);
        QCOMPARE(info.released.length(), 32);
    }

    // ========== parse - v2 flags ==========

    void testParseMusPlayerFlag()
    {
        QByteArray data = createPsidV2Header(0x0001); // Bit 0 set
        auto info = SidFileParser::parse(data);
        QVERIFY(info.musPlayer);
    }

    void testParsePlaysSamplesFlag()
    {
        QByteArray data = createPsidV2Header(0x0002); // Bit 1 set
        auto info = SidFileParser::parse(data);
        QVERIFY(info.playsSamples);
        QVERIFY(!info.basicFlag);
    }

    void testParseBasicFlagRsid()
    {
        QByteArray data = createPsidV2Header(0x0002); // Bit 1 set
        data[0] = 'R'; // Change to RSID
        auto info = SidFileParser::parse(data);
        QVERIFY(info.basicFlag);
        QVERIFY(!info.playsSamples);
    }

    void testParseVideoStandardPal()
    {
        QByteArray data = createPsidV2Header(0x0004); // Bits 2-3 = 01 (PAL)
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.videoStandard, SidFileParser::VideoStandard::PAL);
    }

    void testParseVideoStandardNtsc()
    {
        QByteArray data = createPsidV2Header(0x0008); // Bits 2-3 = 10 (NTSC)
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.videoStandard, SidFileParser::VideoStandard::NTSC);
    }

    void testParseVideoStandardBoth()
    {
        QByteArray data = createPsidV2Header(0x000C); // Bits 2-3 = 11 (Both)
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.videoStandard, SidFileParser::VideoStandard::Both);
    }

    void testParseVideoStandardUnknown()
    {
        QByteArray data = createPsidV2Header(0x0000); // Bits 2-3 = 00 (Unknown)
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.videoStandard, SidFileParser::VideoStandard::Unknown);
    }

    void testParseSidModel6581()
    {
        QByteArray data = createPsidV2Header(0x0010); // Bits 4-5 = 01 (6581)
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.sidModel, SidFileParser::SidModel::MOS6581);
    }

    void testParseSidModel8580()
    {
        QByteArray data = createPsidV2Header(0x0020); // Bits 4-5 = 10 (8580)
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.sidModel, SidFileParser::SidModel::MOS8580);
    }

    void testParseSidModelBoth()
    {
        QByteArray data = createPsidV2Header(0x0030); // Bits 4-5 = 11 (Both)
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.sidModel, SidFileParser::SidModel::Both);
    }

    void testParseSidModelUnknown()
    {
        QByteArray data = createPsidV2Header(0x0000); // Bits 4-5 = 00
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.sidModel, SidFileParser::SidModel::Unknown);
    }

    // ========== parse - multi-SID (v3+) ==========

    void testParseSecondSidAddress()
    {
        QByteArray data = createPsidV3Header(0x42); // Second SID at $D420
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.secondSidAddress, static_cast<quint8>(0x42));
    }

    void testParseSecondSidModel()
    {
        QByteArray data = createPsidV3Header(0x42);
        // Set bits 6-7 = 10 (8580) in flags
        data[0x76] = 0x00; data[0x77] = static_cast<char>(0x80); // 0x0080
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.secondSidModel, SidFileParser::SidModel::MOS8580);
    }

    void testParseThirdSidAddress()
    {
        QByteArray data = createPsidV4Header(0x42, 0x5E); // Third SID at $D5E0
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.thirdSidAddress, static_cast<quint8>(0x5E));
    }

    void testParseThirdSidModel()
    {
        QByteArray data = createPsidV4Header(0x42, 0x5E);
        // Set bits 8-9 = 11 (Both) in flags
        data[0x76] = 0x03; data[0x77] = 0x00; // 0x0300
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.thirdSidModel, SidFileParser::SidModel::Both);
    }

    void testParseNoSecondSid()
    {
        QByteArray data = createPsidV3Header(0x00);
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.secondSidAddress, static_cast<quint8>(0));
    }

    // ========== sidModelToString ==========

    void testSidModelToString6581()
    {
        QCOMPARE(SidFileParser::sidModelToString(SidFileParser::SidModel::MOS6581),
                 QString("MOS 6581"));
    }

    void testSidModelToString8580()
    {
        QCOMPARE(SidFileParser::sidModelToString(SidFileParser::SidModel::MOS8580),
                 QString("MOS 8580"));
    }

    void testSidModelToStringBoth()
    {
        QCOMPARE(SidFileParser::sidModelToString(SidFileParser::SidModel::Both),
                 QString("6581/8580"));
    }

    void testSidModelToStringUnknown()
    {
        QCOMPARE(SidFileParser::sidModelToString(SidFileParser::SidModel::Unknown),
                 QString("Unknown"));
    }

    // ========== videoStandardToString ==========

    void testVideoStandardToStringPal()
    {
        QCOMPARE(SidFileParser::videoStandardToString(SidFileParser::VideoStandard::PAL),
                 QString("PAL (50Hz)"));
    }

    void testVideoStandardToStringNtsc()
    {
        QCOMPARE(SidFileParser::videoStandardToString(SidFileParser::VideoStandard::NTSC),
                 QString("NTSC (60Hz)"));
    }

    void testVideoStandardToStringBoth()
    {
        QCOMPARE(SidFileParser::videoStandardToString(SidFileParser::VideoStandard::Both),
                 QString("PAL/NTSC"));
    }

    void testVideoStandardToStringUnknown()
    {
        QCOMPARE(SidFileParser::videoStandardToString(SidFileParser::VideoStandard::Unknown),
                 QString("Unknown"));
    }

    // ========== formatForDisplay ==========

    void testFormatForDisplayInvalid()
    {
        SidFileParser::SidInfo info;
        info.valid = false;
        QString output = SidFileParser::formatForDisplay(info);
        QCOMPARE(output, QString("Invalid SID file"));
    }

    void testFormatForDisplayBasic()
    {
        QByteArray data = createPsidV1Header("Commando", "Rob Hubbard", "1985 Elite");
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("Commando"));
        QVERIFY(output.contains("Rob Hubbard"));
        QVERIFY(output.contains("1985 Elite"));
        QVERIFY(output.contains("PSID v1"));
    }

    void testFormatForDisplayMultipleSongs()
    {
        QByteArray data = createPsidV1Header();
        data[0x0E] = 0x00; data[0x0F] = 0x05; // 5 songs
        data[0x10] = 0x00; data[0x11] = 0x02; // Start song 2
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("Songs: 5"));
        QVERIFY(output.contains("* Song 2")); // Start song marked
        QVERIFY(output.contains("  Song 1")); // Other songs not marked
    }

    void testFormatForDisplay2Sid()
    {
        QByteArray data = createPsidV3Header(0x42);
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("2SID"));
    }

    void testFormatForDisplay3Sid()
    {
        QByteArray data = createPsidV4Header(0x42, 0x5E);
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("3SID"));
    }

    void testFormatForDisplayEmptyTitle()
    {
        QByteArray data = createPsidV1Header("", "Unknown", "");
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("(Untitled)"));
    }

    void testFormatForDisplayEmptyAuthor()
    {
        QByteArray data = createPsidV1Header("Song", "", "");
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("(Unknown)"));
    }

    void testFormatForDisplayRsid()
    {
        QByteArray data = createPsidV2Header(0x0002); // BASIC flag
        data[0] = 'R'; // RSID
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("RSID"));
        QVERIFY(output.contains("Requires real C64 environment"));
        QVERIFY(output.contains("BASIC program"));
    }

    void testFormatForDisplayIrqPlay()
    {
        QByteArray data = createPsidV1Header();
        data[0x0C] = 0x00; data[0x0D] = 0x00; // Play = 0 (uses IRQ)
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        QVERIFY(output.contains("(uses IRQ)"));
    }

    // ========== Edge cases ==========

    void testParseV1WithV2Size()
    {
        // V1 header but data is V2 size - should still work as V1
        QByteArray data = createPsidV1Header();
        data.resize(0x7C);
        auto info = SidFileParser::parse(data);
        QVERIFY(info.valid);
        QCOMPARE(info.version, static_cast<quint16>(1));
        // V2 fields should remain default
        QCOMPARE(info.sidModel, SidFileParser::SidModel::Unknown);
    }

    void testParseBigEndianWordBoundary()
    {
        QByteArray data = createPsidV1Header();
        // Test with max values
        data[0x08] = static_cast<char>(0xFF);
        data[0x09] = static_cast<char>(0xFF); // $FFFF
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.loadAddress, static_cast<quint16>(0xFFFF));
    }

    void testParseManySongs()
    {
        QByteArray data = createPsidV1Header();
        data[0x0E] = 0x00; data[0x0F] = static_cast<char>(99); // 99 songs
        auto info = SidFileParser::parse(data);
        QCOMPARE(info.songs, static_cast<quint16>(99));
    }

    void testFormatForDisplayManySongs()
    {
        QByteArray data = createPsidV1Header();
        data[0x0E] = 0x00; data[0x0F] = static_cast<char>(50); // 50 songs
        auto info = SidFileParser::parse(data);
        QString output = SidFileParser::formatForDisplay(info);

        // Should show first 32 and note "and 18 more"
        QVERIFY(output.contains("Song 32"));
        QVERIFY(output.contains("... and 18 more"));
    }
};

QTEST_MAIN(TestSidFileParser)
#include "test_sidfileparser.moc"
