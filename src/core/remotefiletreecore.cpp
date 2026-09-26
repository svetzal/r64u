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

QList<FtpEntry> sortEntries(QList<FtpEntry> entries)
{
    std::sort(entries.begin(), entries.end(), [](const FtpEntry &a, const FtpEntry &b) {
        if (a.isDirectory != b.isDirectory) {
            return a.isDirectory;  // directories before files
        }
        return a.name.compare(b.name, Qt::CaseInsensitive) < 0;
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
