#ifndef MOCKFTPCLIENT_H
#define MOCKFTPCLIENT_H

// This mock replaces C64UFtpClient for testing.
// It provides the same interface but allows controlling responses.

#include <QObject>
#include <QQueue>
#include <QMap>
#include <QList>
#include <QDateTime>

// FtpEntry struct - same as in real client
struct FtpEntry {
    QString name;
    bool isDirectory = false;
    qint64 size = 0;
    QString permissions;
    QDateTime modified;
};

/**
 * Mock replacement for C64UFtpClient.
 * Named C64UFtpClient so TransferQueue can use it directly.
 */
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
    ~C64UFtpClient() override = default;

    // State
    void setHost(const QString &host, quint16 port = 21);
    QString host() const { return host_; }
    void setCredentials(const QString &user, const QString &password);
    State state() const { return state_; }
    bool isConnected() const { return connected_; }
    bool isLoggedIn() const { return connected_; }

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
    void downloadToMemory(const QString &remotePath);
    void upload(const QString &localPath, const QString &remotePath);
    void remove(const QString &path);
    void rename(const QString &oldPath, const QString &newPath);

    void abort();

    // === Mock control methods ===

    void mockSetConnected(bool connected);
    void mockSetDirectoryListing(const QString &path, const QList<FtpEntry> &entries);
    void mockSetDownloadData(const QString &remotePath, const QByteArray &data);

    // Process one pending operation and emit its signal
    void mockProcessNextOperation();
    // Process all pending operations
    void mockProcessAllOperations();

    // Inspection for test assertions
    int mockPendingOperationCount() const { return pendingOps_.size(); }
    QStringList mockGetListRequests() const { return listRequests_; }
    QStringList mockGetDownloadRequests() const { return downloadRequests_; }
    QStringList mockGetMkdirRequests() const { return mkdirRequests_; }
    QStringList mockGetUploadRequests() const { return uploadRequests_; }

    // Simulate an error on next operation
    void mockSetNextOperationFails(const QString &errorMessage);

    void mockReset();

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
    void downloadToMemoryFinished(const QString &remotePath, const QByteArray &data);

    void uploadProgress(const QString &file, qint64 sent, qint64 total);
    void uploadFinished(const QString &localPath, const QString &remotePath);

    void fileRemoved(const QString &path);
    void fileRenamed(const QString &oldPath, const QString &newPath);

private:
    struct PendingOp {
        enum Type { List, Download, Upload, Mkdir, DownloadToMemory };
        Type type;
        QString path;
        QString localPath;
    };

    bool connected_ = false;
    State state_ = State::Disconnected;
    QString host_;
    QString currentDir_ = "/";

    QQueue<PendingOp> pendingOps_;
    QMap<QString, QList<FtpEntry>> mockListings_;
    QMap<QString, QByteArray> mockDownloadData_;

    // Track requests for assertions
    QStringList listRequests_;
    QStringList downloadRequests_;
    QStringList mkdirRequests_;
    QStringList uploadRequests_;

    // Error simulation
    bool nextOpFails_ = false;
    QString nextOpError_;
};

#endif // MOCKFTPCLIENT_H
