/**
 * @file ftpresponsehandler.h
 * @brief Handles FTP server response codes and drives the client state machine.
 */

#ifndef FTPRESPONSEHANDLER_H
#define FTPRESPONSEHANDLER_H

#include "ftpcommandqueue.h"
#include "ftptransferstate.h"
#include "iftpclient.h"

#include <QObject>
#include <QString>
#include <QTcpSocket>

/**
 * @brief Context snapshot passed to the response handler for each FTP reply.
 *
 * Contains all the per-command state the handler needs to decide what action
 * to take. Built fresh by C64UFtpClient before each handleBusy/handleResponse call.
 */
struct FtpResponseContext
{
    FtpCommandQueue::Command currentCommand = FtpCommandQueue::Command::None;
    QString currentArg;
    QString currentLocalPath;
    IFtpClient::State clientState = IFtpClient::State::Disconnected;
    bool loggedIn = false;
    QString currentDir;
    QAbstractSocket::SocketState dataSocketState = QAbstractSocket::UnconnectedState;
};

/**
 * @brief Encodes what C64UFtpClient must do after a response is processed.
 */
struct FtpResponseAction
{
    enum class Kind {
        None,
        ProcessNext,
        ProcessNextAndConnect,  ///< Connect data socket then process next command
        Disconnect,
        EmitError,
    };

    Kind kind = Kind::None;

    // For ProcessNextAndConnect
    QString dataHost;
    quint16 dataPort = 0;

    // For EmitError
    QString errorMessage;

    // Signals to emit (populated by handler)
    bool emitConnected = false;
    bool emitDisconnected = false;
    QString directoryChangedPath;
    QString directoryCreatedPath;
    QString fileRemovedPath;
    QString fileRenamedOldPath;
    QString fileRenamedNewPath;
    QString downloadFinishedRemotePath;
    QString downloadFinishedLocalPath;
    QByteArray downloadToMemoryData;
    QString downloadToMemoryPath;
    QString uploadFinishedLocalPath;
    QString uploadFinishedRemotePath;
    QString directoryListedPath;
    QList<FtpEntry> directoryListedEntries;
    QString updatedCurrentDir;

    // Download progress
    QString downloadProgressPath;
    qint64 downloadProgressBytes = 0;
    qint64 downloadProgressTotal = 0;

    // State transitions requested
    bool transitionToReady = false;
    bool transitionToLoggedIn = false;  ///< Sets loggedIn flag

    // Queue a command (User/Pass during login)
    FtpCommandQueue::Command enqueueCommand = FtpCommandQueue::Command::None;

    // Transfer buffer/file flags
    bool clearCurrentRetrFile = false;
    bool clearCurrentStorFile = false;
    bool clearRetrBuffer = false;
    bool setDownloading = false;
    bool clearListBuffer = false;

    QString pendingListPath;
    QString savePendingRetrRemotePath;
    QString savePendingRetrLocalPath;
};

/**
 * @brief Translates FTP reply codes into actions for C64UFtpClient.
 *
 * Owns no sockets and performs no I/O; purely interprets reply codes against
 * the current command/state context and returns what should happen next.
 */
class FtpResponseHandler : public QObject
{
    Q_OBJECT

public:
    explicit FtpResponseHandler(FtpTransferState &transferState, QObject *parent = nullptr);

    /**
     * @brief Handles the FTP server's response to the welcome / login flow.
     *
     * Called when state is Connected or LoggingIn.
     *
     * @param code FTP reply code.
     * @param text Reply text.
     * @param ctx Current client state snapshot.
     * @return Action descriptor for C64UFtpClient to act on.
     */
    [[nodiscard]] FtpResponseAction handleResponse(int code, const QString &text,
                                                   const FtpResponseContext &ctx);

    /**
     * @brief Handles a response when the client is in Busy state.
     *
     * @param code FTP reply code.
     * @param text Reply text.
     * @param ctx Current client state snapshot.
     * @return Action descriptor for C64UFtpClient to act on.
     */
    [[nodiscard]] FtpResponseAction handleBusyResponse(int code, const QString &text,
                                                       const FtpResponseContext &ctx);

    /**
     * @brief Processes data received on the data socket.
     *
     * @param data Raw bytes.
     * @param ctx Current client state snapshot.
     */
    void handleDataReceived(const QByteArray &data, const FtpResponseContext &ctx);

    /**
     * @brief Called when the data socket disconnects.
     *
     * @param ctx Current client state snapshot.
     * @return Action descriptor for C64UFtpClient to act on.
     */
    [[nodiscard]] FtpResponseAction handleDataDisconnected(const FtpResponseContext &ctx);

private:
    FtpTransferState &transferState_;
};

#endif  // FTPRESPONSEHANDLER_H
