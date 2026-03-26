#include "mocks/mockfiledownloader.h"
#include "services/songlengthsdatabase.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QtTest>

namespace {

// Minimal valid Songlengths.md5 content for testing.
// Format: optional "; /path/to/file.sid" path comment, then "md5hash=mm:ss mm:ss..."
QByteArray buildSampleDatabase()
{
    return QByteArray("; HVSC Songlengths database\n"
                      "; /MUSICIANS/T/Tel_Jeroen/Cybernoid_II.sid\n"
                      "d41d8cd98f00b204e9800998ecf8427e=1:23 2:45\n"
                      "; /MUSICIANS/H/Hubbard_Rob/Commando.sid\n"
                      "ffffffffffffffffffffffffffffffff=0:30 1:00\n");
}

}  // namespace

class TestSonglengthsDatabase : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void cleanupTestCase() {}

    void testDownloadDatabase_callsDownloaderWithCorrectUrl()
    {
        MockFileDownloader mock;
        SonglengthsDatabase db(&mock);

        db.downloadDatabase();

        QCOMPARE(mock.mockDownloadCallCount(), 1);
        QCOMPARE(mock.mockLastUrl(), QUrl(QString::fromLatin1(SonglengthsDatabase::DatabaseUrl)));
    }

    void testDownloadDatabase_ignoresWhenAlreadyDownloading()
    {
        MockFileDownloader mock;
        SonglengthsDatabase db(&mock);

        db.downloadDatabase();
        db.downloadDatabase();  // second call should be silently ignored

        QCOMPARE(mock.mockDownloadCallCount(), 1);
    }

    void testDownloadProgress_forwardsSignal()
    {
        MockFileDownloader mock;
        SonglengthsDatabase db(&mock);

        QSignalSpy spy(&db, &SonglengthsDatabase::downloadProgress);

        db.downloadDatabase();
        mock.mockEmitProgress(512, 2048);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toLongLong(), 512LL);
        QCOMPARE(spy.at(0).at(1).toLongLong(), 2048LL);
    }

    void testDownloadFinished_parsesAndEmitsSignals()
    {
        MockFileDownloader mock;
        SonglengthsDatabase db(&mock);

        QSignalSpy finishedSpy(&db, &SonglengthsDatabase::downloadFinished);
        QSignalSpy loadedSpy(&db, &SonglengthsDatabase::databaseLoaded);

        db.downloadDatabase();
        mock.mockEmitFinished(buildSampleDatabase());

        // downloadFinished should be emitted with a non-zero entry count
        QCOMPARE(finishedSpy.count(), 1);
        QVERIFY(finishedSpy.at(0).at(0).toInt() > 0);

        // databaseLoaded should also be emitted
        QCOMPARE(loadedSpy.count(), 1);

        QVERIFY(db.isLoaded());
    }

    void testDownloadFinished_emptyData_emitsFailure()
    {
        MockFileDownloader mock;
        SonglengthsDatabase db(&mock);

        QSignalSpy failedSpy(&db, &SonglengthsDatabase::downloadFailed);
        QSignalSpy finishedSpy(&db, &SonglengthsDatabase::downloadFinished);

        db.downloadDatabase();
        // The mock already blocks empty data at the HttpFileDownloader level,
        // but let's simulate the parser receiving data it cannot parse
        // by providing data that parses to an empty database.
        // Use a parse-fail scenario: valid call but data that produces no entries.
        mock.mockEmitFinished(QByteArray("; no entries here\n"));

        // If parsing yields zero entries the service emits downloadFailed
        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 0);
    }

    void testDownloadFailed_forwardsError()
    {
        MockFileDownloader mock;
        SonglengthsDatabase db(&mock);

        QSignalSpy spy(&db, &SonglengthsDatabase::downloadFailed);

        db.downloadDatabase();
        mock.mockEmitFailed(QStringLiteral("SSL handshake failed"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("SSL handshake failed"));
    }
};

QTEST_MAIN(TestSonglengthsDatabase)
#include "test_songlengthsdatabase.moc"
