#ifndef FAKEFTPSERVER_H
#define FAKEFTPSERVER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QPair>
#include <QPointer>
#include <QString>
#include <QStringList>
#include <QTcpServer>
#include <QTcpSocket>

#include <functional>

/**
 * @brief In-process FTP server for exercising C64UFtpClient end to end.
 *
 * Speaks enough of RFC 959 over real localhost sockets — login, TYPE, PASV
 * with a working passive data channel, LIST, RETR, STOR and ABOR — so tests
 * can drive complete transfers through the real client. Behaviour that real
 * servers vary on (reply/close ordering, ABOR replies, leaving the data
 * connection open after an error) is configurable.
 *
 * Accepts a single control connection at a time.
 */
class FakeFtpServer : public QObject
{
    Q_OBJECT

public:
    /// Order in which the server finishes a successful data transfer.
    enum class CompletionOrder {
        CloseDataThenReply,  ///< Close the data connection, then send 226
        ReplyThenCloseData   ///< Send 226, then close the data connection shortly after
    };

    explicit FakeFtpServer(QObject *parent = nullptr);

    /// Starts listening on an ephemeral localhost port.
    [[nodiscard]] bool listen();

    /// Returns the control port.
    [[nodiscard]] quint16 port() const;

    /// @name Configuration
    /// @{

    /// Overrides the reply for commands starting with @p prefix (first match wins).
    /// @p reply must include the trailing CRLF (and may hold several reply lines).
    void addReply(const QString &prefix, const QString &reply);

    /// Makes @p path retrievable with RETR; unknown paths get 550.
    void setFile(const QString &path, const QByteArray &contents);

    /// Sets the payload returned by LIST.
    void setListing(const QByteArray &listing);

    /// Sets the order used to finish successful LIST/RETR transfers.
    void setCompletionOrder(CompletionOrder order);

    /// RETR sends only the first @p bytes then stalls until ABOR (negative disables).
    void setStallAfterBytes(qint64 bytes);

    /// Sets the reply sent to ABOR while a transfer is active.
    void setAborReplyDuringTransfer(const QByteArray &reply);

    /// Replies 502 to the next @p count PASV commands.
    void failNextPasv(int count = 1);

    /// Advertises a closed port in the next PASV reply; the following transfer
    /// command then gets 425 because no data connection can be made.
    void advertiseDeadDataPortOnce();

    /// When true, a RETR that fails with 550 leaves the passive connection open.
    void setKeepDataOpenOnError(bool keep);
    /// @}

    /// @name Inspection
    /// @{
    [[nodiscard]] QStringList commands() const { return commands_; }
    [[nodiscard]] int commandCount(const QString &prefix) const;
    [[nodiscard]] QByteArray storedFile(const QString &path) const { return stored_.value(path); }
    /// @}

    /// Closes the control connection from the server side.
    void closeClientConnection();

private:
    void onNewControlConnection();
    void onControlReadyRead();
    void handleCommand(const QString &line);
    bool replyFromOverride(const QString &line);
    void reply(const QByteArray &text);

    void handlePasv();
    void handleList();
    void handleRetr(const QString &path);
    void handleStor(const QString &path);
    void handleAbor();

    void withDataConnection(const std::function<void(QTcpSocket *)> &action);
    void finishTransfer(QTcpSocket *data);
    [[nodiscard]] bool hasDataChannel() const;

    QTcpServer controlServer_;
    QPointer<QTcpSocket> control_;
    QPointer<QTcpServer> passiveServer_;
    QPointer<QTcpSocket> data_;
    QPointer<QTcpSocket> activeTransfer_;
    std::function<void(QTcpSocket *)> onDataConnected_;
    bool deadDataPort_ = false;

    QList<QPair<QString, QString>> overrides_;
    QHash<QString, QByteArray> files_;
    QHash<QString, QByteArray> stored_;
    QByteArray listing_;
    CompletionOrder completionOrder_ = CompletionOrder::CloseDataThenReply;
    qint64 stallAfterBytes_ = -1;
    QByteArray aborReplyDuringTransfer_ = "426 Transfer aborted\r\n226 ABOR successful\r\n";
    int pasvFailuresRemaining_ = 0;
    bool advertiseDeadPort_ = false;
    bool keepDataOpenOnError_ = false;
    QStringList commands_;
};

#endif  // FAKEFTPSERVER_H
