#include "models/transfertimeoutmanager.h"

#include <QSignalSpy>
#include <QtTest>

class TestTransferTimeoutManager : public QObject
{
    Q_OBJECT

private slots:
    void testStartDoesNotEmitImmediately()
    {
        TransferTimeoutManager mgr(50);
        QSignalSpy spy(&mgr, &TransferTimeoutManager::operationTimedOut);
        mgr.start();
        QCOMPARE(spy.count(), 0);
    }

    void testStopCancelsTimer()
    {
        TransferTimeoutManager mgr(50);
        QSignalSpy spy(&mgr, &TransferTimeoutManager::operationTimedOut);
        mgr.start();
        mgr.stop();
        QTest::qWait(150);
        QCOMPARE(spy.count(), 0);
    }

    void testTimerExpiryEmitsSignal()
    {
        TransferTimeoutManager mgr(50);
        QSignalSpy spy(&mgr, &TransferTimeoutManager::operationTimedOut);
        mgr.start();
        QVERIFY(spy.wait(500));
        QCOMPARE(spy.count(), 1);
    }

    void testStartTwiceResetsTimer()
    {
        TransferTimeoutManager mgr(200);
        QSignalSpy spy(&mgr, &TransferTimeoutManager::operationTimedOut);
        mgr.start();
        QTest::qWait(100);
        mgr.start();
        QTest::qWait(150);
        QCOMPARE(spy.count(), 0);
        QVERIFY(spy.wait(500));
        QCOMPARE(spy.count(), 1);
    }
};

QTEST_MAIN(TestTransferTimeoutManager)
#include "test_transfertimeoutmanager.moc"
