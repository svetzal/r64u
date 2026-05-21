#ifndef REMOTEFILEBROWSERWIDGET_H
#define REMOTEFILEBROWSERWIDGET_H

#include "filebrowserwidget.h"

class RemoteFileModel;
class RemoteFileOperationsService;
class RemoteFileBrowserController;
class RefreshPolicyManager;

class RemoteFileBrowserWidget : public FileBrowserWidget
{
    Q_OBJECT

public:
    explicit RemoteFileBrowserWidget(RemoteFileModel *model, QWidget *parent = nullptr);

    [[nodiscard]] QString selectedPath() const override;
    [[nodiscard]] bool isSelectedDirectory() const override;
    void setDownloadEnabled(bool enabled);
    void setFileOperations(RemoteFileOperationsService *ops);
    void refresh();
    void refreshIfStale();

    /**
     * @brief RAII guard that suppresses auto-refresh for its lifetime.
     *
     * Delegates to RefreshPolicyManager internally. Callers can write:
     * @code
     *   auto suppressor = widget->suppressRefresh();
     *   // ... operations that should not trigger refresh ...
     * @endcode
     */
    class AutoRefreshSuppressor
    {
    public:
        explicit AutoRefreshSuppressor(RemoteFileBrowserWidget *widget);
        ~AutoRefreshSuppressor();
        AutoRefreshSuppressor(const AutoRefreshSuppressor &) = delete;
        AutoRefreshSuppressor &operator=(const AutoRefreshSuppressor &) = delete;

    private:
        RemoteFileBrowserWidget *widget_;
    };

    [[nodiscard]] AutoRefreshSuppressor suppressRefresh();

public slots:
    void setCurrentDirectory(const QString &path) override;
    void onConnectionStateChanged(bool connected);

protected:
    void showEvent(QShowEvent *event) override;

    void setupUi() override;
    void setupContextMenu() override;
    void setupConnections() override;
    void updateActions() override;

    [[nodiscard]] QString labelText() const override { return tr("C64U Files"); }
    [[nodiscard]] QString navLabelText() const override { return tr("Upload to:"); }
    [[nodiscard]] QAbstractItemModel *model() const override;
    [[nodiscard]] QString filePath(const QModelIndex &index) const override;
    [[nodiscard]] bool isDirectory(const QModelIndex &index) const override;
    void navigateToDirectory(const QString &path) override;

protected slots:
    void onParentFolder() override;
    void onContextMenu(const QPoint &pos) override;
    void onNewFolder() override;
    void onRename() override;
    void onDelete() override;

signals:
    void downloadRequested(const QString &remotePath, bool isDirectory);
    void deleteRequested(const QString &remotePath, bool isDirectory);
    void createFolderRequested(const QString &path);
    void renameRequested(const QString &oldPath, const QString &newPath);

private slots:
    void onDownload();
    void onRefresh();

    // FTP result slots
    void onDirectoryCreated(const QString &path);
    void onFileRemoved(const QString &path);
    void onFileRenamed(const QString &oldPath, const QString &newPath);

private:
    void setSuppressAutoRefresh(bool suppress);

    // Dependencies (not owned)
    RemoteFileModel *remoteFileModel_ = nullptr;
    RemoteFileOperationsService *fileOperations_ = nullptr;

    // Owned controller and policy (Qt parent ownership)
    RemoteFileBrowserController *controller_ = nullptr;
    RefreshPolicyManager *refreshPolicy_ = nullptr;

    // State
    bool connected_ = false;

    // Remote-specific actions
    QAction *downloadAction_ = nullptr;
    QAction *refreshAction_ = nullptr;
};

#endif  // REMOTEFILEBROWSERWIDGET_H
