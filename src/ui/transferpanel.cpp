#include "transferpanel.h"
#include "localfilebrowserwidget.h"
#include "remotefilebrowserwidget.h"
#include "transferprogresswidget.h"
#include "services/deviceconnection.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"

#include <QVBoxLayout>
#include <QFileInfo>

TransferPanel::TransferPanel(DeviceConnection *connection,
                             RemoteFileModel *model,
                             TransferQueue *queue,
                             QWidget *parent)
    : QWidget(parent)
    , deviceConnection_(connection)
    , transferQueue_(queue)
{
    // Create browser widgets with their dependencies
    remoteBrowser_ = new RemoteFileBrowserWidget(model, connection->ftpClient(), this);
    localBrowser_ = new LocalFileBrowserWidget(this);
    progressWidget_ = new TransferProgressWidget(this);

    setupUi();
    setupConnections();
}

void TransferPanel::setupUi()
{
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);

    splitter_ = new QSplitter(Qt::Horizontal);
    splitter_->addWidget(remoteBrowser_);
    splitter_->addWidget(localBrowser_);
    splitter_->setSizes({400, 400});

    layout->addWidget(splitter_, 1);
    layout->addWidget(progressWidget_);
}

void TransferPanel::setupConnections()
{
    // Connect upload/download requests to transfer queue
    connect(localBrowser_, &LocalFileBrowserWidget::uploadRequested,
            this, &TransferPanel::onUploadRequested);
    connect(remoteBrowser_, &RemoteFileBrowserWidget::downloadRequested,
            this, &TransferPanel::onDownloadRequested);

    // Forward status messages from all widgets
    connect(localBrowser_, &LocalFileBrowserWidget::statusMessage,
            this, &TransferPanel::statusMessage);
    connect(remoteBrowser_, &RemoteFileBrowserWidget::statusMessage,
            this, &TransferPanel::statusMessage);
    connect(progressWidget_, &TransferProgressWidget::statusMessage,
            this, &TransferPanel::statusMessage);

    // Forward selection changes
    connect(localBrowser_, &LocalFileBrowserWidget::selectionChanged,
            this, &TransferPanel::selectionChanged);
    connect(remoteBrowser_, &RemoteFileBrowserWidget::selectionChanged,
            this, &TransferPanel::selectionChanged);

    // Update upload/download button states based on connection
    connect(localBrowser_, &LocalFileBrowserWidget::selectionChanged, this, [this]() {
        localBrowser_->setUploadEnabled(deviceConnection_->isConnected());
    });
    connect(remoteBrowser_, &RemoteFileBrowserWidget::selectionChanged, this, [this]() {
        remoteBrowser_->setDownloadEnabled(deviceConnection_->isConnected());
    });

    // Set up transfer queue for progress widget
    progressWidget_->setTransferQueue(transferQueue_);
}

void TransferPanel::setCurrentLocalDir(const QString &path)
{
    localBrowser_->setCurrentDirectory(path);
}

void TransferPanel::setCurrentRemoteDir(const QString &path)
{
    remoteBrowser_->setCurrentDirectory(path);
}

QString TransferPanel::currentLocalDir() const
{
    return localBrowser_->currentDirectory();
}

QString TransferPanel::currentRemoteDir() const
{
    return remoteBrowser_->currentDirectory();
}

void TransferPanel::onConnectionStateChanged(bool connected)
{
    remoteBrowser_->onConnectionStateChanged(connected);
    localBrowser_->setUploadEnabled(connected);
}

QString TransferPanel::selectedLocalPath() const
{
    return localBrowser_->selectedPath();
}

QString TransferPanel::selectedRemotePath() const
{
    return remoteBrowser_->selectedPath();
}

bool TransferPanel::isSelectedRemoteDirectory() const
{
    return remoteBrowser_->isSelectedDirectory();
}

void TransferPanel::onUploadRequested(const QString &localPath, bool isDirectory)
{
    if (!deviceConnection_->isConnected()) {
        return;
    }

    QFileInfo fileInfo(localPath);
    QString remoteDir = remoteBrowser_->currentDirectory();
    if (remoteDir.isEmpty()) {
        remoteDir = "/";
    }

    if (isDirectory) {
        transferQueue_->enqueueRecursiveUpload(localPath, remoteDir);
        emit statusMessage(tr("Queued folder upload: %1 -> %2").arg(fileInfo.fileName()).arg(remoteDir), 3000);
    } else {
        if (!remoteDir.endsWith('/')) {
            remoteDir += '/';
        }
        QString remotePath = remoteDir + fileInfo.fileName();
        transferQueue_->enqueueUpload(localPath, remotePath);
        emit statusMessage(tr("Queued upload: %1 -> %2").arg(fileInfo.fileName()).arg(remoteDir), 3000);
    }
}

void TransferPanel::onDownloadRequested(const QString &remotePath, bool isDirectory)
{
    if (!deviceConnection_->isConnected()) {
        return;
    }

    QString downloadDir = localBrowser_->currentDirectory();

    if (isDirectory) {
        transferQueue_->enqueueRecursiveDownload(remotePath, downloadDir);
        QString folderName = QFileInfo(remotePath).fileName();
        emit statusMessage(tr("Queued folder download: %1 -> %2").arg(folderName).arg(downloadDir), 3000);
    } else {
        QString fileName = QFileInfo(remotePath).fileName();
        QString localPath = downloadDir + "/" + fileName;
        transferQueue_->enqueueDownload(remotePath, localPath);
        emit statusMessage(tr("Queued download: %1 -> %2").arg(fileName).arg(downloadDir), 3000);
    }
}
