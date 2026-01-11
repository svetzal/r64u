#ifndef LOCALFILEBROWSERWIDGET_H
#define LOCALFILEBROWSERWIDGET_H

#include <QWidget>
#include <QTreeView>
#include <QToolBar>
#include <QMenu>
#include <QFileSystemModel>

class LocalFileProxyModel;
class PathNavigationWidget;

class LocalFileBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    explicit LocalFileBrowserWidget(QWidget *parent = nullptr);

    void setCurrentDirectory(const QString &path);
    QString currentDirectory() const { return currentDirectory_; }
    QString selectedPath() const;
    bool isSelectedDirectory() const;
    void setUploadEnabled(bool enabled);

signals:
    void uploadRequested(const QString &localPath, bool isDirectory);
    void currentDirectoryChanged(const QString &path);
    void selectionChanged();
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onDoubleClicked(const QModelIndex &index);
    void onContextMenu(const QPoint &pos);
    void onParentFolder();
    void onUpload();
    void onNewFolder();
    void onRename();
    void onDelete();

private:
    void setupUi();
    void setupContextMenu();
    void setupConnections();
    void updateActions();

    // State
    QString currentDirectory_;

    // UI widgets
    QTreeView *treeView_ = nullptr;
    QFileSystemModel *fileModel_ = nullptr;
    LocalFileProxyModel *proxyModel_ = nullptr;
    QToolBar *toolBar_ = nullptr;
    PathNavigationWidget *navWidget_ = nullptr;

    // Actions
    QAction *uploadAction_ = nullptr;
    QAction *newFolderAction_ = nullptr;
    QAction *renameAction_ = nullptr;
    QAction *deleteAction_ = nullptr;

    // Context menu
    QMenu *contextMenu_ = nullptr;
    QAction *setDestAction_ = nullptr;
};

#endif // LOCALFILEBROWSERWIDGET_H
