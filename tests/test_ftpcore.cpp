/**
 * @file test_ftpcore.cpp
 * @brief Unit tests for pure FTP protocol parsing functions.
 *
 * Tests the ftp:: namespace functions which are pure (no I/O, no sockets).
 * Migrated from test_c64uftpclient_protocol.cpp with the API adapted to
 * return std::optional<ftp::PassiveResult> instead of using out-params.
 */

#include <QtTest>

#include <services/ftpcore.h>

class TestFtpCore : public QObject
{
    Q_OBJECT

private slots:
    // =========================================================
    // parsePassiveResponse — success cases
    // =========================================================

    void parsePassiveResponse_StandardFormat()
    {
        auto result = ftp::parsePassiveResponse("Entering Passive Mode (192,168,1,64,4,0)");

        QVERIFY(result.has_value());
        QCOMPARE(result->host, QString("192.168.1.64"));
        QCOMPARE(result->port, static_cast<quint16>(1024));  // (4 * 256) + 0
    }

    void parsePassiveResponse_HighPort()
    {
        auto result = ftp::parsePassiveResponse("Entering Passive Mode (10,0,0,1,200,10)");

        QVERIFY(result.has_value());
        QCOMPARE(result->host, QString("10.0.0.1"));
        QCOMPARE(result->port, static_cast<quint16>(51210));  // (200 * 256) + 10
    }

    void parsePassiveResponse_LowPort()
    {
        auto result = ftp::parsePassiveResponse("Entering Passive Mode (127,0,0,1,0,21)");

        QVERIFY(result.has_value());
        QCOMPARE(result->host, QString("127.0.0.1"));
        QCOMPARE(result->port, static_cast<quint16>(21));  // (0 * 256) + 21
    }

    void parsePassiveResponse_MaxPort()
    {
        // Maximum port 65535 = (255 * 256) + 255
        auto result = ftp::parsePassiveResponse("Entering Passive Mode (0,0,0,0,255,255)");

        QVERIFY(result.has_value());
        QCOMPARE(result->port, static_cast<quint16>(65535));
    }

    void parsePassiveResponse_WithExtraText()
    {
        // Full PASV response line with leading code and trailing period
        auto result = ftp::parsePassiveResponse("227 Entering Passive Mode (172,16,0,1,39,16).");

        QVERIFY(result.has_value());
        QCOMPARE(result->host, QString("172.16.0.1"));
        QCOMPARE(result->port, static_cast<quint16>(10000));  // (39 * 256) + 16
    }

    // =========================================================
    // parsePassiveResponse — failure cases
    // =========================================================

    void parsePassiveResponse_InvalidFormat_NoParens()
    {
        auto result = ftp::parsePassiveResponse("Entering Passive Mode 192,168,1,64,4,0");

        QVERIFY(!result.has_value());
    }

    void parsePassiveResponse_InvalidFormat_MissingNumbers()
    {
        auto result = ftp::parsePassiveResponse("Entering Passive Mode (192,168,1,64,4)");

        QVERIFY(!result.has_value());
    }

    void parsePassiveResponse_InvalidFormat_Empty()
    {
        auto result = ftp::parsePassiveResponse("");

        QVERIFY(!result.has_value());
    }

    void parsePassiveResponse_InvalidFormat_NotNumbers()
    {
        auto result = ftp::parsePassiveResponse("Entering Passive Mode (a,b,c,d,e,f)");

        QVERIFY(!result.has_value());
    }

    void parsePassiveResponse_LeadingZeros()
    {
        // All-zeros address and port
        auto result = ftp::parsePassiveResponse("(0,0,0,0,0,0)");

        QVERIFY(result.has_value());
        QCOMPARE(result->host, QString("0.0.0.0"));
        QCOMPARE(result->port, static_cast<quint16>(0));
    }

    // =========================================================
    // parseDirectoryListing — Unix-style entries
    // =========================================================

    void parseDirectoryListing_UnixStyleFile()
    {
        QByteArray data = "-rw-r--r-- 1 user group 12345 Jan 15 10:30 myfile.txt\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("myfile.txt"));
        QCOMPARE(entries[0].isDirectory, false);
        QCOMPARE(entries[0].size, static_cast<qint64>(12345));
        QCOMPARE(entries[0].permissions, QString("rw-r--r--"));
    }

    void parseDirectoryListing_UnixStyleDirectory()
    {
        QByteArray data = "drwxr-xr-x 2 user group 4096 Feb 28 14:00 subdir\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("subdir"));
        QCOMPARE(entries[0].isDirectory, true);
        QCOMPARE(entries[0].size, static_cast<qint64>(4096));
        QCOMPARE(entries[0].permissions, QString("rwxr-xr-x"));
    }

    void parseDirectoryListing_MultipleEntries()
    {
        QByteArray data = "drwxr-xr-x 2 user group 4096 Jan  1 00:00 dir1\r\n"
                          "-rw-r--r-- 1 user group 1024 Jan  2 12:00 file1.prg\r\n"
                          "-rwxr-xr-x 1 user group 512 Jan  3 15:30 file2.sid\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 3);
        QCOMPARE(entries[0].name, QString("dir1"));
        QVERIFY(entries[0].isDirectory);
        QCOMPARE(entries[1].name, QString("file1.prg"));
        QVERIFY(!entries[1].isDirectory);
        QCOMPARE(entries[2].name, QString("file2.sid"));
        QVERIFY(!entries[2].isDirectory);
    }

    void parseDirectoryListing_SimpleListing()
    {
        // Some FTP servers just return filenames
        QByteArray data = "game.d64\r\nmusic.sid\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].name, QString("game.d64"));
        QCOMPARE(entries[1].name, QString("music.sid"));
        // Simple listings default to non-directory
        QVERIFY(!entries[0].isDirectory);
        QVERIFY(!entries[1].isDirectory);
    }

    void parseDirectoryListing_FiltersDotEntries()
    {
        QByteArray data = "drwxr-xr-x 2 user group 4096 Jan  1 00:00 .\r\n"
                          "drwxr-xr-x 2 user group 4096 Jan  1 00:00 ..\r\n"
                          "-rw-r--r-- 1 user group 1024 Jan  2 12:00 real_file.txt\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("real_file.txt"));
    }

    void parseDirectoryListing_EmptyListing()
    {
        QByteArray data = "";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 0);
    }

    void parseDirectoryListing_OnlyWhitespace()
    {
        QByteArray data = "\r\n\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 0);
    }

    void parseDirectoryListing_LargeFileSize()
    {
        QByteArray data = "-rw-r--r-- 1 user group 1234567890 Mar 10 08:00 bigfile.bin\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].size, static_cast<qint64>(1234567890));
    }

    void parseDirectoryListing_FileWithSpaces()
    {
        QByteArray data = "-rw-r--r-- 1 user group 1024 Apr 20 16:45 my file with spaces.prg\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("my file with spaces.prg"));
    }

    void parseDirectoryListing_TimeFormatWithYear()
    {
        // Some servers show year instead of time for old files
        QByteArray data = "-rw-r--r-- 1 user group 2048 Dec 25  2023 oldfile.txt\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("oldfile.txt"));
        QCOMPARE(entries[0].size, static_cast<qint64>(2048));
    }

    void parseDirectoryListing_MixedFormats()
    {
        // Unix-style lines mixed with a simple filename line
        QByteArray data = "-rw-r--r-- 1 user group 512 Jan  1 00:00 formatted.sid\r\n"
                          "simple_filename.prg\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].name, QString("formatted.sid"));
        QVERIFY(!entries[0].isDirectory);
        QCOMPARE(entries[1].name, QString("simple_filename.prg"));
        QVERIFY(!entries[1].isDirectory);
    }

    void parseDirectoryListing_SymlinkPermissions()
    {
        // lrwxrwxrwx for symlinks — should be treated as non-directory (not 'd')
        QByteArray data = "lrwxrwxrwx 1 user group 7 Jan  1 00:00 linkname\r\n";

        QList<FtpEntry> entries = ftp::parseDirectoryListing(data);

        // The regex requires the first char to be 'd' or '-', so symlinks won't match
        // Unix-style regex and fall through to simple listing path
        QCOMPARE(entries.size(), 1);
        QVERIFY(!entries[0].name.isEmpty());
    }
};

QTEST_MAIN(TestFtpCore)
#include "test_ftpcore.moc"
