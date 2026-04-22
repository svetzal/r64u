#ifndef IFTPCLIENT_H
#define IFTPCLIENT_H

#include "ftpentry.h"

#include <QList>
#include <QObject>
#include <QString>

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
    enum class State { Disconnected, Connecting, Connected, LoggingIn, Ready, Busy };
    Q_ENUM(State)

    explicit IFtpClient(QObject *parent = nullptr) : QObject(parent) {}
    ~IFtpClient() override = default;

    /**
     * @brief Sets the target host and port.
     * @param host Hostname or IP address of the FTP server.
     * @param port FTP control port (default: 21).
     */
    virtual void setHost(const QString &host, quint16 port = 21) = 0;

    [[nodiscard]] virtual QString host() const = 0;
    virtual void setCredentials(const QString &user, const QString &password) = 0;

    [[nodiscard]] virtual State state() const = 0;
    [[nodiscard]] virtual bool isConnected() const = 0;
    [[nodiscard]] virtual bool isLoggedIn() const = 0;
    [[nodiscard]] virtual QString currentDirectory() const = 0;

    virtual void connectToHost() = 0;
    virtual void disconnect() = 0;

    virtual void list(const QString &path = QString()) = 0;
    virtual void changeDirectory(const QString &path) = 0;
    virtual void makeDirectory(const QString &path) = 0;
    virtual void removeDirectory(const QString &path) = 0;

    virtual void download(const QString &remotePath, const QString &localPath) = 0;
    virtual void downloadToMemory(const QString &remotePath) = 0;
    virtual void upload(const QString &localPath, const QString &remotePath) = 0;
    virtual void remove(const QString &path) = 0;
    virtual void rename(const QString &oldPath, const QString &newPath) = 0;

    virtual void abort() = 0;

signals:
    void stateChanged(IFtpClient::State state);
    void connected();
    void disconnected();
    void error(const QString &message);

    void directoryListed(const QString &path, const QList<FtpEntry> &entries);
    void directoryChanged(const QString &path);
    void directoryCreated(const QString &path);

    /**
     * @brief Emitted during file download to report progress.
     * @param file The remote file path.
     * @param received Bytes received so far.
     * @param total Total file size (0 if unknown).
     */
    void downloadProgress(const QString &file, qint64 received, qint64 total);

    void downloadFinished(const QString &remotePath, const QString &localPath);
    void downloadToMemoryFinished(const QString &remotePath, const QByteArray &data);

    /**
     * @brief Emitted during file upload to report progress.
     * @param file The local file path.
     * @param sent Bytes sent so far.
     * @param total Total file size (0 if unknown).
     */
    void uploadProgress(const QString &file, qint64 sent, qint64 total);

    void uploadFinished(const QString &localPath, const QString &remotePath);

    void fileRemoved(const QString &path);
    void fileRenamed(const QString &oldPath, const QString &newPath);
};

#endif  // IFTPCLIENT_H
