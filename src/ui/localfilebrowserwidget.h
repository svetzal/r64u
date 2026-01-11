#ifndef LOCALFILEBROWSERWIDGET_H
#define LOCALFILEBROWSERWIDGET_H

#include "filebrowserwidget.h"
#include <QFileSystemModel>

class LocalFileProxyModel;

class LocalFileBrowserWidget : public FileBrowserWidget
{
    Q_OBJECT

public:
    explicit LocalFileBrowserWidget(QWidget *parent = nullptr);

    [[nodiscard]] QString selectedPath() const override;
    [[nodiscard]] bool isSelectedDirectory() const override;
    void setUploadEnabled(bool enabled);

public slots:
    void setCurrentDirectory(const QString &path) override;

signals:
    void uploadRequested(const QString &localPath, bool isDirectory);

protected slots:
    void onNewFolder() override;
    void onRename() override;
    void onDelete() override;

private slots:
    void onUpload();

protected:
    void setupUi() override;
    void setupContextMenu() override;
    void setupConnections() override;
    void updateActions() override;

    [[nodiscard]] QString labelText() const override { return tr("Local Files"); }
    [[nodiscard]] QString navLabelText() const override { return tr("Download to:"); }
    [[nodiscard]] QAbstractItemModel* model() const override;
    [[nodiscard]] QString filePath(const QModelIndex &index) const override;
    [[nodiscard]] bool isDirectory(const QModelIndex &index) const override;
    void navigateToDirectory(const QString &path) override;

private:
    // Model
    QFileSystemModel *fileModel_ = nullptr;
    LocalFileProxyModel *proxyModel_ = nullptr;

    // Local-specific action
    QAction *uploadAction_ = nullptr;
};

#endif // LOCALFILEBROWSERWIDGET_H
