#ifndef MOCKFTPCLIENT_H
#define MOCKFTPCLIENT_H

// This mock replaces C64UFtpClient for testing.
// It provides the same interface but allows controlling responses.

#include <QQueue>
#include <QMap>
#include <QList>
#include <QDateTime>

#include "services/iftpclient.h"

/**
 * Mock replacement for C64UFtpClient.
 * Named C64UFtpClient so it shadows the production implementation at link time.
 * Inherits from IFtpClient to maintain interface compatibility.
 */
class C64UFtpClient : public IFtpClient
{
    Q_OBJECT

public:
    // State enum is inherited from IFtpClient

    explicit C64UFtpClient(QObject *parent = nullptr);
    ~C64UFtpClient() override = default;

    // State - IFtpClient implementations
    void setHost(const QString &host, quint16 port = 21) override;
    QString host() const override { return host_; }
    void setCredentials(const QString &user, const QString &password) override;
    State state() const override { return state_; }
    bool isConnected() const override { return connected_; }
    bool isLoggedIn() const override { return connected_; }

    // Connection - IFtpClient implementations
    void connectToHost() override;
    void disconnect() override;

    // Directory operations - IFtpClient implementations
    void list(const QString &path = QString()) override;
    void changeDirectory(const QString &path) override;
    void makeDirectory(const QString &path) override;
    void removeDirectory(const QString &path) override;
    QString currentDirectory() const override { return currentDir_; }

    // File operations - IFtpClient implementations
    void download(const QString &remotePath, const QString &localPath) override;
    void downloadToMemory(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remotePath) override;
    void remove(const QString &path) override;
    void rename(const QString &oldPath, const QString &newPath) override;

    void abort() override;

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

    // Signals are inherited from IFtpClient

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
