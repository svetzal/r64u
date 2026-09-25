/**
 * @file test_ftperrorreporting.cpp
 * @brief How often the user hears about an FTP failure, end to end.
 *
 * The real C64UFtpClient talks to a FakeFtpServer and shares the client between
 * the components that make requests of it, all registered with ErrorHandler the
 * way the application wires them (ErrorHandler::connectSources). A failed
 * request is reported by the component that made it; a failed connection by the
 * client. Either way the user is told exactly once.
 */

#include "fakes/fakeftpserver.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/c64uftpclient.h"
#include "services/errorhandler.h"
#include "services/filepreviewservice.h"
#include "services/transferservice.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

namespace {

constexpr int SignalTimeoutMs = 5000;

/// Grace period for reports that must NOT arrive (duplicates).
void letLateSignalsArrive()
{
    QTest::qWait(150);
}

QString describeReports(const QSignalSpy &reportSpy)
{
    QStringList reports;
    for (const auto &args : reportSpy) {
        reports << QString("%1: %2").arg(args.at(2).toString(), args.at(3).toString());
    }
    return QString("reports: [%1]").arg(reports.join(" | "));
}

}  // namespace

class TestFtpErrorReporting : public QObject
{
    Q_OBJECT

private:
    FakeFtpServer *server_ = nullptr;
    C64UFtpClient *ftp_ = nullptr;
    ErrorHandler *handler_ = nullptr;
    RemoteFileModel *remoteModel_ = nullptr;
    FilePreviewService *previewService_ = nullptr;
    TransferQueue *transferQueue_ = nullptr;
    TransferService *transferService_ = nullptr;

    void startLogIn(const QString &password)
    {
        ftp_->setHost("127.0.0.1", server_->port());
        ftp_->setCredentials("user", password);
        ftp_->connectToHost();
    }

    [[nodiscard]] bool logIn()
    {
        startLogIn("pass");
        return QTest::qWaitFor([this]() { return ftp_->state() == IFtpClient::State::Ready; },
                               SignalTimeoutMs);
    }

private slots:
    void init()
    {
        server_ = new FakeFtpServer(this);
        QVERIFY(server_->listen());
        ftp_ = new C64UFtpClient(this);
        handler_ = new ErrorHandler(nullptr, this);

        remoteModel_ = new RemoteFileModel(this);
        remoteModel_->setFtpClient(ftp_);
        previewService_ = new FilePreviewService(ftp_, this);
        transferQueue_ = new TransferQueue(this);
        transferQueue_->setFtpClient(ftp_);
        transferService_ = new TransferService(nullptr, transferQueue_, this);

        handler_->connectSources(nullptr, nullptr, remoteModel_, ftp_, previewService_, nullptr,
                                 transferService_, nullptr, nullptr, nullptr, nullptr, nullptr,
                                 nullptr, nullptr, nullptr, nullptr);
    }

    void cleanup()
    {
        delete transferService_;
        delete transferQueue_;
        delete previewService_;
        delete remoteModel_;
        delete handler_;
        delete ftp_;
        delete server_;
    }

    // === A failed request: its requester tells the user ===

    void testFailedTransfer_IsReportedOnce()
    {
        QVERIFY(logIn());
        QTemporaryDir dir;
        QSignalSpy reports(handler_, &ErrorHandler::errorLogged);  // what the user is told

        transferQueue_->enqueueDownload("/SD/missing.prg", dir.filePath("missing.prg"));

        QTRY_COMPARE_WITH_TIMEOUT(reports.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(reports.count() == 1, qPrintable(describeReports(reports)));
        QVERIFY(reports.first().at(2).toString().contains("missing.prg"));
    }

    void testFailedPreview_IsReportedOnce()
    {
        QVERIFY(logIn());
        QSignalSpy reports(handler_, &ErrorHandler::errorLogged);  // what the user is told

        previewService_->requestPreview("/SD/missing.sid");

        QTRY_COMPARE_WITH_TIMEOUT(reports.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(reports.count() == 1, qPrintable(describeReports(reports)));
        QVERIFY(reports.first().at(2).toString().contains("missing.sid"));
    }

    void testFailedRemoteBrowserListing_IsReportedOnce()
    {
        server_->addReply("LIST", "550 No such directory\r\n");
        QVERIFY(logIn());
        remoteModel_->setRootPath("/SD/gone");
        QSignalSpy reports(handler_, &ErrorHandler::errorLogged);  // what the user is told

        remoteModel_->fetchMore(QModelIndex());

        QTRY_COMPARE_WITH_TIMEOUT(reports.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(reports.count() == 1, qPrintable(describeReports(reports)));
    }

    // === A failed connection: the client tells the user ===

    void testRejectedLogin_IsReportedOnce()
    {
        server_->addReply("PASS", "530 Login incorrect\r\n");
        QSignalSpy reports(handler_, &ErrorHandler::errorLogged);  // what the user is told

        startLogIn("wrong");

        QTRY_COMPARE_WITH_TIMEOUT(reports.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(reports.count() == 1, qPrintable(describeReports(reports)));
        QVERIFY(reports.first().at(3).toString().contains("Login failed"));
    }

    void testDroppedConnection_IsReportedOnce()
    {
        QVERIFY(logIn());
        QSignalSpy reports(handler_, &ErrorHandler::errorLogged);  // what the user is told

        server_->closeClientConnection();

        QTRY_COMPARE_WITH_TIMEOUT(reports.count(), 1, SignalTimeoutMs);
        letLateSignalsArrive();
        QVERIFY2(reports.count() == 1, qPrintable(describeReports(reports)));
    }
};

QTEST_MAIN(TestFtpErrorReporting)
#include "test_ftperrorreporting.moc"
