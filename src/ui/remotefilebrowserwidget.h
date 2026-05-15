#ifndef REMOTEFILEBROWSERWIDGET_H
#define REMOTEFILEBROWSERWIDGET_H

#include <QMenu>
#include <QToolBar>
#include <QTreeView>
#include <QWidget>

class RemoteFileModel;
class RemoteFileOperationsService;
class RemoteFileBrowserController;
class RefreshPolicy;
class PathNavigationWidget;
class ErrorHandler;

class RemoteFileBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteFileBrowserWidget(RemoteFileModel *model, QWidget *parent = nullptr);

    void setCurrentDirectory(const QString &path);
    QString currentDirectory() const { return currentDirectory_; }
    [[nodiscard]] QString selectedPath() const;
    [[nodiscard]] QStringList selectedPaths() const;
    [[nodiscard]] bool isSelectedDirectory() const;
    void setDownloadEnabled(bool enabled);
    void setFileOperations(RemoteFileOperationsService *ops);
    void setErrorHandler(ErrorHandler *handler);
    void refresh();
    void refreshIfStale();

    /**
     * @brief RAII guard that suppresses auto-refresh for its lifetime.
     *
     * Delegates to RefreshPolicy internally. Callers can write:
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
    void onConnectionStateChanged(bool connected);

protected:
    void showEvent(QShowEvent *event) override;

signals:
    void downloadRequested(const QString &remotePath, bool isDirectory);
    void deleteRequested(const QString &remotePath, bool isDirectory);
    void createFolderRequested(const QString &path);
    void renameRequested(const QString &oldPath, const QString &newPath);
    void currentDirectoryChanged(const QString &path);
    void selectionChanged();

private slots:
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void onParentFolder();
    void onDownload();
    void onNewFolder();
    void onRename();
    void onDelete();
    void onRefresh();

    // FTP result slots
    void onDirectoryCreated(const QString &path);
    void onFileRemoved(const QString &path);
    void onFileRenamed(const QString &oldPath, const QString &newPath);

private:
    void setupUi();
    void setupContextMenu();
    void setupConnections();
    void updateActions();

    void setSuppressAutoRefresh(bool suppress);

    // Dependencies (not owned)
    RemoteFileModel *remoteFileModel_ = nullptr;
    RemoteFileOperationsService *fileOperations_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;

    // Owned controller and policy (Qt parent ownership)
    RemoteFileBrowserController *controller_ = nullptr;
    RefreshPolicy *refreshPolicy_ = nullptr;

    // State
    QString currentDirectory_;
    bool connected_ = false;

    // UI widgets
    QTreeView *treeView_ = nullptr;
    QToolBar *toolBar_ = nullptr;
    PathNavigationWidget *navWidget_ = nullptr;

    // Actions
    QAction *downloadAction_ = nullptr;
    QAction *newFolderAction_ = nullptr;
    QAction *renameAction_ = nullptr;
    QAction *deleteAction_ = nullptr;
    QAction *refreshAction_ = nullptr;

    // Context menu
    QMenu *contextMenu_ = nullptr;
    QAction *setDestAction_ = nullptr;
};

#endif  // REMOTEFILEBROWSERWIDGET_H
