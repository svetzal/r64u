/**
 * @file iftpclient.h
 * @brief Interface for FTP client implementations.
 *
 * This interface allows dependency injection of FTP clients, enabling
 * runtime swapping between production and mock implementations for testing.
 */

#ifndef IFTPCLIENT_H
#define IFTPCLIENT_H

#include <QObject>
#include <QString>
#include <QList>

#include "ftpentry.h"

/**
 * @brief Abstract interface for FTP client implementations.
 *
 * This interface defines the contract that all FTP clients must implement.
 * It enables dependency injection for testing by allowing mock implementations
 * to be swapped in at runtime.
 *
 * @par Example usage:
 * @code
 * // Production code
 * IFtpClient *ftp = new C64UFtpClient(this);
 *
 * // Test code
 * IFtpClient *ftp = new MockFtpClient(this);
 *
 * // Both can be used identically
 * ftp->setHost("192.168.1.64");
 * ftp->connectToHost();
 * @endcode
 */
class IFtpClient : public QObject
{
    Q_OBJECT

public:
    /**
     * @brief Connection state of the FTP client.
     */
    enum class State {
        Disconnected,  ///< Not connected to any host
        Connecting,    ///< TCP connection in progress
        Connected,     ///< TCP connected, awaiting server greeting
        LoggingIn,     ///< Authentication in progress
        Ready,         ///< Logged in and ready for commands
        Busy           ///< Command in progress
    };
    Q_ENUM(State)

    /**
     * @brief Constructs an FTP client interface.
     * @param parent Optional parent QObject for memory management.
     */
    explicit IFtpClient(QObject *parent = nullptr) : QObject(parent) {}

    /**
     * @brief Virtual destructor.
     */
    ~IFtpClient() override = default;

    /// @name Configuration
    /// @{

    /**
     * @brief Sets the target host and port.
     * @param host Hostname or IP address of the FTP server.
     * @param port FTP control port (default: 21).
     */
    virtual void setHost(const QString &host, quint16 port = 21) = 0;

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] virtual QString host() const = 0;

    /**
     * @brief Sets login credentials.
     * @param user Username for FTP login.
     * @param password Password for FTP login.
     */
    virtual void setCredentials(const QString &user, const QString &password) = 0;
    /// @}

    /// @name Connection State
    /// @{

    /**
     * @brief Returns the current connection state.
     * @return The current State enum value.
     */
    [[nodiscard]] virtual State state() const = 0;

    /**
     * @brief Checks if the client is connected and ready.
     * @return True if in Ready or Busy state.
     */
    [[nodiscard]] virtual bool isConnected() const = 0;

    /**
     * @brief Checks if successfully logged in.
     * @return True if authentication completed successfully.
     */
    [[nodiscard]] virtual bool isLoggedIn() const = 0;

    /**
     * @brief Returns the current working directory.
     * @return The current directory path.
     */
    [[nodiscard]] virtual QString currentDirectory() const = 0;
    /// @}

    /// @name Connection Management
    /// @{

    /**
     * @brief Initiates connection to the configured host.
     */
    virtual void connectToHost() = 0;

    /**
     * @brief Disconnects from the FTP server.
     */
    virtual void disconnect() = 0;
    /// @}

    /// @name Directory Operations
    /// @{

    /**
     * @brief Lists contents of a directory.
     * @param path Directory path to list (empty for current directory).
     */
    virtual void list(const QString &path = QString()) = 0;

    /**
     * @brief Changes the current working directory.
     * @param path Path to change to.
     */
    virtual void changeDirectory(const QString &path) = 0;

    /**
     * @brief Creates a new directory.
     * @param path Path of directory to create.
     */
    virtual void makeDirectory(const QString &path) = 0;

    /**
     * @brief Removes an empty directory.
     * @param path Path of directory to remove.
     */
    virtual void removeDirectory(const QString &path) = 0;
    /// @}

    /// @name File Operations
    /// @{

    /**
     * @brief Downloads a file to the local filesystem.
     * @param remotePath Path of the remote file.
     * @param localPath Path where the file will be saved locally.
     */
    virtual void download(const QString &remotePath, const QString &localPath) = 0;

    /**
     * @brief Downloads a file into memory.
     * @param remotePath Path of the remote file.
     */
    virtual void downloadToMemory(const QString &remotePath) = 0;

    /**
     * @brief Uploads a file to the remote server.
     * @param localPath Path of the local file.
     * @param remotePath Destination path on the server.
     */
    virtual void upload(const QString &localPath, const QString &remotePath) = 0;

    /**
     * @brief Deletes a file from the remote server.
     * @param path Path of the file to delete.
     */
    virtual void remove(const QString &path) = 0;

    /**
     * @brief Renames or moves a file on the remote server.
     * @param oldPath Current path of the file.
     * @param newPath New path for the file.
     */
    virtual void rename(const QString &oldPath, const QString &newPath) = 0;
    /// @}

    /**
     * @brief Aborts the current operation.
     */
    virtual void abort() = 0;

signals:
    /// @name Connection Signals
    /// @{

    /**
     * @brief Emitted when the connection state changes.
     * @param state The new connection state.
     */
    void stateChanged(IFtpClient::State state);

    /**
     * @brief Emitted when successfully connected and logged in.
     */
    void connected();

    /**
     * @brief Emitted when disconnected from the server.
     */
    void disconnected();

    /**
     * @brief Emitted when an error occurs.
     * @param message Human-readable error description.
     */
    void error(const QString &message);
    /// @}

    /// @name Directory Signals
    /// @{

    /**
     * @brief Emitted when a directory listing completes.
     * @param path The listed directory path.
     * @param entries List of directory entries.
     */
    void directoryListed(const QString &path, const QList<FtpEntry> &entries);

    /**
     * @brief Emitted when the current directory changes.
     * @param path The new current directory.
     */
    void directoryChanged(const QString &path);

    /**
     * @brief Emitted when a directory is created.
     * @param path The path of the created directory.
     */
    void directoryCreated(const QString &path);
    /// @}

    /// @name Transfer Signals
    /// @{

    /**
     * @brief Emitted during file download to report progress.
     * @param file The remote file path.
     * @param received Bytes received so far.
     * @param total Total file size (0 if unknown).
     */
    void downloadProgress(const QString &file, qint64 received, qint64 total);

    /**
     * @brief Emitted when a file download completes.
     * @param remotePath The remote file path.
     * @param localPath The local file path where it was saved.
     */
    void downloadFinished(const QString &remotePath, const QString &localPath);

    /**
     * @brief Emitted when a download-to-memory completes.
     * @param remotePath The remote file path.
     * @param data The file contents.
     */
    void downloadToMemoryFinished(const QString &remotePath, const QByteArray &data);

    /**
     * @brief Emitted during file upload to report progress.
     * @param file The local file path.
     * @param sent Bytes sent so far.
     * @param total Total file size.
     */
    void uploadProgress(const QString &file, qint64 sent, qint64 total);

    /**
     * @brief Emitted when a file upload completes.
     * @param localPath The local file path.
     * @param remotePath The remote destination path.
     */
    void uploadFinished(const QString &localPath, const QString &remotePath);
    /// @}

    /// @name File Operation Signals
    /// @{

    /**
     * @brief Emitted when a file is deleted.
     * @param path The path of the deleted file.
     */
    void fileRemoved(const QString &path);

    /**
     * @brief Emitted when a file is renamed.
     * @param oldPath The original file path.
     * @param newPath The new file path.
     */
    void fileRenamed(const QString &oldPath, const QString &newPath);
    /// @}
};

#endif // IFTPCLIENT_H
