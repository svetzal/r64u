#ifndef TRANSFERPANEL_H
#define TRANSFERPANEL_H

#include <QWidget>
#include <QSplitter>

class DeviceConnection;
class RemoteFileModel;
class TransferQueue;
class LocalFileBrowserWidget;
class RemoteFileBrowserWidget;
class TransferProgressWidget;

class TransferPanel : public QWidget
{
    Q_OBJECT

public:
    explicit TransferPanel(DeviceConnection *connection,
                           RemoteFileModel *model,
                           TransferQueue *queue,
                           QWidget *parent = nullptr);

    // Public API for MainWindow coordination
    void setCurrentLocalDir(const QString &path);
    void setCurrentRemoteDir(const QString &path);
    QString currentLocalDir() const;
    QString currentRemoteDir() const;

    // Settings
    void loadSettings();
    void saveSettings();

    // Selection info for MainWindow
    QString selectedLocalPath() const;
    QString selectedRemotePath() const;
    bool isSelectedRemoteDirectory() const;

signals:
    void statusMessage(const QString &message, int timeout = 0);
    void selectionChanged();

private slots:
    void onConnectionStateChanged();
    void onUploadRequested(const QString &localPath, bool isDirectory);
    void onDownloadRequested(const QString &remotePath, bool isDirectory);

private:
    void setupUi();
    void setupConnections();

    // Dependencies (not owned)
    DeviceConnection *deviceConnection_ = nullptr;
    TransferQueue *transferQueue_ = nullptr;

    // UI widgets
    QSplitter *splitter_ = nullptr;
    RemoteFileBrowserWidget *remoteBrowser_ = nullptr;
    LocalFileBrowserWidget *localBrowser_ = nullptr;
    TransferProgressWidget *progressWidget_ = nullptr;
};

#endif // TRANSFERPANEL_H
