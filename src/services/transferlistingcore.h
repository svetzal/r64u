/**
 * @file transferlistingcore.h
 * @brief Pure functions for processing FTP directory listings during transfer scanning.
 */

#ifndef TRANSFERLISTINGCORE_H
#define TRANSFERLISTINGCORE_H

#include "transfercore.h"

namespace transfer {

// ---------------------------------------------------------------------------
// Phase 5: Scanning and directory listing helpers
// ---------------------------------------------------------------------------

/// @brief Sort a delete queue: files first, then directories deepest-first.
[[nodiscard]] QList<DeleteItem> sortDeleteQueue(const QList<DeleteItem> &queue);

/// @brief Result of processing a directory listing for download scanning.
struct DirectoryListingResult
{
    QList<PendingScan> newSubScans;                   ///< Subdirectories to scan next
    QList<QPair<QString, QString>> newFileDownloads;  ///< (remotePath, localPath) pairs
    int directoriesScanned = 0;
};

/// @brief Process a FTP directory listing for download scanning.
/// Produces sub-scans for subdirectories and download file pairs.
/// @note Does NOT create local directories — caller must call QDir().mkpath()
[[nodiscard]] DirectoryListingResult
processDirectoryListingForDownload(const PendingScan &currentScan, const QList<FtpEntry> &entries);

/// @brief Result of processing a directory listing for delete scanning.
struct DeleteListingResult
{
    QList<PendingScan> newSubScans;  ///< Subdirectories to scan for deletion
    QList<DeleteItem> fileItems;     ///< Files discovered (to delete)
    DeleteItem directoryItem;        ///< The directory itself (delete after contents)
};

/// @brief Process a FTP directory listing for recursive delete scanning.
[[nodiscard]] DeleteListingResult processDirectoryListingForDelete(const QString &path,
                                                                   const QList<FtpEntry> &entries);

/// @brief Update pending folder ops' destExists flags from a remote directory listing.
/// @param parentPath The remote directory that was listed.
/// @param entries The entries in that directory.
[[nodiscard]] State updateFolderExistence(const State &state, const QString &parentPath,
                                          const QList<FtpEntry> &entries);

/// @brief Result of checking whether an upload's target file already exists.
struct UploadFileCheckResult
{
    State newState;
    bool fileExists = false;
    QString fileName;
};

/// @brief Check if the current upload item's remote target file exists in a directory listing.
[[nodiscard]] UploadFileCheckResult checkUploadFileExists(const State &state,
                                                          const QList<FtpEntry> &entries);

}  // namespace transfer

#endif  // TRANSFERLISTINGCORE_H
