#ifndef REMOTEFILETREECORE_H
#define REMOTEFILETREECORE_H

#include "core/filesortcore.h"
#include "core/filetypecore.h"
#include "ftp/ftpentry.h"

#include <QDateTime>
#include <QList>
#include <QString>

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
// Fetch-state mutation helpers
// ---------------------------------------------------------------------------

/**
 * @brief Marks a node's fetch state as successfully completed.
 * @param fetched  Out: set to true.
 * @param fetchedAt Out: set to @p now.
 * @param now      Timestamp to record as the fetch time.
 */
void markFetched(bool &fetched, QDateTime &fetchedAt, const QDateTime &now);

/**
 * @brief Marks a node's fetch state as stale (clears it for re-fetch).
 * @param fetched   Out: set to false.
 * @param fetchedAt Out: set to an invalid QDateTime.
 */
void markStale(bool &fetched, QDateTime &fetchedAt);

// ---------------------------------------------------------------------------
// Entry sorting
// ---------------------------------------------------------------------------

/**
 * @brief Returns a sorted copy of @p entries in the order @p spec asks for.
 *
 * Directories are placed before files whatever the key or direction; within
 * each group the filesort rule applies (name, raw size or type, with a
 * case-insensitive name tie-break). The default spec is name, ascending.
 *
 * @param entries Unsorted list of FTP entries.
 * @param spec    Which column orders the listing, and in which direction.
 * @return Sorted copy of the entries.
 */
[[nodiscard]] QList<FtpEntry> sortEntries(QList<FtpEntry> entries, const filesort::Spec &spec = {});

/**
 * @brief The attributes of @p entry the filesort rule looks at.
 *
 * The type name is what the Type column shows for the entry's file type.
 */
[[nodiscard]] filesort::Entry sortableEntry(const FtpEntry &entry);

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

}  // namespace remotefiletree

#endif  // REMOTEFILETREECORE_H
