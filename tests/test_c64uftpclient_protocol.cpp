/**
 * @file test_c64uftpclient_protocol.cpp
 * @brief Unit tests for C64UFtpClient protocol handling.
 *
 * Tests verify:
 * - Passive mode address extraction from PASV response
 * - Directory listing parsing (Unix-style format)
 * - State machine transitions
 * - Operation guards (not-logged-in behavior)
 */

#include <QtTest>
#include <QSignalSpy>
#include <QTcpSocket>

// Use angle brackets to force include path search order (avoids tests/services shadow)
#include <services/c64uftpclient.h>

// Since the parsing methods are now public in C64UFtpClient,
// we can test them directly without a subclass.

class TestC64UFtpClientProtocol : public QObject
{
    Q_OBJECT

private:
    C64UFtpClient *ftp;

private slots:
    void init()
    {
        ftp = new C64UFtpClient(this);
    }

    void cleanup()
    {
        delete ftp;
        ftp = nullptr;
    }

    // === PASV Response Parsing Tests ===

    void parsePassiveResponse_StandardFormat()
    {
        QString host;
        quint16 port = 0;

        // Standard PASV response format
        bool result = ftp->parsePassiveResponse(
            "Entering Passive Mode (192,168,1,64,4,0)", host, port);

        QVERIFY(result);
        QCOMPARE(host, QString("192.168.1.64"));
        QCOMPARE(port, static_cast<quint16>(1024));  // (4 * 256) + 0
    }

    void parsePassiveResponse_HighPort()
    {
        QString host;
        quint16 port = 0;

        // High port number
        bool result = ftp->parsePassiveResponse(
            "Entering Passive Mode (10,0,0,1,200,10)", host, port);

        QVERIFY(result);
        QCOMPARE(host, QString("10.0.0.1"));
        QCOMPARE(port, static_cast<quint16>(51210));  // (200 * 256) + 10
    }

    void parsePassiveResponse_LowPort()
    {
        QString host;
        quint16 port = 0;

        // Low port number
        bool result = ftp->parsePassiveResponse(
            "Entering Passive Mode (127,0,0,1,0,21)", host, port);

        QVERIFY(result);
        QCOMPARE(host, QString("127.0.0.1"));
        QCOMPARE(port, static_cast<quint16>(21));  // (0 * 256) + 21
    }

    void parsePassiveResponse_MaxPort()
    {
        QString host;
        quint16 port = 0;

        // Maximum port 65535 = (255 * 256) + 255
        bool result = ftp->parsePassiveResponse(
            "Entering Passive Mode (0,0,0,0,255,255)", host, port);

        QVERIFY(result);
        QCOMPARE(port, static_cast<quint16>(65535));
    }

    void parsePassiveResponse_WithExtraText()
    {
        QString host;
        quint16 port = 0;

        // Extra text before and after
        bool result = ftp->parsePassiveResponse(
            "227 Entering Passive Mode (172,16,0,1,39,16).", host, port);

        QVERIFY(result);
        QCOMPARE(host, QString("172.16.0.1"));
        QCOMPARE(port, static_cast<quint16>(10000));  // (39 * 256) + 16
    }

    void parsePassiveResponse_InvalidFormat_NoParens()
    {
        QString host;
        quint16 port = 0;

        bool result = ftp->parsePassiveResponse(
            "Entering Passive Mode 192,168,1,64,4,0", host, port);

        QVERIFY(!result);
    }

    void parsePassiveResponse_InvalidFormat_MissingNumbers()
    {
        QString host;
        quint16 port = 0;

        bool result = ftp->parsePassiveResponse(
            "Entering Passive Mode (192,168,1,64,4)", host, port);

        QVERIFY(!result);
    }

    void parsePassiveResponse_InvalidFormat_Empty()
    {
        QString host;
        quint16 port = 0;

        bool result = ftp->parsePassiveResponse("", host, port);

        QVERIFY(!result);
    }

    void parsePassiveResponse_InvalidFormat_NotNumbers()
    {
        QString host;
        quint16 port = 0;

        bool result = ftp->parsePassiveResponse(
            "Entering Passive Mode (a,b,c,d,e,f)", host, port);

        QVERIFY(!result);
    }

    // === Directory Listing Parsing Tests ===

    void parseDirectoryListing_UnixStyleFile()
    {
        QByteArray data = "-rw-r--r-- 1 user group 12345 Jan 15 10:30 myfile.txt\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("myfile.txt"));
        QCOMPARE(entries[0].isDirectory, false);
        QCOMPARE(entries[0].size, static_cast<qint64>(12345));
        QCOMPARE(entries[0].permissions, QString("rw-r--r--"));
    }

    void parseDirectoryListing_UnixStyleDirectory()
    {
        QByteArray data = "drwxr-xr-x 2 user group 4096 Feb 28 14:00 subdir\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("subdir"));
        QCOMPARE(entries[0].isDirectory, true);
        QCOMPARE(entries[0].size, static_cast<qint64>(4096));
        QCOMPARE(entries[0].permissions, QString("rwxr-xr-x"));
    }

    void parseDirectoryListing_MultipleEntries()
    {
        QByteArray data =
            "drwxr-xr-x 2 user group 4096 Jan  1 00:00 dir1\r\n"
            "-rw-r--r-- 1 user group 1024 Jan  2 12:00 file1.prg\r\n"
            "-rwxr-xr-x 1 user group 512 Jan  3 15:30 file2.sid\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

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

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 2);
        QCOMPARE(entries[0].name, QString("game.d64"));
        QCOMPARE(entries[1].name, QString("music.sid"));
        // Simple listings default to non-directory
        QVERIFY(!entries[0].isDirectory);
        QVERIFY(!entries[1].isDirectory);
    }

    void parseDirectoryListing_FiltersDotEntries()
    {
        QByteArray data =
            "drwxr-xr-x 2 user group 4096 Jan  1 00:00 .\r\n"
            "drwxr-xr-x 2 user group 4096 Jan  1 00:00 ..\r\n"
            "-rw-r--r-- 1 user group 1024 Jan  2 12:00 real_file.txt\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("real_file.txt"));
    }

    void parseDirectoryListing_EmptyListing()
    {
        QByteArray data = "";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 0);
    }

    void parseDirectoryListing_OnlyWhitespace()
    {
        QByteArray data = "\r\n\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 0);
    }

    void parseDirectoryListing_LargeFileSize()
    {
        QByteArray data = "-rw-r--r-- 1 user group 1234567890 Mar 10 08:00 bigfile.bin\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].size, static_cast<qint64>(1234567890));
    }

    void parseDirectoryListing_FileWithSpaces()
    {
        QByteArray data = "-rw-r--r-- 1 user group 1024 Apr 20 16:45 my file with spaces.prg\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("my file with spaces.prg"));
    }

    void parseDirectoryListing_TimeFormatWithYear()
    {
        // Some servers show year instead of time for old files
        QByteArray data = "-rw-r--r-- 1 user group 2048 Dec 25  2023 oldfile.txt\r\n";

        QList<FtpEntry> entries = ftp->parseDirectoryListing(data);

        QCOMPARE(entries.size(), 1);
        QCOMPARE(entries[0].name, QString("oldfile.txt"));
        QCOMPARE(entries[0].size, static_cast<qint64>(2048));
    }

    // === Initial State Tests ===

    void testInitialState_Disconnected()
    {
        QCOMPARE(ftp->state(), IFtpClient::State::Disconnected);
    }

    void testInitialState_NotConnected()
    {
        QVERIFY(!ftp->isConnected());
    }

    void testInitialState_NotLoggedIn()
    {
        QVERIFY(!ftp->isLoggedIn());
    }

    void testInitialState_DefaultDirectory()
    {
        QCOMPARE(ftp->currentDirectory(), QString("/"));
    }

    void testInitialState_NoHost()
    {
        QCOMPARE(ftp->host(), QString(""));
    }

    // === Host Configuration Tests ===

    void testSetHost_UpdatesHost()
    {
        ftp->setHost("192.168.1.64");
        QCOMPARE(ftp->host(), QString("192.168.1.64"));
    }

    void testSetHost_WithCustomPort()
    {
        ftp->setHost("192.168.1.64", 2121);
        QCOMPARE(ftp->host(), QString("192.168.1.64"));
        // Port is stored internally, not exposed via getter
    }

    void testSetHost_CanChangeHost()
    {
        ftp->setHost("192.168.1.1");
        QCOMPARE(ftp->host(), QString("192.168.1.1"));

        ftp->setHost("10.0.0.1");
        QCOMPARE(ftp->host(), QString("10.0.0.1"));
    }

    // === Connection State Tests ===

    void testConnectToHost_EmitsError_WhenAlreadyConnecting()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        // State should be Connecting
        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);

        // Try to connect again
        ftp->connectToHost();

        // Should emit error
        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("already"));
    }

    void testConnectToHost_ChangesState_ToConnecting()
    {
        QSignalSpy stateSpy(ftp, &C64UFtpClient::stateChanged);

        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);
        QCOMPARE(stateSpy.count(), 1);
    }

    void testDisconnect_WhenDisconnected_NoOp()
    {
        QSignalSpy disconnectedSpy(ftp, &C64UFtpClient::disconnected);

        // Should be a no-op when already disconnected
        ftp->disconnect();

        QCOMPARE(ftp->state(), IFtpClient::State::Disconnected);
        QCOMPARE(disconnectedSpy.count(), 0);
    }

    // === Operation Guard Tests ===

    void testList_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->list("/some/path");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testChangeDirectory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->changeDirectory("/some/path");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testMakeDirectory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->makeDirectory("/new/dir");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testRemoveDirectory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->removeDirectory("/some/dir");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testDownload_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->download("/remote/file.txt", "/local/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testDownloadToMemory_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->downloadToMemory("/remote/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testUpload_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->upload("/local/file.txt", "/remote/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testRemove_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->remove("/some/file.txt");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    void testRename_EmitsError_WhenNotLoggedIn()
    {
        QSignalSpy errorSpy(ftp, &C64UFtpClient::error);

        ftp->rename("/old/path", "/new/path");

        QCOMPARE(errorSpy.count(), 1);
        QVERIFY(errorSpy.first().first().toString().contains("not connected"));
    }

    // === Abort Tests ===

    void testAbort_WhenDisconnected_SetsReady()
    {
        // Currently abort() sets state to Ready even when disconnected.
        // This may be unexpected behavior - abort when disconnected could
        // arguably be a no-op. Documenting current behavior.
        ftp->abort();
        // Note: This sets Ready even though we're not actually connected
        QCOMPARE(ftp->state(), IFtpClient::State::Ready);
    }

    // === IsConnected Logic Tests ===

    void testIsConnected_FalseWhenDisconnected()
    {
        QVERIFY(!ftp->isConnected());
    }

    void testIsConnected_FalseWhenConnecting()
    {
        ftp->setHost("192.168.1.64");
        ftp->connectToHost();

        QCOMPARE(ftp->state(), IFtpClient::State::Connecting);
        QVERIFY(!ftp->isConnected());
    }
};

QTEST_MAIN(TestC64UFtpClientProtocol)
#include "test_c64uftpclient_protocol.moc"
