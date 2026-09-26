#ifndef LOCALFILEBROWSERWIDGET_H
#define LOCALFILEBROWSERWIDGET_H

#include "filebrowserwidget.h"

#include <QFileSystemModel>

class LocalFileOperationsService;
class LocalFileProxyModel;

class LocalFileBrowserWidget : public FileBrowserWidget
{
    Q_OBJECT

public:
    explicit LocalFileBrowserWidget(ErrorHandler *errorHandler, QWidget *parent = nullptr);

    [[nodiscard]] QString selectedPath() const override;
    [[nodiscard]] bool isSelectedDirectory() const override;
    void setUploadEnabled(bool enabled);

public slots:
    void setCurrentDirectory(const QString &path) override;

signals:
    void uploadRequested(const QString &localPath, bool isDirectory);
    /**
     * @brief Emitted when a remote entry is dropped on this pane.
     * @param remotePath The device path to download.
     * @param isDirectory Whether it is a directory (downloaded recursively).
     * @param size The file's listed size in bytes (0 for a directory or if unknown).
     * @param localDirectory Where to put it: the directory row it was dropped
     *        on, or this pane's current directory.
     */
    void downloadRequested(const QString &remotePath, bool isDirectory, qint64 size,
                           const QString &localDirectory);

private slots:
    void onUpload();

protected:
    void setupUi() override;
    void setupContextMenu() override;
    void setupConnections() override;
    void updateActions() override;

    [[nodiscard]] QString labelText() const override { return tr("Local Files"); }
    [[nodiscard]] QString navLabelText() const override { return tr("Download to:"); }
    [[nodiscard]] QAbstractItemModel *model() const override;
    [[nodiscard]] QString filePath(const QModelIndex &index) const override;
    [[nodiscard]] bool isDirectory(const QModelIndex &index) const override;
    void navigateToDirectory(const QString &path) override;
    void requestTransferOfSelection() override { onUpload(); }
    [[nodiscard]] dropcore::Pane pane() const override { return dropcore::Pane::Local; }
    /// Emits downloadRequested() for each dropped remote entry.
    void executeDropPlan(const dropcore::DropPlan &plan) override;

    void performNewFolder(const QString &folderName) override;
    void performRename(const QString &path, const QString &newName) override;
    void performDelete(const QList<SelectedEntry> &entries) override;

    [[nodiscard]] QString deleteVerbPhrase() const override;
    [[nodiscard]] QString deleteActionLabel() const override;
    [[nodiscard]] IMessagePresenter::MessageIcon deleteIcon() const override;

private:
    // Service
    LocalFileOperationsService *fileOps_ = nullptr;

    // Model
    QFileSystemModel *fileModel_ = nullptr;
    LocalFileProxyModel *proxyModel_ = nullptr;

    // Local-specific action
    QAction *uploadAction_ = nullptr;
};

#endif  // LOCALFILEBROWSERWIDGET_H
