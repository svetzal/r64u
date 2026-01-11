#include "transferpanel.h"
#include "localfilebrowserwidget.h"
#include "remotefilebrowserwidget.h"
#include "transferprogresswidget.h"
#include "services/deviceconnection.h"
#include "services/transferservice.h"
#include "models/remotefilemodel.h"
#include "models/transferqueue.h"

#include <QVBoxLayout>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QDir>

TransferPanel::TransferPanel(DeviceConnection *connection,
                             RemoteFileModel *model,
                             TransferService *transferService,
                             QWidget *parent)
    : QWidget(parent)
    , deviceConnection_(connection)
    , transferService_(transferService)
{
    // These dependencies are required - assert in debug builds
    Q_ASSERT(deviceConnection_ && "DeviceConnection is required");
    Q_ASSERT(model && "RemoteFileModel is required");
    Q_ASSERT(transferService_ && "TransferService is required");

    // Create browser widgets with their dependencies
    // Guard against null ftpClient() - it should exist if deviceConnection_ is valid
    C64UFtpClient *ftpClient = deviceConnection_ ? deviceConnection_->ftpClient() : nullptr;
    remoteBrowser_ = new RemoteFileBrowserWidget(model, ftpClient, this);
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
    // Subscribe to device connection state changes
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnection::stateChanged,
                this, &TransferPanel::onConnectionStateChanged);
    }

    // Connect upload/download/delete requests to transfer queue
    if (localBrowser_) {
        connect(localBrowser_, &LocalFileBrowserWidget::uploadRequested,
                this, &TransferPanel::onUploadRequested);
        connect(localBrowser_, &LocalFileBrowserWidget::statusMessage,
                this, &TransferPanel::statusMessage);
        connect(localBrowser_, &LocalFileBrowserWidget::selectionChanged,
                this, &TransferPanel::selectionChanged);
        connect(localBrowser_, &LocalFileBrowserWidget::selectionChanged, this, [this]() {
            if (localBrowser_ && deviceConnection_) {
                localBrowser_->setUploadEnabled(deviceConnection_->canPerformOperations());
            }
        });
    }

    if (remoteBrowser_) {
        connect(remoteBrowser_, &RemoteFileBrowserWidget::downloadRequested,
                this, &TransferPanel::onDownloadRequested);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::deleteRequested,
                this, &TransferPanel::onDeleteRequested);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::statusMessage,
                this, &TransferPanel::statusMessage);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::selectionChanged,
                this, &TransferPanel::selectionChanged);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::selectionChanged, this, [this]() {
            if (remoteBrowser_ && deviceConnection_) {
                remoteBrowser_->setDownloadEnabled(deviceConnection_->canPerformOperations());
            }
        });
    }

    if (progressWidget_) {
        connect(progressWidget_, &TransferProgressWidget::statusMessage,
                this, &TransferPanel::statusMessage);
        progressWidget_->setTransferService(transferService_);
    }

    // Forward status messages from transfer service
    if (transferService_) {
        connect(transferService_, &TransferService::statusMessage,
                this, &TransferPanel::statusMessage);

        // Suppress auto-refresh during queue operations to prevent constant reloading
        connect(transferService_, &TransferService::operationStarted, this, [this]() {
            if (remoteBrowser_) {
                remoteBrowser_->setSuppressAutoRefresh(true);
            }
        });
        connect(transferService_, &TransferService::allOperationsCompleted, this, [this]() {
            if (remoteBrowser_) {
                remoteBrowser_->setSuppressAutoRefresh(false);
                remoteBrowser_->refresh();
            }
        });
    }
}

void TransferPanel::setCurrentLocalDir(const QString &path)
{
    if (localBrowser_) {
        localBrowser_->setCurrentDirectory(path);
    }
}

void TransferPanel::setCurrentRemoteDir(const QString &path)
{
    if (remoteBrowser_) {
        remoteBrowser_->setCurrentDirectory(path);
    }
}

QString TransferPanel::currentLocalDir() const
{
    return localBrowser_ ? localBrowser_->currentDirectory() : QString();
}

QString TransferPanel::currentRemoteDir() const
{
    return remoteBrowser_ ? remoteBrowser_->currentDirectory() : QString();
}

void TransferPanel::loadSettings()
{
    QSettings settings;
    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    QString savedLocalDir = settings.value("directories/local", homePath).toString();
    QString savedRemoteDir = settings.value("directories/remote", "/").toString();

    if (QDir(savedLocalDir).exists()) {
        setCurrentLocalDir(savedLocalDir);
    }
    setCurrentRemoteDir(savedRemoteDir);
}

void TransferPanel::saveSettings()
{
    QSettings settings;
    settings.setValue("directories/local", currentLocalDir());
    settings.setValue("directories/remote", currentRemoteDir());
}

void TransferPanel::onConnectionStateChanged()
{
    bool canOperate = deviceConnection_ && deviceConnection_->canPerformOperations();
    if (remoteBrowser_) {
        remoteBrowser_->onConnectionStateChanged(canOperate);
    }
    if (localBrowser_) {
        localBrowser_->setUploadEnabled(canOperate);
    }
}

QString TransferPanel::selectedLocalPath() const
{
    return localBrowser_ ? localBrowser_->selectedPath() : QString();
}

QString TransferPanel::selectedRemotePath() const
{
    return remoteBrowser_ ? remoteBrowser_->selectedPath() : QString();
}

bool TransferPanel::isSelectedRemoteDirectory() const
{
    return remoteBrowser_ ? remoteBrowser_->isSelectedDirectory() : false;
}

void TransferPanel::onUploadRequested(const QString &localPath, bool isDirectory)
{
    if (!transferService_) {
        return;
    }

    QString remoteDir = remoteBrowser_ ? remoteBrowser_->currentDirectory() : QString("/");
    if (remoteDir.isEmpty()) {
        remoteDir = "/";
    }

    if (isDirectory) {
        transferService_->uploadDirectory(localPath, remoteDir);
    } else {
        transferService_->uploadFile(localPath, remoteDir);
    }
}

void TransferPanel::onDownloadRequested(const QString &remotePath, bool isDirectory)
{
    if (!transferService_ || !localBrowser_) {
        return;
    }

    QString downloadDir = localBrowser_->currentDirectory();

    if (isDirectory) {
        transferService_->downloadDirectory(remotePath, downloadDir);
    } else {
        transferService_->downloadFile(remotePath, downloadDir);
    }
}

void TransferPanel::onDeleteRequested(const QString &remotePath, bool isDirectory)
{
    if (!transferService_) {
        return;
    }

    if (isDirectory) {
        transferService_->deleteRecursive(remotePath);
    } else {
        transferService_->deleteRemote(remotePath, false);
    }
}
