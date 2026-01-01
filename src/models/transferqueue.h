#ifndef TRANSFERQUEUE_H
#define TRANSFERQUEUE_H

#include <QAbstractListModel>
#include <QQueue>
#include <QString>

class C64UFtpClient;

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
    void clear();
    void removeCompleted();
    void cancelAll();

    int pendingCount() const;
    int activeCount() const;
    bool isProcessing() const { return processing_; }

    // QAbstractListModel interface
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

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

private:
    void processNext();
    int findItemIndex(const QString &localPath, const QString &remotePath) const;

    C64UFtpClient *ftpClient_ = nullptr;
    QList<TransferItem> items_;
    bool processing_ = false;
    int currentIndex_ = -1;
};

#endif // TRANSFERQUEUE_H
