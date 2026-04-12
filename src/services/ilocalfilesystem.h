/**
 * @file ilocalfilesystem.h
 * @brief Gateway interface for local file system operations.
 *
 * All local file system I/O is routed through this interface, enabling
 * unit testing of coordinator logic without touching the real file system.
 * This mirrors the IFtpClient gateway pattern already used for remote I/O.
 *
 * The production implementation (LocalFileSystem) wraps Qt's QDir,
 * QDirIterator, and QFileInfo APIs. Test code injects a mock that records
 * calls and returns pre-configured values.
 *
 * @note This interface deliberately contains no business logic. Any logic
 *       found here should be moved to the pure transfer core instead.
 */

#ifndef ILOCALFILESYSTEM_H
#define ILOCALFILESYSTEM_H

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Abstract gateway for local file system operations.
 *
 * Coordinators that previously called QDir/QDirIterator/QFileInfo directly
 * now depend on this interface, making them independently testable without
 * any real disk access.
 *
 * @par Example usage:
 * @code
 * // Production code
 * ILocalFileSystem *fs = new LocalFileSystem(this);
 *
 * // Test code
 * ILocalFileSystem *fs = new MockLocalFileSystem(this);
 *
 * // Both are used identically
 * fs->createDirectoryPath("/tmp/downloads/subdir");
 * @endcode
 */
class ILocalFileSystem : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Constructs the file system interface.
     * @param parent Optional parent QObject for memory management.
     */
    explicit ILocalFileSystem(QObject *parent = nullptr) : QObject(parent) {}

    /// Destructor.
    ~ILocalFileSystem() override = default;

    // -------------------------------------------------------------------------
    // Directory queries
    // -------------------------------------------------------------------------

    /**
     * @brief Returns true if the given directory path exists on disk.
     * @param path Absolute path to check.
     */
    [[nodiscard]] virtual bool directoryExists(const QString &path) const = 0;

    // -------------------------------------------------------------------------
    // Directory mutations
    // -------------------------------------------------------------------------

    /**
     * @brief Creates all directories in the given path (equivalent to mkdir -p).
     * @param path Absolute path to create.
     * @return True on success; false if the path could not be created.
     */
    [[nodiscard]] virtual bool createDirectoryPath(const QString &path) = 0;

    /**
     * @brief Recursively deletes a directory and all its contents.
     * @param path Absolute path of the directory to remove.
     * @return True on success; false if removal failed.
     */
    [[nodiscard]] virtual bool removeDirectoryRecursively(const QString &path) = 0;

    // -------------------------------------------------------------------------
    // Directory enumeration
    // -------------------------------------------------------------------------

    /**
     * @brief Returns all subdirectory paths under the given root, recursively.
     *
     * Each element is the absolute path of a subdirectory. Does not include
     * the root path itself. Equivalent to iterating with QDirIterator over
     * QDir::Dirs | QDir::NoDotAndDotDot with Subdirectories flag.
     *
     * @param path Absolute root path to walk.
     * @return List of absolute subdirectory paths.
     */
    [[nodiscard]] virtual QStringList listSubdirectoriesRecursively(const QString &path) const = 0;

    /**
     * @brief Returns all file paths under the given root, recursively.
     *
     * Each element is the absolute path of a file. Equivalent to iterating
     * with QDirIterator over QDir::Files with Subdirectories flag.
     *
     * @param path Absolute root path to walk.
     * @return List of absolute file paths.
     */
    [[nodiscard]] virtual QStringList listFilesRecursively(const QString &path) const = 0;

    // -------------------------------------------------------------------------
    // File queries
    // -------------------------------------------------------------------------

    /**
     * @brief Returns true if the given file path exists on disk.
     * @param path Absolute path to check.
     */
    [[nodiscard]] virtual bool fileExists(const QString &path) const = 0;

    /**
     * @brief Returns the size in bytes of the given file.
     * @param path Absolute path of the file.
     * @return File size in bytes; 0 if file does not exist.
     */
    [[nodiscard]] virtual qint64 fileSize(const QString &path) const = 0;

    // -------------------------------------------------------------------------
    // Path utilities (local paths only)
    // -------------------------------------------------------------------------

    /**
     * @brief Extracts the file or directory name from a local absolute path.
     *
     * For example, "/home/user/documents/file.txt" → "file.txt".
     * Equivalent to QFileInfo(path).fileName().
     *
     * @param path Local absolute path.
     * @return The last component of the path.
     */
    [[nodiscard]] virtual QString fileName(const QString &path) const = 0;

    /**
     * @brief Returns the path of fullPath relative to basePath.
     *
     * For example, basePath="/home/user", fullPath="/home/user/docs/a.txt"
     * → "docs/a.txt". Equivalent to QDir(basePath).relativeFilePath(fullPath).
     *
     * @param basePath The base directory path.
     * @param fullPath A path inside basePath.
     * @return Relative path string.
     */
    [[nodiscard]] virtual QString relativePath(const QString &basePath,
                                               const QString &fullPath) const = 0;
};

#endif  // ILOCALFILESYSTEM_H
