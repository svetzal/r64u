#ifndef TRANSFERWIRING_H
#define TRANSFERWIRING_H

/**
 * @file transferwiring.h
 * @brief Signal/slot wiring for TransferManager and its collaborators.
 */

class TransferManager;

namespace transferwiring {

/**
 * @brief Constructs handler and coordinator objects and configures their cross-dependencies.
 *
 * Creates the following members of @p mgr that cannot be initialised in the constructor
 * initialiser list because they depend on each other or on other members that are
 * initialised there:
 *   - deleteHandler_
 *   - ftpHandler_ (+ setTimeoutManager / setDirCreator / setScanCoordinator)
 *   - dispatchHandler_ (+ setLocalFileSystem / setFolderCoordinator)
 *   - enqueueHandler_ (+ callbacks)
 *
 * Also installs the createBatch / completeBatch callbacks that link these handlers
 * back to TransferManager.
 *
 * Called once from the TransferManager constructor, before connectAll().
 *
 * @param mgr The TransferManager whose handlers should be built.
 */
void buildCollaborators(TransferManager &mgr);

/**
 * @brief Connects all collaborator signals and slots for the given TransferManager.
 *
 * This free function wires together the internal collaborators (BatchManager,
 * TransferTimeoutManager, RecursiveScanCoordinator, RemoteDirectoryCoordinator,
 * FolderOperationCoordinator, TransferFtpHandler, TransferDispatchHandler,
 * TransferDeleteHandler, and SingleFileEnqueueHandler) so that the TransferManager
 * orchestrates them correctly at runtime.
 *
 * Called once from the TransferManager constructor, after buildCollaborators().
 *
 * @param mgr The TransferManager whose collaborators should be wired up.
 */
void connectAll(TransferManager &mgr);

}  // namespace transferwiring

#endif  // TRANSFERWIRING_H
