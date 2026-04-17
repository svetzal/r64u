#include "models/transfereventprocessor.h"

#include <QtTest>

class TestTransferEventProcessor : public QObject
{
    Q_OBJECT

private slots:
    void testScheduleDoesNotExecuteSynchronously()
    {
        TransferEventProcessor processor;
        bool ran = false;
        processor.schedule([&ran]() { ran = true; });
        QVERIFY(!ran);
    }

    void testFlushDrainsQueue()
    {
        TransferEventProcessor processor;
        bool ran = false;
        processor.schedule([&ran]() { ran = true; });
        processor.flush();
        QVERIFY(ran);
    }

    void testFlushExecutesInFifoOrder()
    {
        TransferEventProcessor processor;
        QStringList order;
        processor.schedule([&order]() { order.append("first"); });
        processor.schedule([&order]() { order.append("second"); });
        processor.schedule([&order]() { order.append("third"); });
        processor.flush();
        QCOMPARE(order, QStringList({"first", "second", "third"}));
    }

    void testFlushIsNoOpWhenEmpty()
    {
        TransferEventProcessor processor;
        processor.flush();
    }

    void testFlushIsReentrantSafe()
    {
        TransferEventProcessor processor;
        int outerRan = 0;
        processor.schedule([&processor, &outerRan]() {
            ++outerRan;
            processor.flush();
        });
        processor.flush();
        QCOMPARE(outerRan, 1);
    }

    void testScheduleWhileFlushing()
    {
        TransferEventProcessor processor;
        bool aRan = false;
        bool bRan = false;
        processor.schedule([&processor, &aRan, &bRan]() {
            aRan = true;
            processor.schedule([&bRan]() { bRan = true; });
        });
        processor.flush();
        QVERIFY(aRan);
        QVERIFY(bRan);
    }
};

QTEST_MAIN(TestTransferEventProcessor)
#include "test_transfereventprocessor.moc"
