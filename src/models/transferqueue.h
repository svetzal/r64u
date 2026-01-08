#ifndef TRANSFERQUEUE_H
#define TRANSFERQUEUE_H

#include <QAbstractListModel>
#include <QQueue>
#include <QString>
#include <QSet>

class C64UFtpClient;
struct FtpEntry;

struct TransferItem {
    enum class Direction { Upload, Download };
    enum class Status { Pending, InProgress, Completed, Failed };

    QString localPath;
    QString remotePath;
    Direction direction;
    Status status = Status::Pending;
    qint64 bytesTransferred = 0;
    qint64 totalBytes = 0;
    QString errorMessage;
};

class TransferQueue : public QAbstractListModel
{
    Q_OBJECT

public:
    enum Roles {
        LocalPathRole = Qt::UserRole + 1,
        RemotePathRole,
        DirectionRole,
        StatusRole,
        ProgressRole,
        BytesTransferredRole,
        TotalBytesRole,
        ErrorMessageRole,
        FileNameRole
    };

    explicit TransferQueue(QObject *parent = nullptr);

    void setFtpClient(C64UFtpClient *client);

    // Queue operations
    void enqueueUpload(const QString &localPath, const QString &remotePath);
    void enqueueDownload(const QString &remotePath, const QString &localPath);

    // Recursive operations
    void enqueueRecursiveUpload(const QString &localDir, const QString &remoteDir);
    void enqueueRecursiveDownload(const QString &remoteDir, const QString &localDir);

    void clear();
    void removeCompleted();
    void cancelAll();

    [[nodiscard]] int pendingCount() const;
    [[nodiscard]] int activeCount() const;
    [[nodiscard]] bool isProcessing() const { return processing_; }
    [[nodiscard]] bool isScanning() const { return !pendingScans_.isEmpty(); }

    // QAbstractListModel interface
    [[nodiscard]] int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    [[nodiscard]] QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    [[nodiscard]] QHash<int, QByteArray> roleNames() const override;

signals:
    void transferStarted(const QString &fileName);
    void transferCompleted(const QString &fileName);
    void transferFailed(const QString &fileName, const QString &error);
    void allTransfersCompleted();
    void queueChanged();

private slots:
    void onUploadProgress(const QString &file, qint64 sent, qint64 total);
    void onUploadFinished(const QString &localPath, const QString &remotePath);
    void onDownloadProgress(const QString &file, qint64 received, qint64 total);
    void onDownloadFinished(const QString &remotePath, const QString &localPath);
    void onFtpError(const QString &message);
    void onDirectoryCreated(const QString &path);
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);

private:
    void processNext();
    [[nodiscard]] int findItemIndex(const QString &localPath, const QString &remotePath) const;
    void processRecursiveUpload(const QString &localDir, const QString &remoteDir);
    void processPendingDirectoryCreation();

    C64UFtpClient *ftpClient_ = nullptr;
    QList<TransferItem> items_;
    bool processing_ = false;
    int currentIndex_ = -1;

    // Recursive download state
    struct PendingScan {
        QString remotePath;
        QString localBasePath;
    };
    QQueue<PendingScan> pendingScans_;
    QSet<QString> requestedListings_;  // Track paths we've explicitly requested
    QString recursiveLocalBase_;
    QString recursiveRemoteBase_;
    bool scanningDirectories_ = false;  // True while scanning dirs for recursive download

    // Recursive upload state
    struct PendingMkdir {
        QString remotePath;
        QString localDir;
        QString remoteBase;
    };
    QQueue<PendingMkdir> pendingMkdirs_;
    bool creatingDirectory_ = false;
};

#endif // TRANSFERQUEUE_H
