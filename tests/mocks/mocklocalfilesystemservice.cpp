/**
 * @file mocklocalfilesystemservice.cpp
 * @brief Mock local file system implementation.
 */

#include "mocklocalfilesystemservice.h"

#include <QDir>
#include <QFileInfo>

MockLocalFileSystemService::MockLocalFileSystemService(QObject *parent)
    : ILocalFileSystemService(parent)
{
}

bool MockLocalFileSystemService::directoryExists(const QString &path) const
{
    return directoryExistsMap_.value(path, false);
}

bool MockLocalFileSystemService::createDirectoryPath(const QString &path)
{
    mkpathRequests_.append(path);
    if (nextMkpathFails_) {
        nextMkpathFails_ = false;
        return false;
    }
    // Record as existing after creation
    directoryExistsMap_[path] = true;
    return true;
}

bool MockLocalFileSystemService::removeDirectoryRecursively(const QString &path)
{
    removeDirRequests_.append(path);
    if (nextRemoveFails_) {
        nextRemoveFails_ = false;
        return false;
    }
    directoryExistsMap_[path] = false;
    return true;
}

QStringList MockLocalFileSystemService::listSubdirectoriesRecursively(const QString &path) const
{
    return subdirMap_.value(path, QStringList{});
}

QStringList MockLocalFileSystemService::listFilesRecursively(const QString &path) const
{
    return filesMap_.value(path, QStringList{});
}

bool MockLocalFileSystemService::fileExists(const QString &path) const
{
    return fileExistsMap_.value(path, false);
}

qint64 MockLocalFileSystemService::fileSize(const QString &path) const
{
    return fileSizeMap_.value(path, 0);
}

QString MockLocalFileSystemService::fileName(const QString &path) const
{
    // Pure string operation — no disk access needed.
    return QFileInfo(path).fileName();
}

QString MockLocalFileSystemService::relativePath(const QString &basePath,
                                                 const QString &fullPath) const
{
    // Pure string operation — no disk access needed.
    return QDir(basePath).relativeFilePath(fullPath);
}

// ============================================================================
// Mock configuration
// ============================================================================

void MockLocalFileSystemService::mockSetDirectoryExists(const QString &path, bool exists)
{
    directoryExistsMap_[path] = exists;
}

void MockLocalFileSystemService::mockSetFileExists(const QString &path, bool exists)
{
    fileExistsMap_[path] = exists;
}

void MockLocalFileSystemService::mockSetFileSize(const QString &path, qint64 size)
{
    fileSizeMap_[path] = size;
}

void MockLocalFileSystemService::mockSetSubdirectories(const QString &rootPath,
                                                       const QStringList &subdirs)
{
    subdirMap_[rootPath] = subdirs;
}

void MockLocalFileSystemService::mockSetFiles(const QString &rootPath, const QStringList &files)
{
    filesMap_[rootPath] = files;
}

void MockLocalFileSystemService::mockSetNextMkpathFails(bool fails)
{
    nextMkpathFails_ = fails;
}

void MockLocalFileSystemService::mockSetNextRemoveFails(bool fails)
{
    nextRemoveFails_ = fails;
}

void MockLocalFileSystemService::mockReset()
{
    directoryExistsMap_.clear();
    fileExistsMap_.clear();
    fileSizeMap_.clear();
    subdirMap_.clear();
    filesMap_.clear();
    nextMkpathFails_ = false;
    nextRemoveFails_ = false;
    mkpathRequests_.clear();
    removeDirRequests_.clear();
}
