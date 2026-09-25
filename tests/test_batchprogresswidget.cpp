/**
 * @file test_batchprogresswidget.cpp
 * @brief Unit tests for BatchProgressWidget state transitions and progress updates.
 *
 * BatchProgressWidget displays progress for a single transfer batch.  These tests
 * verify:
 *
 * - Construction with a valid batchId
 * - batchId() accessor
 * - updateProgress() with scanning state
 * - updateProgress() with active (transfer) state
 * - setState() transitions between visual states
 * - setDescription() / setActive() / setOperationType() do not crash
 * - cancelRequested signal emitted when cancel button clicked
 */

#include "models/transferqueue.h"
#include "ui/batchprogresswidget.h"

#include <QLabel>
#include <QLocale>
#include <QProgressBar>
#include <QPushButton>
#include <QSignalSpy>
#include <QtTest>

namespace {

constexpr qint64 MiB = qint64{1024} * 1024;

BatchProgress activeTransfer(int totalItems, int completedItems, int permilleDone, qint64 bytesDone,
                             qint64 bytesTotal)
{
    BatchProgress progress;
    progress.batchId = 1;
    progress.operationType = OperationType::Upload;
    progress.totalItems = totalItems;
    progress.completedItems = completedItems;
    progress.permilleDone = permilleDone;
    progress.bytesDone = bytesDone;
    progress.bytesTotal = bytesTotal;
    return progress;
}

/// Text of the status label (the widest of the widget's labels; the other is the icon).
QString labelText(const BatchProgressWidget &widget)
{
    QString longest;
    for (const QLabel *label : widget.findChildren<QLabel *>()) {
        if (label->text().size() > longest.size()) {
            longest = label->text();
        }
    }
    return longest;
}

}  // namespace

class TestBatchProgressWidget : public QObject
{
    Q_OBJECT

private slots:

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        BatchProgressWidget widget(42);
        QVERIFY(true);
    }

    // =========================================================================
    // batchId() accessor
    // =========================================================================

    void testBatchId_ReturnsConstructorValue()
    {
        BatchProgressWidget widget(99);
        QCOMPARE(widget.batchId(), 99);
    }

    // =========================================================================
    // setState() — transitions
    // =========================================================================

    void testSetState_Queued_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Queued);
        QVERIFY(true);
    }

    void testSetState_Scanning_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Scanning);
        QVERIFY(true);
    }

    void testSetState_Active_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Active);
        QVERIFY(true);
    }

    void testSetState_Completed_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Completed);
        QVERIFY(true);
    }

    void testSetState_Creating_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Creating);
        QVERIFY(true);
    }

    void testSetState_SameStateTwice_NoOp()
    {
        // Setting the same state twice should be a no-op (no crash)
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Active);
        widget.setState(BatchProgressWidget::State::Active);
        QVERIFY(true);
    }

    // =========================================================================
    // Scanning state — progress bar is indeterminate
    // =========================================================================

    void testSetState_Scanning_ProgressBarIndeterminate()
    {
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Scanning);

        auto *bar = widget.findChild<QProgressBar *>();
        QVERIFY(bar != nullptr);
        // isHidden() tests the widget's own visibility flag independent of parent visibility
        QVERIFY(!bar->isHidden());
        QCOMPARE(bar->maximum(), 0);  // Indeterminate
    }

    // =========================================================================
    // Active state — progress bar visible with determinate range
    // =========================================================================

    void testSetState_Active_ProgressBarDeterminate()
    {
        BatchProgressWidget widget(1);
        widget.setState(BatchProgressWidget::State::Active);

        auto *bar = widget.findChild<QProgressBar *>();
        QVERIFY(bar != nullptr);
        // isHidden() tests the widget's own visibility flag independent of parent visibility
        QVERIFY(!bar->isHidden());
        QCOMPARE(bar->maximum(), 100);
    }

    // =========================================================================
    // updateProgress() — scanning state
    // =========================================================================

    void testUpdateProgress_Scanning_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        BatchProgress progress;
        progress.batchId = 1;
        progress.isScanning = true;
        progress.directoriesScanned = 3;
        progress.filesDiscovered = 10;
        progress.operationType = OperationType::Download;

        widget.updateProgress(progress);
        QVERIFY(true);
    }

    // =========================================================================
    // updateProgress() — active transfer state
    // =========================================================================

    void testUpdateProgress_ActiveTransfer_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        BatchProgress progress;
        progress.batchId = 1;
        progress.isScanning = false;
        progress.totalItems = 10;
        progress.completedItems = 5;
        progress.failedItems = 0;
        progress.operationType = OperationType::Download;

        widget.updateProgress(progress);
        QVERIFY(true);
    }

    void testUpdateProgress_AllComplete_ProgressIs100()
    {
        BatchProgressWidget widget(1);
        BatchProgress progress;
        progress.batchId = 1;
        progress.isScanning = false;
        progress.totalItems = 5;
        progress.completedItems = 5;
        progress.failedItems = 0;
        progress.permilleDone = 1000;
        progress.operationType = OperationType::Upload;

        widget.updateProgress(progress);

        auto *bar = widget.findChild<QProgressBar *>();
        QVERIFY(bar != nullptr);
        QCOMPARE(bar->value(), 100);
    }

    // =========================================================================
    // updateProgress() — the bar follows bytes, not file count
    // =========================================================================

    void testUpdateProgress_LargeFileInFlight_BarFollowsItsBytes()
    {
        BatchProgressWidget widget(1);

        widget.updateProgress(activeTransfer(1, 0, 250, 4 * MiB, 16 * MiB));

        QCOMPARE(widget.findChild<QProgressBar *>()->value(), 25);
    }

    void testUpdateProgress_LessWorkDoneThanShown_BarDoesNotGoBack()
    {
        BatchProgressWidget widget(1);
        widget.updateProgress(activeTransfer(2, 1, 600, 0, 0));

        // e.g. a file's size became known and outweighs the files already done
        widget.updateProgress(activeTransfer(2, 1, 400, 0, 0));

        QCOMPARE(widget.findChild<QProgressBar *>()->value(), 60);
    }

    void testUpdateProgress_SizesKnown_LabelShowsBytesAndFileCount()
    {
        QLocale::setDefault(QLocale::c());
        BatchProgressWidget widget(1);

        widget.updateProgress(activeTransfer(1, 0, 250, 4 * MiB, 16 * MiB));

        const QString text = labelText(widget);
        QVERIFY2(text.contains("1 of 1"), qPrintable(text));
        QVERIFY2(text.contains("4.0 MB of 16.0 MB"), qPrintable(text));
    }

    void testUpdateProgress_SizesUnknown_LabelShowsFileCountOnly()
    {
        QLocale::setDefault(QLocale::c());
        BatchProgressWidget widget(1);

        widget.updateProgress(activeTransfer(3, 1, 333, 0, 0));

        const QString text = labelText(widget);
        QVERIFY2(text.contains("2 of 3"), qPrintable(text));
        QVERIFY2(!text.contains("MB"), qPrintable(text));
    }

    // =========================================================================
    // setDescription() — does not crash
    // =========================================================================

    void testSetDescription_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setDescription("Downloading folder1");
        QVERIFY(true);
    }

    // =========================================================================
    // setActive() — does not crash
    // =========================================================================

    void testSetActive_True_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setActive(true);
        QVERIFY(true);
    }

    void testSetActive_False_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setActive(false);
        QVERIFY(true);
    }

    // =========================================================================
    // setOperationType() — does not crash
    // =========================================================================

    void testSetOperationType_Download_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setOperationType(OperationType::Download);
        QVERIFY(true);
    }

    void testSetOperationType_Upload_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setOperationType(OperationType::Upload);
        QVERIFY(true);
    }

    void testSetOperationType_Delete_DoesNotCrash()
    {
        BatchProgressWidget widget(1);
        widget.setOperationType(OperationType::Delete);
        QVERIFY(true);
    }

    // =========================================================================
    // cancelRequested signal — emitted when cancel button clicked
    // =========================================================================

    void testCancelRequested_EmitsSignalWithBatchId()
    {
        BatchProgressWidget widget(42);
        QSignalSpy spy(&widget, &BatchProgressWidget::cancelRequested);

        auto *cancelButton = widget.findChild<QPushButton *>();
        QVERIFY(cancelButton != nullptr);
        cancelButton->click();

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 42);
    }
};

QTEST_MAIN(TestBatchProgressWidget)
#include "test_batchprogresswidget.moc"
