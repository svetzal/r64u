#ifndef TRANSFERWIRING_H
#define TRANSFERWIRING_H

/**
 * @file transferwiring.h
 * @brief Signal/slot wiring for TransferManager and its collaborators.
 */

class TransferManager;

namespace transferwiring {

/**
 * @brief Connects all collaborator signals and slots for the given TransferManager.
 *
 * This free function wires together the internal collaborators (BatchManager,
 * TransferTimeoutManager, RecursiveScanCoordinator, RemoteDirectoryCoordinator,
 * FolderOperationCoordinator, TransferFtpHandler, TransferDispatchHandler,
 * TransferDeleteHandler, and SingleFileEnqueueHandler) so that the TransferManager
 * orchestrates them correctly at runtime.
 *
 * Called once from the TransferManager constructor.
 *
 * @param mgr The TransferManager whose collaborators should be wired up.
 */
void connectAll(TransferManager &mgr);

}  // namespace transferwiring

#endif  // TRANSFERWIRING_H
