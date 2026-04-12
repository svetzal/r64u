/**
 * @file mocklocalfilesystem.cpp
 * @brief Mock local file system implementation.
 */

#include "mocklocalfilesystem.h"

#include <QDir>
#include <QFileInfo>

MockLocalFileSystem::MockLocalFileSystem(QObject *parent) : ILocalFileSystem(parent) {}

bool MockLocalFileSystem::directoryExists(const QString &path) const
{
    return directoryExistsMap_.value(path, false);
}

bool MockLocalFileSystem::createDirectoryPath(const QString &path)
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

bool MockLocalFileSystem::removeDirectoryRecursively(const QString &path)
{
    removeDirRequests_.append(path);
    if (nextRemoveFails_) {
        nextRemoveFails_ = false;
        return false;
    }
    directoryExistsMap_[path] = false;
    return true;
}

QStringList MockLocalFileSystem::listSubdirectoriesRecursively(const QString &path) const
{
    return subdirMap_.value(path, QStringList{});
}

QStringList MockLocalFileSystem::listFilesRecursively(const QString &path) const
{
    return filesMap_.value(path, QStringList{});
}

bool MockLocalFileSystem::fileExists(const QString &path) const
{
    return fileExistsMap_.value(path, false);
}

qint64 MockLocalFileSystem::fileSize(const QString &path) const
{
    return fileSizeMap_.value(path, 0);
}

QString MockLocalFileSystem::fileName(const QString &path) const
{
    // Pure string operation — no disk access needed.
    return QFileInfo(path).fileName();
}

QString MockLocalFileSystem::relativePath(const QString &basePath, const QString &fullPath) const
{
    // Pure string operation — no disk access needed.
    return QDir(basePath).relativeFilePath(fullPath);
}

// ============================================================================
// Mock configuration
// ============================================================================

void MockLocalFileSystem::mockSetDirectoryExists(const QString &path, bool exists)
{
    directoryExistsMap_[path] = exists;
}

void MockLocalFileSystem::mockSetFileExists(const QString &path, bool exists)
{
    fileExistsMap_[path] = exists;
}

void MockLocalFileSystem::mockSetFileSize(const QString &path, qint64 size)
{
    fileSizeMap_[path] = size;
}

void MockLocalFileSystem::mockSetSubdirectories(const QString &rootPath, const QStringList &subdirs)
{
    subdirMap_[rootPath] = subdirs;
}

void MockLocalFileSystem::mockSetFiles(const QString &rootPath, const QStringList &files)
{
    filesMap_[rootPath] = files;
}

void MockLocalFileSystem::mockSetNextMkpathFails(bool fails)
{
    nextMkpathFails_ = fails;
}

void MockLocalFileSystem::mockSetNextRemoveFails(bool fails)
{
    nextRemoveFails_ = fails;
}

void MockLocalFileSystem::mockReset()
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
