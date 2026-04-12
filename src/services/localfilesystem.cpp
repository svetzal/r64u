/**
 * @file localfilesystem.cpp
 * @brief Production implementation of ILocalFileSystem.
 */

#include "localfilesystem.h"

#include <QDir>
#include <QDirIterator>
#include <QFileInfo>

LocalFileSystem::LocalFileSystem(QObject *parent) : ILocalFileSystem(parent) {}

bool LocalFileSystem::directoryExists(const QString &path) const
{
    return QDir(path).exists();
}

bool LocalFileSystem::createDirectoryPath(const QString &path)
{
    return QDir().mkpath(path);
}

bool LocalFileSystem::removeDirectoryRecursively(const QString &path)
{
    return QDir(path).removeRecursively();
}

QStringList LocalFileSystem::listSubdirectoriesRecursively(const QString &path) const
{
    QStringList result;
    QDirIterator it(path, QDir::Dirs | QDir::NoDotAndDotDot, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(it.next());
    }
    return result;
}

QStringList LocalFileSystem::listFilesRecursively(const QString &path) const
{
    QStringList result;
    QDirIterator it(path, QDir::Files, QDirIterator::Subdirectories);
    while (it.hasNext()) {
        result.append(it.next());
    }
    return result;
}

bool LocalFileSystem::fileExists(const QString &path) const
{
    return QFileInfo(path).exists();
}

qint64 LocalFileSystem::fileSize(const QString &path) const
{
    return QFileInfo(path).size();
}

QString LocalFileSystem::fileName(const QString &path) const
{
    return QFileInfo(path).fileName();
}

QString LocalFileSystem::relativePath(const QString &basePath, const QString &fullPath) const
{
    return QDir(basePath).relativeFilePath(fullPath);
}
