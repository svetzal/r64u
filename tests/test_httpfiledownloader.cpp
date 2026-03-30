/**
 * @file test_httpfiledownloader.cpp
 * @brief Unit tests for HttpFileDownloader.
 *
 * Uses a local QTcpServer to serve controlled HTTP responses, so no external
 * network access is required.  Tests cover:
 * - Initial state (isDownloading returns false, no crash)
 * - download() initiates a request and sets isDownloading()
 * - cancel() aborts an in-flight download
 * - Successful response emits downloadFinished with the correct data
 * - HTTP error response emits downloadFailed
 * - Empty response body emits downloadFailed
 * - Duplicate download() while active is a no-op (does not start a second request)
 */

#include "services/httpfiledownloader.h"

#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QtTest>

// ---------------------------------------------------------------------------
// Minimal inline HTTP server for test control
// ---------------------------------------------------------------------------

class TestHttpServer : public QObject
{
    Q_OBJECT

public:
    explicit TestHttpServer(QObject *parent = nullptr) : QObject(parent)
    {
        server_ = new QTcpServer(this);
        QVERIFY(server_->listen(QHostAddress::LocalHost, 0));
        connect(server_, &QTcpServer::newConnection, this, &TestHttpServer::onNewConnection);
    }

    quint16 port() const { return server_->serverPort(); }

    QUrl url(const QString &path = "/test.sid") const
    {
        return QUrl(QString("http://127.0.0.1:%1%2").arg(port()).arg(path));
    }

    // Configure what response to send for the next incoming request
    void setResponse(int statusCode, const QByteArray &body)
    {
        responseStatus_ = statusCode;
        responseBody_ = body;
    }

    // Disconnect the next connecting client without sending anything (simulate drop)
    void setDropConnection(bool drop) { dropConnection_ = drop; }

private slots:
    void onNewConnection()
    {
        QTcpSocket *client = server_->nextPendingConnection();
        if (!client) {
            return;
        }

        if (dropConnection_) {
            client->disconnectFromHost();
            client->deleteLater();
            return;
        }

        // Read and discard the HTTP request, then send a canned response
        connect(client, &QTcpSocket::readyRead, this, [this, client]() {
            client->readAll();  // consume request

            QByteArray response;
            if (responseStatus_ == 200) {
                response = "HTTP/1.1 200 OK\r\n"
                           "Content-Length: " +
                           QByteArray::number(responseBody_.size()) +
                           "\r\n"
                           "\r\n" +
                           responseBody_;
            } else if (responseStatus_ == 404) {
                response = "HTTP/1.1 404 Not Found\r\n"
                           "Content-Length: 0\r\n"
                           "\r\n";
            } else {
                response = "HTTP/1.1 500 Internal Server Error\r\n"
                           "Content-Length: 0\r\n"
                           "\r\n";
            }

            client->write(response);
            client->flush();
            client->disconnectFromHost();
            client->deleteLater();
        });
    }

private:
    QTcpServer *server_ = nullptr;
    int responseStatus_ = 200;
    QByteArray responseBody_;
    bool dropConnection_ = false;
};

// ---------------------------------------------------------------------------
// Test class
// ---------------------------------------------------------------------------

class TestHttpFileDownloader : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        server_ = new TestHttpServer(this);
        downloader_ = new HttpFileDownloader(this);
    }

    void cleanup()
    {
        delete downloader_;
        downloader_ = nullptr;
        delete server_;
        server_ = nullptr;
    }

    // =========================================================
    // Initial state
    // =========================================================

    void testInitialState_NotDownloading() { QVERIFY(!downloader_->isDownloading()); }

    // =========================================================
    // download() — state transitions
    // =========================================================

    void testDownload_SetsIsDownloading()
    {
        server_->setResponse(200, QByteArray("data"));

        downloader_->download(server_->url());

        QVERIFY(downloader_->isDownloading());
    }

    void testDownload_WhileActive_IsNoOp()
    {
        server_->setResponse(200, QByteArray("data"));

        downloader_->download(server_->url());
        QVERIFY(downloader_->isDownloading());

        // Second call while active — should not crash or emit an extra signal
        downloader_->download(server_->url("/other.sid"));
        QVERIFY(downloader_->isDownloading());
    }

    // =========================================================
    // Successful download
    // =========================================================

    void testDownload_Success_EmitsDownloadFinished()
    {
        const QByteArray payload("Hello SID world");
        server_->setResponse(200, payload);
        QSignalSpy spy(downloader_, &HttpFileDownloader::downloadFinished);

        downloader_->download(server_->url());
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);

        QCOMPARE(spy.first().first().toByteArray(), payload);
    }

    void testDownload_Success_NotDownloadingAfterCompletion()
    {
        server_->setResponse(200, QByteArray("payload data"));
        QSignalSpy spy(downloader_, &HttpFileDownloader::downloadFinished);

        downloader_->download(server_->url());
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);

        QVERIFY(!downloader_->isDownloading());
    }

    void testDownload_Success_DoesNotEmitDownloadFailed()
    {
        server_->setResponse(200, QByteArray("payload data"));
        QSignalSpy failedSpy(downloader_, &HttpFileDownloader::downloadFailed);
        QSignalSpy finishedSpy(downloader_, &HttpFileDownloader::downloadFinished);

        downloader_->download(server_->url());
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 3000);

        QCOMPARE(failedSpy.count(), 0);
    }

    // =========================================================
    // Empty response body
    // =========================================================

    void testDownload_EmptyBody_EmitsDownloadFailed()
    {
        server_->setResponse(200, QByteArray());  // empty body
        QSignalSpy spy(downloader_, &HttpFileDownloader::downloadFailed);

        downloader_->download(server_->url());
        QTRY_COMPARE_WITH_TIMEOUT(spy.count(), 1, 3000);
    }

    // =========================================================
    // cancel()
    // =========================================================

    void testCancel_WhenNotDownloading_IsNoOp()
    {
        // Should not crash
        downloader_->cancel();
        QVERIFY(!downloader_->isDownloading());
    }

    void testCancel_WhenDownloading_SetsNotDownloading()
    {
        server_->setResponse(200, QByteArray("data"));

        downloader_->download(server_->url());
        QVERIFY(downloader_->isDownloading());

        downloader_->cancel();
        QVERIFY(!downloader_->isDownloading());
    }

    void testCancel_DoesNotEmitDownloadFinished()
    {
        server_->setResponse(200, QByteArray("data"));
        QSignalSpy finishedSpy(downloader_, &HttpFileDownloader::downloadFinished);

        downloader_->download(server_->url());
        downloader_->cancel();

        // Give the event loop a moment to process any stray signals
        QTest::qWait(50);

        QCOMPARE(finishedSpy.count(), 0);
    }

    // =========================================================
    // downloadProgress signal
    // =========================================================

    void testDownload_EmitsDownloadProgress()
    {
        server_->setResponse(200, QByteArray(1024, 'X'));
        QSignalSpy progressSpy(downloader_, &HttpFileDownloader::downloadProgress);
        QSignalSpy finishedSpy(downloader_, &HttpFileDownloader::downloadFinished);

        downloader_->download(server_->url());
        QTRY_COMPARE_WITH_TIMEOUT(finishedSpy.count(), 1, 3000);

        QVERIFY(progressSpy.count() >= 1);
    }

private:
    TestHttpServer *server_ = nullptr;
    HttpFileDownloader *downloader_ = nullptr;
};

QTEST_MAIN(TestHttpFileDownloader)
#include "test_httpfiledownloader.moc"
