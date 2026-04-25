#ifndef TRANSFERPANEL_H
#define TRANSFERPANEL_H

#include "ui/ipanel.h"
#include "ui/remotefilebrowserwidget.h"

#include <QSplitter>
#include <QWidget>

#include <memory>

class DeviceConnection;
class RemoteFileModel;
class TransferQueue;
class TransferService;
class LocalFileBrowserWidget;
class RemoteFileOperations;
class TransferProgressContainer;

class TransferPanel : public QWidget, public IPanel
{
    Q_OBJECT

public:
    QObject *asQObject() override { return this; }

    explicit TransferPanel(DeviceConnection *connection, RemoteFileModel *model,
                           TransferService *transferService, QWidget *parent = nullptr);

    // Public API for MainWindow coordination
    void setCurrentLocalDir(const QString &path);
    void setCurrentRemoteDir(const QString &path) override;
    QString currentLocalDir() const;
    QString currentRemoteDir() const override;

    // Settings
    void loadSettings();
    void saveSettings() const;

    // Selection info for MainWindow
    QString selectedLocalPath() const;
    QString selectedRemotePath() const;
    bool isSelectedRemoteDirectory() const;

signals:
    void statusMessage(const QString &message, int timeout = 0);
    void clearStatusMessages();
    void selectionChanged();

private slots:
    void onConnectionStateChanged();
    void onUploadRequested(const QString &localPath, bool isDirectory);
    void onDownloadRequested(const QString &remotePath, bool isDirectory);
    void onDeleteRequested(const QString &remotePath, bool isDirectory);

private:
    void setupUi();
    void setupConnections();

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;
    TransferService *transferService_ = nullptr;

    // UI widgets
    QSplitter *splitter_ = nullptr;
    RemoteFileBrowserWidget *remoteBrowser_ = nullptr;
    LocalFileBrowserWidget *localBrowser_ = nullptr;
    TransferProgressContainer *progressContainer_ = nullptr;
    RemoteFileOperations *fileOperations_ = nullptr;
    std::unique_ptr<RemoteFileBrowserWidget::AutoRefreshSuppressor> refreshSuppressor_;
};

#endif  // TRANSFERPANEL_H
