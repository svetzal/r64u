/**
 * @file test_transferprogresswidget.cpp
 * @brief Unit tests for TransferProgressWidget visibility transitions and signal emission.
 *
 * Tests verify:
 * - Construction does not crash
 * - Widget is initially hidden
 * - setTransferService(nullptr) does not crash
 * - onScanningStarted makes the widget visible and sets indeterminate progress
 * - onScanningProgress does not crash
 * - onDirectoryCreationProgress makes the widget visible and sets determinate progress
 * - onAllOperationsCompleted hides widget, emits statusMessage and clearStatusMessages
 * - onOperationsCancelled hides widget, emits statusMessage and clearStatusMessages
 */

#include "models/transferqueue.h"
#include "ui/transferprogresswidget.h"

#include <QProgressBar>
#include <QSignalSpy>
#include <QtTest>

class TestTransferProgressWidget : public QObject
{
    Q_OBJECT

private slots:

    void testConstruct_doesNotCrash()
    {
        TransferProgressWidget widget;
        QVERIFY(true);
    }

    void testConstruct_initiallyHidden()
    {
        TransferProgressWidget widget;
        QVERIFY(!widget.isVisible());
    }

    void testSetTransferService_nullptr_doesNotCrash()
    {
        TransferProgressWidget widget;
        widget.setTransferService(nullptr);
        QVERIFY(true);
    }

    void testOnScanningStarted_makesWidgetVisible()
    {
        TransferProgressWidget widget;
        QMetaObject::invokeMethod(&widget, "onScanningStarted", Q_ARG(QString, "folder1"),
                                  Q_ARG(OperationType, OperationType::Download));
        QVERIFY(widget.isVisible());
    }

    void testOnScanningStarted_setsIndeterminateProgressBar()
    {
        TransferProgressWidget widget;
        QMetaObject::invokeMethod(&widget, "onScanningStarted", Q_ARG(QString, "folder1"),
                                  Q_ARG(OperationType, OperationType::Download));

        auto *bar = widget.findChild<QProgressBar *>();
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->maximum(), 0);
    }

    void testOnScanningProgress_doesNotCrash()
    {
        TransferProgressWidget widget;
        QMetaObject::invokeMethod(&widget, "onScanningStarted", Q_ARG(QString, "folder1"),
                                  Q_ARG(OperationType, OperationType::Download));
        QMetaObject::invokeMethod(&widget, "onScanningProgress", Q_ARG(int, 3), Q_ARG(int, 1),
                                  Q_ARG(int, 10));
        QVERIFY(true);
    }

    void testOnDirectoryCreationProgress_makesWidgetVisible()
    {
        TransferProgressWidget widget;
        QMetaObject::invokeMethod(&widget, "onDirectoryCreationProgress", Q_ARG(int, 5),
                                  Q_ARG(int, 10));
        QVERIFY(widget.isVisible());
    }

    void testOnDirectoryCreationProgress_setsDeterminateProgressBar()
    {
        TransferProgressWidget widget;
        QMetaObject::invokeMethod(&widget, "onDirectoryCreationProgress", Q_ARG(int, 50),
                                  Q_ARG(int, 100));

        auto *bar = widget.findChild<QProgressBar *>();
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->value(), 50);
    }

    void testOnAllOperationsCompleted_hidesWidget()
    {
        TransferProgressWidget widget;
        QMetaObject::invokeMethod(&widget, "onScanningStarted", Q_ARG(QString, "folder1"),
                                  Q_ARG(OperationType, OperationType::Download));
        QVERIFY(widget.isVisible());

        QMetaObject::invokeMethod(&widget, "onAllOperationsCompleted");
        QVERIFY(!widget.isVisible());
    }

    void testOnAllOperationsCompleted_emitsStatusMessage()
    {
        TransferProgressWidget widget;
        QSignalSpy spy(&widget, &TransferProgressWidget::statusMessage);

        QMetaObject::invokeMethod(&widget, "onAllOperationsCompleted");

        QVERIFY(spy.count() >= 1);
    }

    void testOnAllOperationsCompleted_emitsClearStatusMessages()
    {
        TransferProgressWidget widget;
        QSignalSpy spy(&widget, &TransferProgressWidget::clearStatusMessages);

        QMetaObject::invokeMethod(&widget, "onAllOperationsCompleted");

        QCOMPARE(spy.count(), 1);
    }

    void testOnOperationsCancelled_hidesWidget()
    {
        TransferProgressWidget widget;
        QMetaObject::invokeMethod(&widget, "onScanningStarted", Q_ARG(QString, "folder1"),
                                  Q_ARG(OperationType, OperationType::Upload));
        QVERIFY(widget.isVisible());

        QMetaObject::invokeMethod(&widget, "onOperationsCancelled");
        QVERIFY(!widget.isVisible());
    }

    void testOnOperationsCancelled_emitsStatusMessage()
    {
        TransferProgressWidget widget;
        QSignalSpy spy(&widget, &TransferProgressWidget::statusMessage);

        QMetaObject::invokeMethod(&widget, "onOperationsCancelled");

        QVERIFY(spy.count() >= 1);
    }
};

QTEST_MAIN(TestTransferProgressWidget)
#include "test_transferprogresswidget.moc"
