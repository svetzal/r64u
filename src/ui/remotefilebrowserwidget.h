#ifndef REMOTEFILEBROWSERWIDGET_H
#define REMOTEFILEBROWSERWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QToolBar>
#include <QMenu>

class RemoteFileModel;
class C64UFtpClient;
class PathNavigationWidget;

class RemoteFileBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit RemoteFileBrowserWidget(RemoteFileModel *model,
                                     C64UFtpClient *ftpClient,
                                     QWidget *parent = nullptr);

    void setCurrentDirectory(const QString &path);
    QString currentDirectory() const { return currentDirectory_; }
    QString selectedPath() const;
    bool isSelectedDirectory() const;
    void setDownloadEnabled(bool enabled);
    void onConnectionStateChanged(bool connected);
    void refresh();
    void setSuppressAutoRefresh(bool suppress);

signals:
    void downloadRequested(const QString &remotePath, bool isDirectory);
    void deleteRequested(const QString &remotePath, bool isDirectory);
    void currentDirectoryChanged(const QString &path);
    void selectionChanged();
    void statusMessage(const QString &message, int timeout = 0);

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

    // Dependencies (not owned)
    RemoteFileModel *remoteFileModel_ = nullptr;
    C64UFtpClient *ftpClient_ = nullptr;

    // State
    QString currentDirectory_;
    bool connected_ = false;
    bool suppressAutoRefresh_ = false;

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

#endif // REMOTEFILEBROWSERWIDGET_H
