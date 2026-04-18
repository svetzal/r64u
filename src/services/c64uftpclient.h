/**
 * @file c64uftpclient.h
 * @brief FTP client for communicating with Ultimate 64/II+ devices.
 *
 * Provides asynchronous FTP operations for file transfers, directory
 * management, and remote file system navigation on C64 Ultimate devices.
 */

#ifndef C64UFTPCLIENT_H
#define C64UFTPCLIENT_H

#include "ftpcommandqueue.h"
#include "ftpresponsehandler.h"
#include "ftptransferstate.h"
#include "iftpclient.h"

#include <QDateTime>
#include <QFile>
#include <QTcpSocket>
#include <QTimer>

#include <memory>
#include <optional>

/**
 * @brief Asynchronous FTP client for Ultimate 64/II+ devices.
 *
 * Coordinates FtpResponseHandler (protocol logic), FtpCommandQueue, and
 * FtpTransferState. Owns the TCP sockets and delegates response
 * interpretation to the handler via FtpResponseContext / FtpResponseAction.
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
class C64UFtpClient : public IFtpClient
{
    Q_OBJECT

public:
    /// @name FTP Protocol Constants
    /// @{
    static constexpr quint16 DefaultPort = 21;         ///< Default FTP control port
    static constexpr int ConnectionTimeoutMs = 15000;  ///< Connection timeout in milliseconds
    static constexpr int FtpReplyCodeLength = 3;       ///< Length of FTP reply code
    static constexpr int FtpReplyTextOffset = 4;       ///< Offset to reply text after code
    static constexpr int CrLfLength = 2;               ///< Length of CRLF line ending
    static constexpr int PassivePortMultiplier = 256;  ///< Multiplier for passive port calculation
    /// @}

    /// @name FTP Response Codes (RFC 959)
    /// @{
    static constexpr int FtpReplyServiceReady = 220;      ///< Service ready for new user
    static constexpr int FtpReplyUserLoggedIn = 230;      ///< User logged in, proceed
    static constexpr int FtpReplyPasswordRequired = 331;  ///< User name okay, need password
    static constexpr int FtpReplyPathCreated = 257;       ///< Pathname created
    static constexpr int FtpReplyActionOk = 250;          ///< Requested file action okay
    static constexpr int FtpReplyEnteringPassive = 227;   ///< Entering passive mode
    static constexpr int FtpReplyFileStatusOk = 150;      ///< File status okay, opening connection
    static constexpr int FtpReplyDataConnectionOpen = 125;  ///< Data connection already open
    static constexpr int FtpReplyTransferComplete = 226;    ///< Transfer complete
    static constexpr int FtpReplyPendingFurtherInfo =
        350;                                            ///< Requested action pending further info
    static constexpr int FtpReplyErrorThreshold = 400;  ///< Codes >= this indicate error
    static constexpr int FtpReplyFileExists = 553;      ///< File/directory already exists
    /// @}

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
    void setHost(const QString &host, quint16 port = DefaultPort) override;

    /**
     * @brief Returns the currently configured host.
     * @return The hostname or IP address.
     */
    [[nodiscard]] QString host() const override { return host_; }

    /**
     * @brief Sets login credentials.
     * @param user Username for FTP login.
     * @param password Password for FTP login.
     */
    void setCredentials(const QString &user, const QString &password) override;

    /**
     * @brief Returns the current connection state.
     * @return The current State enum value.
     */
    [[nodiscard]] State state() const override { return state_; }

    /**
     * @brief Checks if the client is connected and ready.
     * @return True if in Ready or Busy state.
     */
    [[nodiscard]] bool isConnected() const override
    {
        return state_ == State::Ready || state_ == State::Busy;
    }

    /**
     * @brief Checks if successfully logged in.
     * @return True if authentication completed successfully.
     */
    [[nodiscard]] bool isLoggedIn() const override { return loggedIn_; }

    /// @name Connection Management
    /// @{

    /**
     * @brief Initiates connection to the configured host.
     */
    void connectToHost() override;

    /**
     * @brief Disconnects from the FTP server.
     */
    void disconnect() override;
    /// @}

    /// @name Directory Operations
    /// @{
    void list(const QString &path = QString()) override;
    void changeDirectory(const QString &path) override;
    void makeDirectory(const QString &path) override;
    void removeDirectory(const QString &path) override;
    [[nodiscard]] QString currentDirectory() const override { return currentDir_; }
    /// @}

    /// @name File Operations
    /// @{
    void download(const QString &remotePath, const QString &localPath) override;
    void downloadToMemory(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remotePath) override;
    void remove(const QString &path) override;
    void rename(const QString &oldPath, const QString &newPath) override;
    /// @}

    /**
     * @brief Aborts the current operation.
     */
    void abort() override;

private slots:
    void onControlConnected();
    void onControlDisconnected();
    void onControlReadyRead();
    void onControlError(QAbstractSocket::SocketError error);
    void onConnectionTimeout();

    void onDataConnected();
    void onDataReadyRead();
    void onDataDisconnected();
    void onDataError(QAbstractSocket::SocketError error);

private:
    using Command = FtpCommandQueue::Command;
    using PendingCommand = FtpCommandQueue::PendingCommand;

    void setState(State state);
    void sendCommand(const QString &command);
    void queueCommand(Command cmd, const QString &arg = QString(),
                      const QString &localPath = QString());
    void queueRetrCommand(const QString &remotePath, const QString &localPath,
                          std::shared_ptr<QFile> file, bool isMemory);
    void queueStorCommand(const QString &remotePath, const QString &localPath,
                          std::shared_ptr<QFile> file);
    void processNextCommand();
    void drainCommandQueue();
    void resetTransferState();
    void performDisconnectCleanup();

    /// Builds a context snapshot for the response handler.
    [[nodiscard]] FtpResponseContext buildContext() const;

    /// Applies an action returned by the response handler.
    void applyAction(const FtpResponseAction &action);

    // Network connections
    QTcpSocket *controlSocket_ = nullptr;
    QTcpSocket *dataSocket_ = nullptr;
    QTimer *connectionTimer_ = nullptr;

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
    FtpCommandQueue commandQueue_;
    QString responseBuffer_;

    // Data transfer state
    FtpTransferState transferState_;

    // Response interpretation (owned, Qt parent)
    FtpResponseHandler *responseHandler_ = nullptr;
};

#endif  // C64UFTPCLIENT_H
