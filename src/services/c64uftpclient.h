/**
 * @file c64uftpclient.h
 * @brief FTP client for communicating with Ultimate 64/II+ devices.
 *
 * Provides asynchronous FTP operations for file transfers, directory
 * management, and remote file system navigation on C64 Ultimate devices.
 */

#ifndef C64UFTPCLIENT_H
#define C64UFTPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QQueue>
#include <QFile>
#include <QDateTime>

#include <memory>
#include <optional>

/**
 * @brief Represents a single entry in an FTP directory listing.
 */
struct FtpEntry {
    QString name;           ///< Name of the file or directory
    bool isDirectory = false;  ///< True if this entry is a directory
    qint64 size = 0;        ///< Size in bytes (0 for directories)
    QString permissions;    ///< Unix-style permission string
    QDateTime modified;     ///< Last modification timestamp
};

/**
 * @brief Asynchronous FTP client for Ultimate 64/II+ devices.
 *
 * This class provides a Qt-based FTP client implementation specifically
 * designed for communicating with C64 Ultimate devices. It supports:
 * - Connection management with automatic login
 * - Directory listing and navigation
 * - File upload and download (to disk or memory)
 * - Directory creation and removal
 * - File deletion and renaming
 *
 * All operations are asynchronous with results delivered via Qt signals.
 *
 * @par Example usage:
 * @code
 * C64UFtpClient *ftp = new C64UFtpClient(this);
 * ftp->setHost("192.168.1.64");
 * ftp->setCredentials("admin", "password");
 *
 * connect(ftp, &C64UFtpClient::connected, this, &MyClass::onConnected);
 * connect(ftp, &C64UFtpClient::directoryListed, this, &MyClass::onListing);
 *
 * ftp->connectToHost();
 * @endcode
 */
class C64UFtpClient : public QObject
{
    Q_OBJECT

public:
    /// @name FTP Protocol Constants
    /// @{
    static constexpr quint16 DefaultPort = 21;  ///< Default FTP control port
    static constexpr int FtpReplyCodeLength = 3;  ///< Length of FTP reply code
    static constexpr int FtpReplyTextOffset = 4;  ///< Offset to reply text after code
    static constexpr int CrLfLength = 2;  ///< Length of CRLF line ending
    static constexpr int PassivePortMultiplier = 256;  ///< Multiplier for passive port calculation
    /// @}

    /// @name FTP Response Codes (RFC 959)
    /// @{
    static constexpr int FtpReplyServiceReady = 220;  ///< Service ready for new user
    static constexpr int FtpReplyUserLoggedIn = 230;  ///< User logged in, proceed
    static constexpr int FtpReplyPasswordRequired = 331;  ///< User name okay, need password
    static constexpr int FtpReplyPathCreated = 257;  ///< Pathname created
    static constexpr int FtpReplyActionOk = 250;  ///< Requested file action okay
    static constexpr int FtpReplyEnteringPassive = 227;  ///< Entering passive mode
    static constexpr int FtpReplyFileStatusOk = 150;  ///< File status okay, opening connection
    static constexpr int FtpReplyDataConnectionOpen = 125;  ///< Data connection already open
    static constexpr int FtpReplyTransferComplete = 226;  ///< Transfer complete
    static constexpr int FtpReplyPendingFurtherInfo = 350;  ///< Requested action pending further info
    static constexpr int FtpReplyErrorThreshold = 400;  ///< Codes >= this indicate error
    /// @}

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
     * @brief Constructs an FTP client.
     * @param parent Optional parent QObject for memory management.
     */
    explicit C64UFtpClient(QObject *parent = nullptr);

    /**
     * @brief Destructor. Disconnects if connected.
     */
    ~C64UFtpClient() override;

    /**
     * @brief Sets the target host and port.
     * @param host Hostname or IP address of the FTP server.
     * @param port FTP control port (default: 21).
     */
    void setHost(const QString &host, quint16 port = DefaultPort);

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] QString host() const { return host_; }

    /**
     * @brief Sets login credentials.
     * @param user Username for FTP login.
     * @param password Password for FTP login.
     */
    void setCredentials(const QString &user, const QString &password);

    /**
     * @brief Returns the current connection state.
     * @return The current State enum value.
     */
    [[nodiscard]] State state() const { return state_; }

    /**
     * @brief Checks if the client is connected and ready.
     * @return True if in Ready or Busy state.
     */
    [[nodiscard]] bool isConnected() const { return state_ == State::Ready || state_ == State::Busy; }

    /**
     * @brief Checks if successfully logged in.
     * @return True if authentication completed successfully.
     */
    [[nodiscard]] bool isLoggedIn() const { return loggedIn_; }

    /// @name Connection Management
    /// @{

    /**
     * @brief Initiates connection to the configured host.
     *
     * Connects to the FTP server and automatically performs login
     * using the configured credentials. Emits connected() on success
     * or error() on failure.
     */
    void connectToHost();

    /**
     * @brief Disconnects from the FTP server.
     *
     * Sends QUIT command and closes all connections.
     * Emits disconnected() when complete.
     */
    void disconnect();
    /// @}

    /// @name Directory Operations
    /// @{

    /**
     * @brief Lists contents of a directory.
     * @param path Directory path to list (empty for current directory).
     *
     * Results are delivered via directoryListed() signal.
     */
    void list(const QString &path = QString());

    /**
     * @brief Changes the current working directory.
     * @param path Path to change to.
     *
     * Emits directoryChanged() on success.
     */
    void changeDirectory(const QString &path);

    /**
     * @brief Creates a new directory.
     * @param path Path of directory to create.
     *
     * Emits directoryCreated() on success.
     */
    void makeDirectory(const QString &path);

    /**
     * @brief Removes an empty directory.
     * @param path Path of directory to remove.
     */
    void removeDirectory(const QString &path);

    /**
     * @brief Returns the current working directory.
     * @return The current directory path.
     */
    [[nodiscard]] QString currentDirectory() const { return currentDir_; }
    /// @}

    /// @name File Operations
    /// @{

    /**
     * @brief Downloads a file to the local filesystem.
     * @param remotePath Path of the remote file.
     * @param localPath Path where the file will be saved locally.
     *
     * Emits downloadProgress() during transfer and downloadFinished() on completion.
     */
    void download(const QString &remotePath, const QString &localPath);

    /**
     * @brief Downloads a file into memory.
     * @param remotePath Path of the remote file.
     *
     * Useful for previewing files without saving to disk.
     * Emits downloadToMemoryFinished() with the file data.
     */
    void downloadToMemory(const QString &remotePath);

    /**
     * @brief Uploads a file to the remote server.
     * @param localPath Path of the local file.
     * @param remotePath Destination path on the server.
     *
     * Emits uploadProgress() during transfer and uploadFinished() on completion.
     */
    void upload(const QString &localPath, const QString &remotePath);

    /**
     * @brief Deletes a file from the remote server.
     * @param path Path of the file to delete.
     *
     * Emits fileRemoved() on success.
     */
    void remove(const QString &path);

    /**
     * @brief Renames or moves a file on the remote server.
     * @param oldPath Current path of the file.
     * @param newPath New path for the file.
     *
     * Emits fileRenamed() on success.
     */
    void rename(const QString &oldPath, const QString &newPath);
    /// @}

    /**
     * @brief Aborts the current operation.
     *
     * Cancels any ongoing transfer or command.
     */
    void abort();

signals:
    /// @name Connection Signals
    /// @{

    /**
     * @brief Emitted when the connection state changes.
     * @param state The new connection state.
     */
    void stateChanged(C64UFtpClient::State state);

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

private slots:
    void onControlConnected();
    void onControlDisconnected();
    void onControlReadyRead();
    void onControlError(QAbstractSocket::SocketError error);

    void onDataConnected();
    void onDataReadyRead();
    void onDataDisconnected();
    void onDataError(QAbstractSocket::SocketError error);

private:
    enum class Command {
        None,
        User,
        Pass,
        Pwd,
        Cwd,
        Type,
        Pasv,
        List,
        Retr,
        Stor,
        Mkd,
        Rmd,
        Dele,
        RnFr,
        RnTo,
        Quit
    };

    struct PendingCommand {
        Command cmd;
        QString arg;
        QString localPath;  // For transfers
        // For RETR commands - store transfer state with the command
        // so it's not corrupted by concurrent operations
        std::shared_ptr<QFile> transferFile;
        bool isMemoryDownload = false;
    };

    // RAII struct for pending LIST state - cleared with single reset()
    struct PendingListState {
        QString path;
    };

    // RAII struct for pending RETR state - cleared with single reset()
    struct PendingRetrState {
        QString remotePath;
        QString localPath;
        std::shared_ptr<QFile> file;
        bool isMemory = false;
    };

    void setState(State state);
    void sendCommand(const QString &command);
    void queueCommand(Command cmd, const QString &arg = QString(),
                      const QString &localPath = QString());
    void queueRetrCommand(const QString &remotePath, const QString &localPath,
                          std::shared_ptr<QFile> file, bool isMemory);
    void processNextCommand();
    void handleResponse(int code, const QString &text);
    void handleBusyResponse(int code, const QString &text);
    [[nodiscard]] bool parsePassiveResponse(const QString &text, QString &host, quint16 &port);
    [[nodiscard]] QList<FtpEntry> parseDirectoryListing(const QByteArray &data);

    // Network connections
    QTcpSocket *controlSocket_ = nullptr;
    QTcpSocket *dataSocket_ = nullptr;

    // Configuration
    QString host_;
    quint16 port_ = 21;
    QString user_ = "anonymous";
    QString password_;

    // Connection state
    State state_ = State::Disconnected;
    bool loggedIn_ = false;
    QString currentDir_ = "/";

    // Command processing
    Command currentCommand_ = Command::None;
    QString currentArg_;
    QString currentLocalPath_;
    QQueue<PendingCommand> commandQueue_;
    QString responseBuffer_;

    // Data transfer state
    QByteArray dataBuffer_;
    qint64 transferSize_ = 0;
    bool downloading_ = false;
    std::shared_ptr<QFile> transferFile_;  // For uploads (STOR command)
    std::shared_ptr<QFile> currentRetrFile_;  // Current RETR command file
    bool currentRetrIsMemory_ = false;

    // Pending operation state
    // Using optional allows clearing all state with single reset()
    std::optional<PendingListState> pendingList_;  // LIST command completion
    std::optional<PendingRetrState> pendingRetr_;  // RETR command completion
};

#endif // C64UFTPCLIENT_H
