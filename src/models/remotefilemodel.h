#ifndef REMOTEFILEMODEL_H
#define REMOTEFILEMODEL_H

#include <QAbstractItemModel>
#include <QIcon>
#include <QSet>
#include <QDateTime>
#include "services/iftpclient.h"

class RemoteFileModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    enum Roles {
        FilePathRole = Qt::UserRole + 1,
        IsDirectoryRole,
        FileSizeRole,
        FileTypeRole
    };

    enum class FileType {
        Unknown,
        Directory,
        SidMusic,
        ModMusic,
        Program,
        Cartridge,
        DiskImage,
        TapeImage,
        Rom,
        Config
    };
    Q_ENUM(FileType)

    explicit RemoteFileModel(QObject *parent = nullptr);
    ~RemoteFileModel() override;

    void setFtpClient(IFtpClient *client);

    // QAbstractItemModel interface
    QModelIndex index(int row, int column, const QModelIndex &parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex &child) const override;
    int rowCount(const QModelIndex &parent = QModelIndex()) const override;
    int columnCount(const QModelIndex &parent = QModelIndex()) const override;
    QVariant data(const QModelIndex &index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    bool hasChildren(const QModelIndex &parent = QModelIndex()) const override;
    bool canFetchMore(const QModelIndex &parent) const override;
    void fetchMore(const QModelIndex &parent) override;
    Qt::ItemFlags flags(const QModelIndex &index) const override;

    // Custom methods
    void setRootPath(const QString &path);
    QString rootPath() const { return rootPath_; }

    QString filePath(const QModelIndex &index) const;
    bool isDirectory(const QModelIndex &index) const;
    FileType fileType(const QModelIndex &index) const;
    qint64 fileSize(const QModelIndex &index) const;

    void refresh();
    void refresh(const QModelIndex &index);
    void clear();

    /**
     * @brief Invalidate all cached directory listings.
     *
     * Marks all fetched directories as stale, so they will be refetched
     * on next access. Does not clear the current display.
     */
    void invalidateCache();

    /**
     * @brief Invalidate cache for a specific directory path.
     * @param path The directory path to invalidate.
     */
    void invalidatePath(const QString &path);

    /**
     * @brief Set the cache TTL (time-to-live) in seconds.
     * @param seconds TTL in seconds. Use 0 to disable TTL (infinite cache).
     *
     * Default is 30 seconds. Directories fetched longer ago than this
     * will be considered stale and refetched on next access.
     */
    void setCacheTtl(int seconds) { cacheTtlSeconds_ = seconds; }

    /**
     * @brief Get the current cache TTL.
     * @return TTL in seconds.
     */
    int cacheTtl() const { return cacheTtlSeconds_; }

    /**
     * @brief Check if a directory is stale (older than TTL).
     * @param index The model index to check.
     * @return True if the directory is stale and should be refetched.
     */
    bool isStale(const QModelIndex &index) const;

    /**
     * @brief Refresh stale directories that are currently visible.
     *
     * This is useful for auto-refresh on focus/panel switch.
     * Only refreshes the root directory if it's stale.
     */
    void refreshIfStale();

    static FileType detectFileType(const QString &filename);
    static QIcon iconForFileType(FileType type);
    static QString fileTypeString(FileType type);

signals:
    void loadingStarted(const QString &path);
    void loadingFinished(const QString &path);
    void errorOccurred(const QString &message);

private slots:
    void onDirectoryListed(const QString &path, const QList<FtpEntry> &entries);
    void onFtpError(const QString &message);

private:
    struct TreeNode {
        QString name;
        QString fullPath;
        bool isDirectory = false;
        qint64 size = 0;
        FileType fileType = FileType::Unknown;

        TreeNode *parent = nullptr;
        QList<TreeNode*> children;
        bool fetched = false;
        bool fetching = false;
        QDateTime fetchedAt;  ///< When this directory was last fetched

        ~TreeNode() { qDeleteAll(children); }
    };

    TreeNode* nodeFromIndex(const QModelIndex &index) const;
    QModelIndex indexFromNode(TreeNode *node) const;
    TreeNode* findNodeByPath(const QString &path) const;
    void populateNode(TreeNode *node, const QList<FtpEntry> &entries);

    IFtpClient *ftpClient_ = nullptr;
    TreeNode *rootNode_ = nullptr;
    QString rootPath_ = "/";

    // Map pending fetch paths to parent nodes
    QHash<QString, TreeNode*> pendingFetches_;

    // Track paths we explicitly requested (to ignore listings from other components)
    QSet<QString> requestedListings_;

    // Cache TTL in seconds (0 = infinite, no automatic expiry)
    int cacheTtlSeconds_ = 30;
};

#endif // REMOTEFILEMODEL_H
