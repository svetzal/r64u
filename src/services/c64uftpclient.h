#ifndef C64UFTPCLIENT_H
#define C64UFTPCLIENT_H

#include <QObject>
#include <QTcpSocket>
#include <QQueue>
#include <QFile>
#include <QDateTime>

struct FtpEntry {
    QString name;
    bool isDirectory = false;
    qint64 size = 0;
    QString permissions;
    QDateTime modified;
};

class C64UFtpClient : public QObject
{
    Q_OBJECT

public:
    enum class State {
        Disconnected,
        Connecting,
        Connected,
        LoggingIn,
        Ready,
        Busy
    };
    Q_ENUM(State)

    explicit C64UFtpClient(QObject *parent = nullptr);
    ~C64UFtpClient() override;

    void setHost(const QString &host, quint16 port = 21);
    QString host() const { return host_; }

    void setCredentials(const QString &user, const QString &password);

    State state() const { return state_; }
    bool isConnected() const { return state_ == State::Ready || state_ == State::Busy; }

    // Connection
    void connectToHost();
    void disconnect();

    // Directory operations
    void list(const QString &path = QString());
    void changeDirectory(const QString &path);
    void makeDirectory(const QString &path);
    void removeDirectory(const QString &path);
    QString currentDirectory() const { return currentDir_; }

    // File operations
    void download(const QString &remotePath, const QString &localPath);
    void upload(const QString &localPath, const QString &remotePath);
    void remove(const QString &path);
    void rename(const QString &oldPath, const QString &newPath);

    // Cancel current operation
    void abort();

signals:
    void stateChanged(C64UFtpClient::State state);
    void connected();
    void disconnected();
    void error(const QString &message);

    void directoryListed(const QString &path, const QList<FtpEntry> &entries);
    void directoryChanged(const QString &path);
    void directoryCreated(const QString &path);

    void downloadProgress(const QString &file, qint64 received, qint64 total);
    void downloadFinished(const QString &remotePath, const QString &localPath);

    void uploadProgress(const QString &file, qint64 sent, qint64 total);
    void uploadFinished(const QString &localPath, const QString &remotePath);

    void fileRemoved(const QString &path);
    void fileRenamed(const QString &oldPath, const QString &newPath);

private slots:
    void onControlConnected();
    void onControlDisconnected();
    void onControlReadyRead();
    void onControlError(QAbstractSocket::SocketError error);

    void onDataConnected();
    void onDataReadyRead();
    void onDataDisconnected();

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
    };

    void setState(State state);
    void sendCommand(const QString &command);
    void queueCommand(Command cmd, const QString &arg = QString(),
                      const QString &localPath = QString());
    void processNextCommand();
    void handleResponse(int code, const QString &text);
    void handleBusyResponse(int code, const QString &text);
    bool parsePassiveResponse(const QString &text, QString &host, quint16 &port);
    QList<FtpEntry> parseDirectoryListing(const QByteArray &data);

    QTcpSocket *controlSocket_ = nullptr;
    QTcpSocket *dataSocket_ = nullptr;

    QString host_;
    quint16 port_ = 21;
    QString user_ = "anonymous";
    QString password_;

    State state_ = State::Disconnected;
    Command currentCommand_ = Command::None;
    QString currentArg_;
    QString currentLocalPath_;
    QString currentDir_ = "/";

    QQueue<PendingCommand> commandQueue_;
    QString responseBuffer_;

    // For data transfers
    QByteArray dataBuffer_;
    QFile *transferFile_ = nullptr;
    qint64 transferSize_ = 0;
    bool downloading_ = false;
};

#endif // C64UFTPCLIENT_H
