#ifndef LOCALFILESYSTEM_H
#define LOCALFILESYSTEM_H

#include "ilocalfilesystem.h"

/**
 * @brief Production local file system gateway.
 *
 * Thin wrapper around Qt's QDir, QDirIterator, and QFileInfo APIs.
 * Contains no business logic — if you find yourself adding logic here,
 * move it to the pure transfer core (transfercore.h) instead.
 */
class LocalFileSystem : public ILocalFileSystem
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the production file system gateway.
     * @param parent Optional parent QObject for memory management.
     */
    explicit LocalFileSystem(QObject *parent = nullptr);

    [[nodiscard]] bool directoryExists(const QString &path) const override;
    [[nodiscard]] bool createDirectoryPath(const QString &path) override;
    [[nodiscard]] bool removeDirectoryRecursively(const QString &path) override;
    [[nodiscard]] QStringList listSubdirectoriesRecursively(const QString &path) const override;
    [[nodiscard]] QStringList listFilesRecursively(const QString &path) const override;
    [[nodiscard]] bool fileExists(const QString &path) const override;
    [[nodiscard]] qint64 fileSize(const QString &path) const override;
    [[nodiscard]] QString fileName(const QString &path) const override;
    [[nodiscard]] QString relativePath(const QString &basePath,
                                       const QString &fullPath) const override;
};

#endif  // LOCALFILESYSTEM_H
