/**
 * @file localfilesystemservice.cpp
 * @brief Production implementation of ILocalFileSystemService.
 */

#include "localfilesystemservice.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

LocalFileSystemService::LocalFileSystemService(QObject *parent) : ILocalFileSystemService(parent) {}

bool LocalFileSystemService::directoryExists(const QString &path) const
{
    return QDir(path).exists();
}

bool LocalFileSystemService::createDirectoryPath(const QString &path)
{
    return QDir().mkpath(path);
}

bool LocalFileSystemService::removeDirectoryRecursively(const QString &path)
{
    return QDir(path).removeRecursively();
}

QStringList LocalFileSystemService::listSubdirectoriesRecursively(const QString &path) const
{
    QStringList result;
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(it.next());
    }
    return result;
}

QStringList LocalFileSystemService::listFilesRecursively(const QString &path) const
{
    QStringList result;
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(it.next());
    }
    return result;
}

bool LocalFileSystemService::fileExists(const QString &path) const
{
    return QFileInfo(path).exists();
}

qint64 LocalFileSystemService::fileSize(const QString &path) const
{
    return QFileInfo(path).size();
}

QString LocalFileSystemService::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString LocalFileSystemService::relativePath(const QString &basePath, const QString &fullPath) const
{
    return QDir(basePath).relativeFilePath(fullPath);
}
