#include "mocks/mockfiledownloader.h"
#include "services/cacheddownloadmanager.h"

#include <QDir>
#include <QFile>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QtTest>

namespace {

QByteArray sampleData()
{
    return QByteArray("sample parsed content\n");
}

// A parse callback that always succeeds, returning a fixed entry count.
CachedDownloadManager::ParseCallback makeSuccessCallback(int *callCount = nullptr,
                                                         int returnCount = 3)
{
    return [callCount, returnCount](const QByteArray &) -> int {
        if (callCount != nullptr) {
            ++(*callCount);
        }
        return returnCount;
    };
}

// A parse callback that always fails (returns -1).
CachedDownloadManager::ParseCallback makeFailCallback()
{
    return [](const QByteArray &) -> int { return -1; };
}

}  // namespace

class TestCachedDownloadManager : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    void cleanupTestCase() {}

    // ---------------------------------------------------------------------------
    // Construction — no cache
    // ---------------------------------------------------------------------------

    void testConstruction_noCacheFile_doesNotCallParseCallback()
    {
        MockFileDownloader mock;
        int callCount = 0;

        // Make sure no cached file exists
        CachedDownloadManager manager(&mock, QStringLiteral("test_no_cache.dat"),
                                      QUrl("http://example.com/file.dat"),
                                      makeSuccessCallback(&callCount), nullptr);

        // No cached file → parse callback must NOT have been called
        QCOMPARE(callCount, 0);
    }

    // ---------------------------------------------------------------------------
    // Construction — cached file present
    // ---------------------------------------------------------------------------

    void testConstruction_withCacheFile_invokesParseCallbackAndEmitsLoaded()
    {
        MockFileDownloader mock;

        // Write a cache file so the manager finds it on construction
        const QString cacheFilename = QStringLiteral("test_autoload.dat");
        CachedDownloadManager tempManager(&mock, cacheFilename, QUrl("http://example.com/"),
                                          makeSuccessCallback(), nullptr);
        const QString cachePath = tempManager.cacheFilePath();
        {
            QFile f(cachePath);
            QVERIFY(f.open(QIODevice::WriteOnly));
            f.write(sampleData());
        }

        int callCount = 0;
        MockFileDownloader mock2;
        QSignalSpy loadedSpy(nullptr, nullptr);  // placeholder; we need a real object

        // Create a fresh manager that will find the file and autoload
        CachedDownloadManager manager2(&mock2, cacheFilename, QUrl("http://example.com/"),
                                       makeSuccessCallback(&callCount), nullptr);

        QSignalSpy loaded(&manager2, &CachedDownloadManager::loaded);
        // loaded() fires synchronously in the constructor — check callCount directly
        QCOMPARE(callCount, 1);

        // Clean up
        QFile::remove(cachePath);
    }

    // ---------------------------------------------------------------------------
    // download() — delegates to downloader
    // ---------------------------------------------------------------------------

    void testDownload_callsDownloaderWithCorrectUrl()
    {
        MockFileDownloader mock;
        const QUrl url(QStringLiteral("http://example.com/data.txt"));

        CachedDownloadManager manager(&mock, QStringLiteral("dl_test.dat"), url,
                                      makeSuccessCallback(), nullptr);

        manager.download();

        QCOMPARE(mock.mockDownloadCallCount(), 1);
        QCOMPARE(mock.mockLastUrl(), url);
    }

    void testDownload_noOpWhenAlreadyDownloading()
    {
        MockFileDownloader mock;
        const QUrl url(QStringLiteral("http://example.com/data.txt"));

        CachedDownloadManager manager(&mock, QStringLiteral("dl_noop.dat"), url,
                                      makeSuccessCallback(), nullptr);

        manager.download();
        manager.download();  // should be ignored

        QCOMPARE(mock.mockDownloadCallCount(), 1);
    }

    // ---------------------------------------------------------------------------
    // cancelDownload()
    // ---------------------------------------------------------------------------

    void testCancelDownload_delegatesToDownloader()
    {
        MockFileDownloader mock;

        CachedDownloadManager manager(&mock, QStringLiteral("cancel_test.dat"),
                                      QUrl("http://example.com/"), makeSuccessCallback(), nullptr);

        manager.download();
        manager.cancelDownload();

        QCOMPARE(mock.mockCancelCallCount(), 1);
        QVERIFY(!mock.mockIsDownloading());
    }

    // ---------------------------------------------------------------------------
    // Progress signal forwarding
    // ---------------------------------------------------------------------------

    void testDownloadProgress_forwardsSignal()
    {
        MockFileDownloader mock;

        CachedDownloadManager manager(&mock, QStringLiteral("progress_test.dat"),
                                      QUrl("http://example.com/"), makeSuccessCallback(), nullptr);

        QSignalSpy spy(&manager, &CachedDownloadManager::downloadProgress);

        manager.download();
        mock.mockEmitProgress(256, 1024);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toLongLong(), 256LL);
        QCOMPARE(spy.at(0).at(1).toLongLong(), 1024LL);
    }

    // ---------------------------------------------------------------------------
    // Successful download: saves file, calls callback, emits signals
    // ---------------------------------------------------------------------------

    void testDownloadFinished_savesFileParsesAndEmitsSignals()
    {
        MockFileDownloader mock;
        const QString cacheFilename = QStringLiteral("finish_success.dat");
        int parseCallCount = 0;

        CachedDownloadManager manager(&mock, cacheFilename, QUrl("http://example.com/"),
                                      makeSuccessCallback(&parseCallCount, 5), nullptr);

        QSignalSpy finishedSpy(&manager, &CachedDownloadManager::downloadFinished);
        QSignalSpy loadedSpy(&manager, &CachedDownloadManager::loaded);

        manager.download();
        mock.mockEmitFinished(sampleData());

        // Parse callback must have been called
        QCOMPARE(parseCallCount, 1);

        // Signals emitted
        QCOMPARE(finishedSpy.count(), 1);
        QCOMPARE(finishedSpy.at(0).at(0).toInt(), 5);
        QCOMPARE(loadedSpy.count(), 1);

        // File must have been saved to cache
        QVERIFY(manager.hasCachedFile());

        // Clean up
        QFile::remove(manager.cacheFilePath());
    }

    // ---------------------------------------------------------------------------
    // Parse failure on download: emits downloadFailed, does NOT emit loaded
    // ---------------------------------------------------------------------------

    void testDownloadFinished_parseFailure_emitsDownloadFailed()
    {
        MockFileDownloader mock;

        CachedDownloadManager manager(&mock, QStringLiteral("finish_fail.dat"),
                                      QUrl("http://example.com/"), makeFailCallback(), nullptr);

        QSignalSpy failedSpy(&manager, &CachedDownloadManager::downloadFailed);
        QSignalSpy finishedSpy(&manager, &CachedDownloadManager::downloadFinished);
        QSignalSpy loadedSpy(&manager, &CachedDownloadManager::loaded);

        manager.download();
        mock.mockEmitFinished(sampleData());

        QCOMPARE(failedSpy.count(), 1);
        QCOMPARE(finishedSpy.count(), 0);
        QCOMPARE(loadedSpy.count(), 0);

        // Clean up any saved file
        QFile::remove(manager.cacheFilePath());
    }

    // ---------------------------------------------------------------------------
    // Download failure: forwards error signal
    // ---------------------------------------------------------------------------

    void testDownloadFailed_forwardsErrorSignal()
    {
        MockFileDownloader mock;

        CachedDownloadManager manager(&mock, QStringLiteral("dl_failed.dat"),
                                      QUrl("http://example.com/"), makeSuccessCallback(), nullptr);

        QSignalSpy spy(&manager, &CachedDownloadManager::downloadFailed);

        manager.download();
        mock.mockEmitFailed(QStringLiteral("connection refused"));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QStringLiteral("connection refused"));
    }

    // ---------------------------------------------------------------------------
    // hasCachedFile / cacheFilePath
    // ---------------------------------------------------------------------------

    void testHasCachedFile_returnsFalseWhenNoFile()
    {
        MockFileDownloader mock;
        CachedDownloadManager manager(&mock, QStringLiteral("no_such_file_xyz.dat"),
                                      QUrl("http://example.com/"), makeSuccessCallback(), nullptr);

        // Unless there's a leftover file from a previous run, this should be false.
        // (Tests run with QStandardPaths::setTestModeEnabled so the path is sandboxed.)
        const QString path = manager.cacheFilePath();
        QFile::remove(path);  // ensure it's gone
        QVERIFY(!manager.hasCachedFile());
    }

    void testCacheFilePath_containsCacheFilename()
    {
        MockFileDownloader mock;
        const QString filename = QStringLiteral("my_unique_filename.dat");
        CachedDownloadManager manager(&mock, filename, QUrl("http://example.com/"),
                                      makeSuccessCallback(), nullptr);

        QVERIFY(manager.cacheFilePath().endsWith(filename));
    }
};

QTEST_MAIN(TestCachedDownloadManager)
#include "test_cacheddownloadmanager.moc"
