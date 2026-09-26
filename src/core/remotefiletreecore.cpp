/**
 * @file remotefiletreecore.cpp
 * @brief Implementation of pure remote file tree helper functions.
 */

#include "remotefiletreecore.h"

#include <algorithm>

namespace remotefiletree {

bool isStale(bool fetched, const QDateTime &fetchedAt, const QDateTime &now, int ttlSeconds)
{
    if (!fetched) {
        return false;  // Not yet fetched — not stale, simply unfetched
    }

    if (ttlSeconds <= 0) {
        return false;  // TTL disabled — never stale
    }

    if (!fetchedAt.isValid()) {
        return true;  // No timestamp — treat as stale
    }

    qint64 ageSeconds = fetchedAt.secsTo(now);
    return ageSeconds >= static_cast<qint64>(ttlSeconds);
}

void markFetched(bool &fetched, QDateTime &fetchedAt, const QDateTime &now)
{
    fetched = true;
    fetchedAt = now;
}

void markStale(bool &fetched, QDateTime &fetchedAt)
{
    fetched = false;
    fetchedAt = QDateTime();
}

filesort::Entry sortableEntry(const FtpEntry &entry)
{
    const filetype::FileType type = entry.isDirectory ? filetype::FileType::Directory
                                                      : filetype::detectFromFilename(entry.name);
    return {entry.name, entry.isDirectory, entry.size, filetype::displayName(type)};
}

QList<FtpEntry> sortEntries(QList<FtpEntry> entries, const filesort::Spec &spec)
{
    std::sort(entries.begin(), entries.end(), [&spec](const FtpEntry &a, const FtpEntry &b) {
        return filesort::lessThan(sortableEntry(a), sortableEntry(b), spec);
    });
    return entries;
}

QString childPath(const QString &parentFullPath, const QString &name)
{
    if (parentFullPath.endsWith('/')) {
        return parentFullPath + name;
    }
    return parentFullPath + '/' + name;
}

}  // namespace remotefiletree
