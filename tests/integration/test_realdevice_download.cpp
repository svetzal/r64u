/**
 * Integration test for multi-folder download against a real Ultimate64/Ultimate-II+ device.
 *
 * This test requires:
 * - A real device connected to the network
 * - The DEVICE_HOST environment variable set to the device IP (default: 192.168.1.137)
 * - TEST_FOLDERS environment variable with comma-separated folder paths to download
 *
 * Run with:
 *   DEVICE_HOST=192.168.1.137 TEST_FOLDERS="/SD/folder1,/SD/folder2" ./test_realdevice_download
 *
 * This test is designed to diagnose the multi-folder download hang issue where transfers
 * get stuck at the same file consistently.
 */

#include <QtTest>
#include <QCoreApplication>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QElapsedTimer>
#include <QTimer>

#include "services/c64uftpclient.h"
#include "models/transferqueue.h"

class RealDeviceDownloadTest : public QObject
{
    Q_OBJECT

private:
    C64UFtpClient *ftpClient_ = nullptr;
    TransferQueue *queue_ = nullptr;
    QTemporaryDir tempDir_;
    QString deviceHost_;
    QStringList testFolders_;

private slots:
    void initTestCase()
    {
        // Get device host from environment or use default
        deviceHost_ = qEnvironmentVariable("DEVICE_HOST", "192.168.1.137");

        // Get test folders from environment
        QString foldersEnv = qEnvironmentVariable("TEST_FOLDERS", "");
        if (!foldersEnv.isEmpty()) {
            testFolders_ = foldersEnv.split(',', Qt::SkipEmptyParts);
        }

        if (testFolders_.isEmpty()) {
            QSKIP("No TEST_FOLDERS specified. Set TEST_FOLDERS environment variable with comma-separated folder paths.");
        }

        qDebug() << "=== Real Device Download Test ===";
        qDebug() << "Device host:" << deviceHost_;
        qDebug() << "Test folders:" << testFolders_;
        qDebug() << "Temp directory:" << tempDir_.path();

        // Create FTP client and transfer queue
        ftpClient_ = new C64UFtpClient(this);
        queue_ = new TransferQueue(this);
        queue_->setFtpClient(ftpClient_);
        queue_->setAutoOverwrite(true);  // Don't prompt for overwrites
        queue_->setAutoMerge(true);      // Don't prompt for folder merge

        // Connect to device
        QSignalSpy connectedSpy(ftpClient_, &C64UFtpClient::connected);
        QSignalSpy connectionErrorSpy(ftpClient_, &C64UFtpClient::error);

        qDebug() << "Connecting to" << deviceHost_ << "...";
        ftpClient_->setHost(deviceHost_);
        ftpClient_->connectToHost();

        // Wait for connection (10 second timeout)
        bool connected = connectedSpy.wait(10000);
        if (!connected) {
            if (!connectionErrorSpy.isEmpty()) {
                qWarning() << "Connection error:" << connectionErrorSpy.first().first().toString();
            }
            QSKIP("Could not connect to device. Make sure it's powered on and accessible.");
        }

        qDebug() << "Connected successfully!";
    }

    void cleanupTestCase()
    {
        if (ftpClient_ && ftpClient_->isConnected()) {
            ftpClient_->disconnect();
        }
        delete queue_;
        delete ftpClient_;
    }

    void testMultipleFolderDownload()
    {
        QVERIFY2(ftpClient_->isConnected(), "FTP client should be connected");

        QSignalSpy allCompletedSpy(queue_, &TransferQueue::allOperationsCompleted);
        QSignalSpy operationStartedSpy(queue_, &TransferQueue::operationStarted);
        QSignalSpy operationCompletedSpy(queue_, &TransferQueue::operationCompleted);
        QSignalSpy operationFailedSpy(queue_, &TransferQueue::operationFailed);
        QSignalSpy errorSpy(ftpClient_, &C64UFtpClient::error);

        QElapsedTimer timer;
        timer.start();

        // Enqueue all test folders for download
        for (const QString &folder : testFolders_) {
            QString folderName = QFileInfo(folder).fileName();
            QString localPath = tempDir_.path() + "/" + folderName;
            qDebug() << "Enqueueing recursive download:" << folder << "->" << localPath;
            queue_->enqueueRecursiveDownload(folder, localPath);
        }

        // Wait for all operations to complete (5 minute timeout)
        const int timeoutMs = 300000;  // 5 minutes

        // Use a timer to periodically check state and provide progress updates
        QTimer progressTimer;
        int lastCompletedCount = 0;
        connect(&progressTimer, &QTimer::timeout, this, [&]() {
            int currentCompleted = operationCompletedSpy.count();
            int currentFailed = operationFailedSpy.count();
            int queueSize = queue_->rowCount();

            if (currentCompleted != lastCompletedCount) {
                qDebug() << "Progress: completed=" << currentCompleted
                         << "failed=" << currentFailed
                         << "queueSize=" << queueSize
                         << "elapsed=" << timer.elapsed() / 1000 << "s";
                lastCompletedCount = currentCompleted;
            }
        });
        progressTimer.start(5000);  // Update every 5 seconds

        bool completed = allCompletedSpy.wait(timeoutMs);
        progressTimer.stop();

        if (!completed) {
            qCritical() << "=== HANG DETECTED ===";
            qCritical() << "Elapsed time:" << timer.elapsed() / 1000 << "seconds";
            qCritical() << "Queue isProcessing:" << queue_->isProcessing();
            qCritical() << "Queue isScanning:" << queue_->isScanning();
            qCritical() << "Items in queue:" << queue_->rowCount();
            qCritical() << "Operations started:" << operationStartedSpy.count();
            qCritical() << "Operations completed:" << operationCompletedSpy.count();
            qCritical() << "Operations failed:" << operationFailedSpy.count();

            // Dump queue contents
            qCritical() << "=== Queue Contents ===";
            for (int i = 0; i < queue_->rowCount(); ++i) {
                QModelIndex idx = queue_->index(i);
                auto status = queue_->data(idx, TransferQueue::StatusRole).toInt();
                auto remotePath = queue_->data(idx, TransferQueue::RemotePathRole).toString();
                auto localPath = queue_->data(idx, TransferQueue::LocalPathRole).toString();
                auto errorMsg = queue_->data(idx, TransferQueue::ErrorMessageRole).toString();

                QString statusStr;
                switch (static_cast<TransferItem::Status>(status)) {
                case TransferItem::Status::Pending: statusStr = "Pending"; break;
                case TransferItem::Status::InProgress: statusStr = "InProgress"; break;
                case TransferItem::Status::Completed: statusStr = "Completed"; break;
                case TransferItem::Status::Failed: statusStr = "Failed"; break;
                default: statusStr = "Unknown"; break;
                }

                qCritical() << "  Item" << i << ":" << statusStr
                           << "remote:" << remotePath
                           << "local:" << localPath;
                if (!errorMsg.isEmpty()) {
                    qCritical() << "    Error:" << errorMsg;
                }
            }

            // Dump any FTP errors
            if (!errorSpy.isEmpty()) {
                qCritical() << "=== FTP Errors ===";
                for (const auto &error : errorSpy) {
                    qCritical() << "  " << error.first().toString();
                }
            }
        }

        QVERIFY2(completed, "All operations should complete within timeout");

        qDebug() << "=== Test Completed Successfully ===";
        qDebug() << "Total time:" << timer.elapsed() / 1000 << "seconds";
        qDebug() << "Operations completed:" << operationCompletedSpy.count();
        qDebug() << "Operations failed:" << operationFailedSpy.count();
    }

    void testSingleFolderMultipleTimes()
    {
        // Test downloading the same folder multiple times to check for reproducibility
        QVERIFY2(ftpClient_->isConnected(), "FTP client should be connected");

        if (testFolders_.isEmpty()) {
            QSKIP("No test folders specified");
        }

        QString testFolder = testFolders_.first();
        const int iterations = 3;

        qDebug() << "Testing" << iterations << "iterations of downloading:" << testFolder;

        for (int i = 0; i < iterations; ++i) {
            qDebug() << "=== Iteration" << (i + 1) << "of" << iterations << "===";

            QSignalSpy allCompletedSpy(queue_, &TransferQueue::allOperationsCompleted);

            QString folderName = QFileInfo(testFolder).fileName();
            QString localPath = tempDir_.path() + "/iter" + QString::number(i) + "/" + folderName;

            QElapsedTimer timer;
            timer.start();

            queue_->enqueueRecursiveDownload(testFolder, localPath);

            bool completed = allCompletedSpy.wait(120000);  // 2 minute timeout per iteration

            if (!completed) {
                qCritical() << "HANG on iteration" << (i + 1);
                qCritical() << "Queue isProcessing:" << queue_->isProcessing();
                qCritical() << "Queue isScanning:" << queue_->isScanning();
                qCritical() << "Items remaining:" << queue_->rowCount();

                // Dump InProgress items
                for (int j = 0; j < queue_->rowCount(); ++j) {
                    QModelIndex idx = queue_->index(j);
                    auto status = queue_->data(idx, TransferQueue::StatusRole).toInt();
                    if (status == static_cast<int>(TransferItem::Status::InProgress)) {
                        auto remotePath = queue_->data(idx, TransferQueue::RemotePathRole).toString();
                        qCritical() << "  STUCK:" << remotePath;
                    }
                }
            }

            QVERIFY2(completed, qPrintable(QString("Iteration %1 should complete").arg(i + 1)));
            qDebug() << "Iteration" << (i + 1) << "completed in" << timer.elapsed() / 1000 << "seconds";

            // Clear queue for next iteration
            queue_->clear();
        }
    }
};

QTEST_MAIN(RealDeviceDownloadTest)
#include "test_realdevice_download.moc"
