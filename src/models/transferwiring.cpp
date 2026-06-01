/**
 * @file transferwiring.cpp
 * @brief Signal/slot wiring for TransferManager and its collaborators.
 */

#include "transferwiring.h"

#include "transfermanager.h"

namespace transferwiring {

void connectAll(TransferManager &mgr)
{
    // --- BatchManager ---
    QObject::connect(mgr.batchManager_, &BatchManager::modelAboutToReset, &mgr,
                     &TransferManager::modelAboutToReset);
    QObject::connect(mgr.batchManager_, &BatchManager::modelReset, &mgr,
                     &TransferManager::modelReset);
    QObject::connect(mgr.batchManager_, &BatchManager::rowsAboutToBeRemoved, &mgr,
                     &TransferManager::itemsAboutToBeRemoved);
    QObject::connect(mgr.batchManager_, &BatchManager::rowsRemoved, &mgr,
                     &TransferManager::itemsRemoved);
    QObject::connect(mgr.batchManager_, &BatchManager::stopTimeoutRequested, &mgr,
                     &TransferManager::stopOperationTimeout);
    QObject::connect(mgr.batchManager_, &BatchManager::folderOpCompleteRequested, &mgr,
                     [&mgr]() { mgr.folderCoordinator_->onFolderOperationComplete(); });
    QObject::connect(mgr.batchManager_, &BatchManager::scheduleProcessNextRequested, &mgr,
                     [&mgr]() { mgr.scheduleProcessNext(); });
    QObject::connect(mgr.batchManager_, &BatchManager::batchStarted, &mgr,
                     &TransferManager::batchStarted);
    QObject::connect(mgr.batchManager_, &BatchManager::batchProgressUpdate, &mgr,
                     &TransferManager::batchProgressUpdate);
    QObject::connect(mgr.batchManager_, &BatchManager::batchCompleted, &mgr,
                     &TransferManager::batchCompleted);
    QObject::connect(mgr.batchManager_, &BatchManager::allOperationsCompleted, &mgr,
                     &TransferManager::allOperationsCompleted);
    QObject::connect(mgr.batchManager_, &BatchManager::queueChanged, &mgr,
                     &TransferManager::queueChanged);

    // --- TransferTimeoutManager ---
    QObject::connect(mgr.timeoutManager_, &TransferTimeoutManager::operationTimedOut, &mgr,
                     &TransferManager::onOperationTimeout);

    // --- RecursiveScanCoordinator ---
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::downloadFileDiscovered, &mgr,
                     [&mgr](const QString &remotePath, const QString &localPath, int batchId) {
                         mgr.enqueueDownload(remotePath, localPath, batchId);
                     });
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::completeBatchRequested, &mgr,
                     [&mgr](int batchId) { mgr.completeBatch(batchId); });
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::scheduleProcessNextRequested,
                     &mgr, &TransferManager::onScanCompleted);
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::statusMessage, &mgr,
                     &TransferManager::statusMessage);
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::scanningStarted, &mgr,
                     &TransferManager::scanningStarted);
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::scanningProgress, &mgr,
                     &TransferManager::scanningProgress);
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::deleteScanComplete, &mgr,
                     &TransferManager::onDeleteScanComplete);
    QObject::connect(
        mgr.scanCoordinator_, &RecursiveScanCoordinator::folderCheckComplete, &mgr,
        [&mgr](const QString & /*path*/) { mgr.folderCoordinator_->resumeAfterFolderCheck(); });
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::uploadCheckFileExists, &mgr,
                     [&mgr](const QString &fileName) {
                         emit mgr.overwriteConfirmationNeeded(fileName, OperationType::Upload);
                     });
    QObject::connect(mgr.scanCoordinator_, &RecursiveScanCoordinator::uploadCheckNoConflict, &mgr,
                     [&mgr]() { mgr.scheduleProcessNext(); });

    // --- RemoteDirectoryCoordinator ---
    QObject::connect(mgr.dirCreator_, &RemoteDirectoryCoordinator::directoryCreationProgress, &mgr,
                     &TransferManager::directoryCreationProgress);
    QObject::connect(mgr.dirCreator_, &RemoteDirectoryCoordinator::allDirectoriesCreated, &mgr,
                     [&mgr]() { mgr.enqueueHandler_->finishDirectoryCreation(); });

    // --- FolderOperationCoordinator ---
    QObject::connect(mgr.folderCoordinator_,
                     &FolderOperationCoordinator::startDownloadScanRequested, &mgr,
                     &TransferManager::onStartDownloadScanRequested);
    QObject::connect(mgr.folderCoordinator_,
                     &FolderOperationCoordinator::startDirectoryCreationRequested, &mgr,
                     &TransferManager::onStartDirectoryCreationRequested);
    QObject::connect(mgr.folderCoordinator_, &FolderOperationCoordinator::startDeleteRequested,
                     &mgr,
                     [&mgr](const QString &remotePath) { mgr.enqueueRecursiveDelete(remotePath); });
    QObject::connect(mgr.folderCoordinator_,
                     &FolderOperationCoordinator::pendingUploadAfterDeleteSet, &mgr,
                     [&mgr](const QString &targetPath) { mgr.enqueueRecursiveDelete(targetPath); });
    QObject::connect(mgr.folderCoordinator_, &FolderOperationCoordinator::folderConfirmationNeeded,
                     &mgr, &TransferManager::folderExistsConfirmationNeeded);
    QObject::connect(mgr.folderCoordinator_, &FolderOperationCoordinator::operationStarted, &mgr,
                     &TransferManager::operationStarted);
    QObject::connect(mgr.folderCoordinator_, &FolderOperationCoordinator::allOperationsCompleted,
                     &mgr, &TransferManager::allOperationsCompleted);
    QObject::connect(mgr.folderCoordinator_, &FolderOperationCoordinator::operationsCancelled, &mgr,
                     &TransferManager::operationsCancelled);
    QObject::connect(mgr.folderCoordinator_, &FolderOperationCoordinator::statusMessage, &mgr,
                     &TransferManager::statusMessage);
    QObject::connect(mgr.folderCoordinator_, &FolderOperationCoordinator::operationFailed, &mgr,
                     &TransferManager::operationFailed);
    QObject::connect(mgr.folderCoordinator_,
                     &FolderOperationCoordinator::scheduleProcessNextRequested, &mgr,
                     [&mgr]() { mgr.scheduleProcessNext(); });

    // --- TransferFtpHandler ---
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::itemDataChanged, &mgr,
                     [&mgr](int row) { emit mgr.itemDataChanged(row); });
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::operationCompleted, &mgr,
                     &TransferManager::operationCompleted);
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::operationFailed, &mgr,
                     &TransferManager::operationFailed);
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::queueChanged, &mgr,
                     &TransferManager::queueChanged);
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::deleteProgressUpdate, &mgr,
                     &TransferManager::deleteProgressUpdate);
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::scheduleProcessNextRequested, &mgr,
                     [&mgr]() { mgr.scheduleProcessNext(); });
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::processNextDeleteRequested, &mgr,
                     [&mgr]() { mgr.deleteHandler_->processNextDelete(); });
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::completeBatchRequested, &mgr,
                     [&mgr](int batchId) { mgr.completeBatch(batchId); });
    QObject::connect(mgr.ftpHandler_, &TransferFtpHandler::batchProgressRequested, &mgr,
                     [&mgr](int batchId, bool isComplete, bool includeFailed) {
                         mgr.emitBatchProgressAndComplete(batchId, isComplete, includeFailed);
                     });

    // --- TransferDispatchHandler ---
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::operationStarted, &mgr,
                     &TransferManager::operationStarted);
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::itemDataChanged, &mgr,
                     &TransferManager::itemDataChanged);
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::statusMessage, &mgr,
                     &TransferManager::statusMessage);
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::operationFailed, &mgr,
                     &TransferManager::operationFailed);
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::overwriteConfirmationNeeded,
                     &mgr, &TransferManager::overwriteConfirmationNeeded);
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::allOperationsCompleted, &mgr,
                     &TransferManager::allOperationsCompleted);
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::startTimeoutRequested, &mgr,
                     [&mgr]() { mgr.startOperationTimeout(); });
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::stopTimeoutRequested, &mgr,
                     [&mgr]() { mgr.stopOperationTimeout(); });
    QObject::connect(mgr.dispatchHandler_, &TransferDispatchHandler::scheduleProcessNextRequested,
                     &mgr, [&mgr]() { mgr.scheduleProcessNext(); });

    // --- TransferDeleteHandler ---
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::rowsAboutToBeInserted, &mgr,
                     &TransferManager::itemsAboutToBeInserted);
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::rowsInserted, &mgr,
                     &TransferManager::itemsInserted);
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::transitionToRequested, &mgr,
                     [&mgr](QueueState s) { mgr.transitionTo(s); });
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::scheduleProcessNextRequested, &mgr,
                     [&mgr]() { mgr.scheduleProcessNext(); });
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::operationFailed, &mgr,
                     &TransferManager::operationFailed);
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::operationCompleted, &mgr,
                     &TransferManager::operationCompleted);
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::allOperationsCompleted, &mgr,
                     &TransferManager::allOperationsCompleted);
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::statusMessage, &mgr,
                     &TransferManager::statusMessage);
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::queueChanged, &mgr,
                     &TransferManager::queueChanged);
    QObject::connect(mgr.deleteHandler_, &TransferDeleteHandler::batchStarted, &mgr,
                     &TransferManager::batchStarted);
    QObject::connect(mgr.deleteHandler_,
                     &TransferDeleteHandler::startDirectoryCreationAfterDeleteRequested, &mgr,
                     &TransferManager::onStartDirectoryCreationAfterDeleteRequested);

    // --- SingleFileEnqueueHandler ---
    QObject::connect(mgr.enqueueHandler_, &SingleFileEnqueueHandler::itemsAboutToBeInserted, &mgr,
                     &TransferManager::itemsAboutToBeInserted);
    QObject::connect(mgr.enqueueHandler_, &SingleFileEnqueueHandler::itemsInserted, &mgr,
                     &TransferManager::itemsInserted);
    QObject::connect(mgr.enqueueHandler_, &SingleFileEnqueueHandler::queueChanged, &mgr,
                     &TransferManager::queueChanged);
    QObject::connect(mgr.enqueueHandler_, &SingleFileEnqueueHandler::batchStarted, &mgr,
                     &TransferManager::batchStarted);
    QObject::connect(mgr.enqueueHandler_, &SingleFileEnqueueHandler::operationFailed, &mgr,
                     &TransferManager::operationFailed);
    QObject::connect(mgr.enqueueHandler_, &SingleFileEnqueueHandler::scheduleProcessNextRequested,
                     &mgr, [&mgr]() { mgr.scheduleProcessNext(); });
    QObject::connect(mgr.enqueueHandler_, &SingleFileEnqueueHandler::transitionToIdleRequested,
                     &mgr, [&mgr]() { mgr.transitionTo(QueueState::Idle); });
}

}  // namespace transferwiring
