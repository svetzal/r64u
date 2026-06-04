#ifndef REMOTEFILETREECORE_H
#define REMOTEFILETREECORE_H

#include "core/filetypecore.h"
#include "ftp/ftpentry.h"

#include <QDateTime>
#include <QList>
#include <QString>
#include <QStyle>

/**
 * @namespace remotefiletree
 * @brief Pure functions for remote file tree logic: caching, sorting, path construction, and icon
 * mapping.
 *
 * All functions are free of I/O and Qt signals — they operate only on plain value arguments
 * and return results by value, making them trivially testable without mocks.
 */
namespace remotefiletree {

// ---------------------------------------------------------------------------
// Cache staleness
// ---------------------------------------------------------------------------

/**
 * @brief Returns true if a cached directory listing should be considered stale.
 *
 * A listing is stale when:
 * - @p ttlSeconds > 0 (TTL is enabled), AND
 * - @p fetchedAt is valid, AND
 * - the elapsed time from @p fetchedAt to @p now is >= @p ttlSeconds.
 *
 * A listing with an invalid @p fetchedAt timestamp is always considered stale
 * (treat missing timestamp as requiring refresh).
 *
 * If @p ttlSeconds <= 0 the TTL feature is disabled and the function always
 * returns false.  If @p fetched is false (listing never completed) the function
 * also returns false — the listing is not stale, it simply has not been fetched.
 *
 * @param fetched    True when the directory has been successfully fetched at least once.
 * @param fetchedAt  Timestamp of the most recent successful fetch.
 * @param now        Current date/time used for age calculation.
 * @param ttlSeconds Maximum age in seconds before a listing is considered stale.
 *                   A value <= 0 disables the TTL check entirely.
 * @return True if the listing should be refreshed; false otherwise.
 */
[[nodiscard]] bool isStale(bool fetched, const QDateTime &fetchedAt, const QDateTime &now,
                           int ttlSeconds);

// ---------------------------------------------------------------------------
// Entry sorting
// ---------------------------------------------------------------------------

/**
 * @brief Returns a sorted copy of @p entries using the standard tree ordering.
 *
 * Directories are placed before files.  Entries of the same kind are sorted
 * alphabetically by name (case-insensitive).
 *
 * @param entries Unsorted list of FTP entries.
 * @return Sorted copy of the entries.
 */
[[nodiscard]] QList<FtpEntry> sortEntries(QList<FtpEntry> entries);

// ---------------------------------------------------------------------------
// Path construction
// ---------------------------------------------------------------------------

/**
 * @brief Constructs the full path for a child entry given its parent's full path and name.
 *
 * Appends a '/' separator between @p parentFullPath and @p name unless
 * @p parentFullPath already ends with '/'.
 *
 * @param parentFullPath Full path of the parent directory (e.g. "/SD/Games").
 * @param name           Name of the child entry (e.g. "myfile.prg").
 * @return Full path of the child (e.g. "/SD/Games/myfile.prg").
 */
[[nodiscard]] QString childPath(const QString &parentFullPath, const QString &name);

// ---------------------------------------------------------------------------
// Icon mapping
// ---------------------------------------------------------------------------

/**
 * @brief Maps a file type to the standard Qt pixmap used as a tree-view icon.
 *
 * @param type The file type to map.
 * @return The corresponding QStyle::StandardPixmap.
 */
[[nodiscard]] QStyle::StandardPixmap standardPixmapFor(filetype::FileType type);

}  // namespace remotefiletree

#endif  // REMOTEFILETREECORE_H
