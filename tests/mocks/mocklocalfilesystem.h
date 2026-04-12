/**
 * @file mocklocalfilesystem.h
 * @brief Mock local file system for unit testing.
 *
 * Implements ILocalFileSystem with configurable responses and call tracking,
 * following the same Spy/Stub pattern used by MockFtpClient.
 */

#ifndef MOCKLOCALFILESYSTEM_H
#define MOCKLOCALFILESYSTEM_H

#include "services/ilocalfilesystem.h"

#include <QMap>
#include <QStringList>

/**
 * @brief Mock local file system implementing ILocalFileSystem for testing.
 *
 * Allows coordinator unit tests to verify file system interactions without
 * touching the real disk. Responses are pre-configured via mock* methods;
 * calls are recorded for assertion via mockGet* methods.
 *
 * @par Example usage:
 * @code
 * MockLocalFileSystem *mockFs = new MockLocalFileSystem(this);
 * mockFs->mockSetDirectoryExists("/tmp/target", false);
 * mockFs->mockSetFiles("/tmp/source", {"/tmp/source/a.prg", "/tmp/source/sub/b.sid"});
 *
 * coordinator->setLocalFileSystem(mockFs);
 * coordinator->doWork();
 *
 * QVERIFY(mockFs->mockGetMkpathRequests().contains("/tmp/target"));
 * @endcode
 */
class MockLocalFileSystem : public ILocalFileSystem
{
    Q_OBJECT

public:
    explicit MockLocalFileSystem(QObject *parent = nullptr);
    ~MockLocalFileSystem() override = default;

    /// @name ILocalFileSystem Implementation
    /// @{
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
    /// @}

    /// @name Mock Configuration
    /// @{

    /**
     * @brief Configures whether a directory path exists.
     * @param path The path to configure.
     * @param exists True if the directory should be reported as existing.
     */
    void mockSetDirectoryExists(const QString &path, bool exists);

    /**
     * @brief Configures whether a file path exists.
     * @param path The path to configure.
     * @param exists True if the file should be reported as existing.
     */
    void mockSetFileExists(const QString &path, bool exists);

    /**
     * @brief Configures the reported size for a file.
     * @param path The file path.
     * @param size The size to return.
     */
    void mockSetFileSize(const QString &path, qint64 size);

    /**
     * @brief Configures the subdirectories returned for a given root path.
     * @param rootPath The root path whose subdirectories are configured.
     * @param subdirs Absolute paths of the subdirectories to return.
     */
    void mockSetSubdirectories(const QString &rootPath, const QStringList &subdirs);

    /**
     * @brief Configures the files returned for a given root path.
     * @param rootPath The root path whose files are configured.
     * @param files Absolute paths of the files to return.
     */
    void mockSetFiles(const QString &rootPath, const QStringList &files);

    /**
     * @brief Configures the next createDirectoryPath() call to fail.
     * @param fails True to make the next mkpath fail; resets after one call.
     */
    void mockSetNextMkpathFails(bool fails);

    /**
     * @brief Configures the next removeDirectoryRecursively() call to fail.
     * @param fails True to make the next remove fail; resets after one call.
     */
    void mockSetNextRemoveFails(bool fails);

    /**
     * @brief Resets all mock state (config and call records).
     */
    void mockReset();
    /// @}

    /// @name Test Inspection Methods
    /// @{

    /**
     * @brief Returns all paths passed to createDirectoryPath().
     */
    [[nodiscard]] QStringList mockGetMkpathRequests() const { return mkpathRequests_; }

    /**
     * @brief Returns all paths passed to removeDirectoryRecursively().
     */
    [[nodiscard]] QStringList mockGetRemoveDirRequests() const { return removeDirRequests_; }
    /// @}

private:
    // Configuration
    QMap<QString, bool> directoryExistsMap_;
    QMap<QString, bool> fileExistsMap_;
    QMap<QString, qint64> fileSizeMap_;
    QMap<QString, QStringList> subdirMap_;
    QMap<QString, QStringList> filesMap_;

    bool nextMkpathFails_ = false;
    bool nextRemoveFails_ = false;

    // Call tracking
    QStringList mkpathRequests_;
    QStringList removeDirRequests_;
};

#endif  // MOCKLOCALFILESYSTEM_H
