#include "../src/ui/databasedownloadcontroller.h"
#include "../src/ui/imessagepresenter.h"

#include <QtTest>

// ============================================================
// Mock IMessagePresenter that records calls
// ============================================================

struct MessageCall
{
    QString kind;  ///< "info" or "warning"
    QString title;
    QString message;
};

class MockMessagePresenter : public IMessagePresenter
{
public:
    void showInfo(QWidget * /*parent*/, const QString &title, const QString &message) override
    {
        calls.push_back({"info", title, message});
    }

    void showWarning(QWidget * /*parent*/, const QString &title, const QString &message) override
    {
        calls.push_back({"warning", title, message});
    }

    QVector<MessageCall> calls;
};

// ============================================================
// Test fixture
// ============================================================

class TestDatabaseDownloadController : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        controller = new DatabaseDownloadController(nullptr, nullptr);

        // createWidgets() must be called before signals can route to slots
        controller->createWidgets();

        auto *raw = new MockMessagePresenter();
        mock = raw;
        controller->setMessagePresenter(std::unique_ptr<IMessagePresenter>(raw));
    }

    void cleanup()
    {
        delete controller;
        controller = nullptr;
        mock = nullptr;  // deleted by controller via unique_ptr
    }

    void testDownloadFinished_emitsShowInfo()
    {
        // Simulate SonglengthsDatabase emitting downloadFinished
        // We invoke the private slot directly via Qt's signal/slot mechanism
        // by calling the slot directly on the controller.
        QMetaObject::invokeMethod(controller, "onDatabaseDownloadFinished", Qt::DirectConnection,
                                  Q_ARG(int, 42));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("info"));
        QVERIFY(mock->calls[0].title.contains("Download Complete") ||
                mock->calls[0].title == "Download Complete");
        QVERIFY(mock->calls[0].message.contains("42"));
    }

    void testDownloadFailed_emitsShowWarning()
    {
        QMetaObject::invokeMethod(controller, "onDatabaseDownloadFailed", Qt::DirectConnection,
                                  Q_ARG(QString, QString("Network error")));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
        QCOMPARE(mock->calls[0].title, QString("Download Failed"));
        QVERIFY(mock->calls[0].message.contains("Network error"));
    }

    void testStilDownloadFinished_emitsShowInfo()
    {
        QMetaObject::invokeMethod(controller, "onStilDownloadFinished", Qt::DirectConnection,
                                  Q_ARG(int, 1000));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("info"));
        QVERIFY(mock->calls[0].message.contains("1000"));
    }

    void testStilDownloadFailed_emitsShowWarning()
    {
        QMetaObject::invokeMethod(controller, "onStilDownloadFailed", Qt::DirectConnection,
                                  Q_ARG(QString, QString("Timeout")));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
        QVERIFY(mock->calls[0].message.contains("Timeout"));
    }

    void testBuglistDownloadFinished_emitsShowInfo()
    {
        QMetaObject::invokeMethod(controller, "onBuglistDownloadFinished", Qt::DirectConnection,
                                  Q_ARG(int, 500));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("info"));
        QVERIFY(mock->calls[0].message.contains("500"));
    }

    void testBuglistDownloadFailed_emitsShowWarning()
    {
        QMetaObject::invokeMethod(controller, "onBuglistDownloadFailed", Qt::DirectConnection,
                                  Q_ARG(QString, QString("404 Not Found")));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
        QVERIFY(mock->calls[0].message.contains("404 Not Found"));
    }

    void testGameBase64DownloadFinished_emitsShowInfo()
    {
        QMetaObject::invokeMethod(controller, "onGameBase64DownloadFinished", Qt::DirectConnection,
                                  Q_ARG(int, 29000));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("info"));
        QVERIFY(mock->calls[0].message.contains("29000"));
    }

    void testGameBase64DownloadFailed_emitsShowWarning()
    {
        QMetaObject::invokeMethod(controller, "onGameBase64DownloadFailed", Qt::DirectConnection,
                                  Q_ARG(QString, QString("Parse error")));

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
        QVERIFY(mock->calls[0].message.contains("Parse error"));
    }

    void testDownloadDatabase_withNullService_showsWarning()
    {
        // No service injected — should warn the user
        QMetaObject::invokeMethod(controller, "onDownloadDatabase", Qt::DirectConnection);

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
    }

    void testDownloadStil_withNullService_showsWarning()
    {
        QMetaObject::invokeMethod(controller, "onDownloadStil", Qt::DirectConnection);

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
    }

    void testDownloadBuglist_withNullService_showsWarning()
    {
        QMetaObject::invokeMethod(controller, "onDownloadBuglist", Qt::DirectConnection);

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
    }

    void testDownloadGameBase64_withNullService_showsWarning()
    {
        QMetaObject::invokeMethod(controller, "onDownloadGameBase64", Qt::DirectConnection);

        QCOMPARE(mock->calls.size(), 1);
        QCOMPARE(mock->calls[0].kind, QString("warning"));
    }

private:
    DatabaseDownloadController *controller = nullptr;
    MockMessagePresenter *mock = nullptr;  ///< Non-owning; owned by controller
};

QTEST_MAIN(TestDatabaseDownloadController)
#include "test_databasedownloadcontroller.moc"
