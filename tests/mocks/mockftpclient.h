/**
 * @file mockftpclient.h
 * @brief Mock FTP client for integration testing.
 *
 * This mock implements IFtpClient and can be injected at runtime for
 * testing components that depend on FTP functionality.
 */

#ifndef MOCKFTPCLIENT_H
#define MOCKFTPCLIENT_H

#include <QQueue>
#include <QMap>

#include "services/iftpclient.h"

/**
 * @brief Mock FTP client implementing IFtpClient for testing.
 *
 * This class provides a controllable FTP client implementation for testing.
 * It implements the same interface as C64UFtpClient, allowing it to be
 * injected at runtime via dependency injection.
 *
 * @par Features:
 * - Queue-based operation processing (manual or automatic)
 * - Configurable directory listings
 * - Configurable download data
 * - Error simulation
 * - Request tracking for test assertions
 *
 * @par Example usage:
 * @code
 * MockFtpClient *mock = new MockFtpClient(this);
 * mock->mockSetConnected(true);
 * mock->mockSetDirectoryListing("/", entries);
 *
 * // Inject into component under test
 * transferQueue->setFtpClient(mock);
 *
 * // Trigger operation
 * transferQueue->enqueueDownload("/file.txt", localPath);
 *
 * // Process mock operations
 * mock->mockProcessAllOperations();
 *
 * // Verify results
 * QCOMPARE(mock->mockGetDownloadRequests().first(), "/file.txt");
 * @endcode
 */
class MockFtpClient : public IFtpClient
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a mock FTP client.
     * @param parent Optional parent QObject for memory management.
     */
    explicit MockFtpClient(QObject *parent = nullptr);

    /**
     * @brief Destructor.
     */
    ~MockFtpClient() override = default;

    /// @name IFtpClient Implementation
    /// @{

    void setHost(const QString &host, quint16 port = 21) override;
    [[nodiscard]] QString host() const override { return host_; }
    void setCredentials(const QString &user, const QString &password) override;

    [[nodiscard]] State state() const override { return state_; }
    [[nodiscard]] bool isConnected() const override { return connected_; }
    [[nodiscard]] bool isLoggedIn() const override { return connected_; }
    [[nodiscard]] QString currentDirectory() const override { return currentDir_; }

    void connectToHost() override;
    void disconnect() override;

    void list(const QString &path = QString()) override;
    void changeDirectory(const QString &path) override;
    void makeDirectory(const QString &path) override;
    void removeDirectory(const QString &path) override;

    void download(const QString &remotePath, const QString &localPath) override;
    void downloadToMemory(const QString &remotePath) override;
    void upload(const QString &localPath, const QString &remotePath) override;
    void remove(const QString &path) override;
    void rename(const QString &oldPath, const QString &newPath) override;

    void abort() override;
    /// @}

    /// @name Mock Control Methods
    /// @{

    /**
     * @brief Sets the mock connection state.
     * @param connected True to simulate connected state.
     *
     * Emits connected() or disconnected() signal accordingly.
     */
    void mockSetConnected(bool connected);

    /**
     * @brief Configures mock directory listing response.
     * @param path The directory path.
     * @param entries The entries to return for this path.
     */
    void mockSetDirectoryListing(const QString &path, const QList<FtpEntry> &entries);

    /**
     * @brief Configures mock download data.
     * @param remotePath The remote file path.
     * @param data The data to return for this file.
     */
    void mockSetDownloadData(const QString &remotePath, const QByteArray &data);

    /**
     * @brief Processes one pending operation and emits its signal.
     *
     * Call this to simulate asynchronous operation completion.
     */
    void mockProcessNextOperation();

    /**
     * @brief Processes all pending operations.
     */
    void mockProcessAllOperations();

    /**
     * @brief Simulates connection by emitting connected signal.
     *
     * Use this when testing components that wait for connection.
     */
    void mockSimulateConnect();

    /**
     * @brief Simulates disconnection by emitting disconnected signal.
     */
    void mockSimulateDisconnect();
    /// @}

    /// @name Test Inspection Methods
    /// @{

    /**
     * @brief Returns count of pending operations.
     */
    [[nodiscard]] int mockPendingOperationCount() const { return pendingOps_.size(); }

    /**
     * @brief Returns all LIST requests made.
     */
    [[nodiscard]] QStringList mockGetListRequests() const { return listRequests_; }

    /**
     * @brief Returns all download requests made.
     */
    [[nodiscard]] QStringList mockGetDownloadRequests() const { return downloadRequests_; }

    /**
     * @brief Returns all mkdir requests made.
     */
    [[nodiscard]] QStringList mockGetMkdirRequests() const { return mkdirRequests_; }

    /**
     * @brief Returns all upload requests made.
     */
    [[nodiscard]] QStringList mockGetUploadRequests() const { return uploadRequests_; }

    /**
     * @brief Returns all delete requests made.
     */
    [[nodiscard]] QStringList mockGetDeleteRequests() const { return deleteRequests_; }

    /**
     * @brief Configures next operation to fail with error.
     * @param errorMessage The error message to emit.
     */
    void mockSetNextOperationFails(const QString &errorMessage);

    /**
     * @brief Resets all mock state.
     */
    void mockReset();
    /// @}

private:
    struct PendingOp {
        enum Type { List, Download, Upload, Mkdir, DownloadToMemory, Delete, Rename };
        Type type;
        QString path;
        QString localPath;
        QString newPath;  // For rename operations
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
    QStringList deleteRequests_;

    // Error simulation
    bool nextOpFails_ = false;
    QString nextOpError_;
};

#endif // MOCKFTPCLIENT_H
