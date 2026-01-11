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
    // Subscribe to device connection state changes
    connect(deviceConnection_, &DeviceConnection::stateChanged,
            this, &TransferPanel::onConnectionStateChanged);

    // Connect upload/download/delete requests to transfer queue
    connect(localBrowser_, &LocalFileBrowserWidget::uploadRequested,
            this, &TransferPanel::onUploadRequested);
    connect(remoteBrowser_, &RemoteFileBrowserWidget::downloadRequested,
            this, &TransferPanel::onDownloadRequested);
    connect(remoteBrowser_, &RemoteFileBrowserWidget::deleteRequested,
            this, &TransferPanel::onDeleteRequested);

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
        localBrowser_->setUploadEnabled(deviceConnection_->canPerformOperations());
    });
    connect(remoteBrowser_, &RemoteFileBrowserWidget::selectionChanged, this, [this]() {
        remoteBrowser_->setDownloadEnabled(deviceConnection_->canPerformOperations());
    });

    // Set up transfer service for progress widget
    progressWidget_->setTransferService(transferService_);

    // Forward status messages from transfer service
    connect(transferService_, &TransferService::statusMessage,
            this, &TransferPanel::statusMessage);

    // Suppress auto-refresh during queue operations to prevent constant reloading
    connect(transferService_, &TransferService::operationStarted, this, [this]() {
        remoteBrowser_->setSuppressAutoRefresh(true);
    });
    connect(transferService_, &TransferService::allOperationsCompleted, this, [this]() {
        remoteBrowser_->setSuppressAutoRefresh(false);
        remoteBrowser_->refresh();
    });
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
    bool canOperate = deviceConnection_->canPerformOperations();
    remoteBrowser_->onConnectionStateChanged(canOperate);
    localBrowser_->setUploadEnabled(canOperate);
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
    QString remoteDir = remoteBrowser_->currentDirectory();
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
    QString downloadDir = localBrowser_->currentDirectory();

    if (isDirectory) {
        transferService_->downloadDirectory(remotePath, downloadDir);
    } else {
        transferService_->downloadFile(remotePath, downloadDir);
    }
}

void TransferPanel::onDeleteRequested(const QString &remotePath, bool isDirectory)
{
    if (isDirectory) {
        transferService_->deleteRecursive(remotePath);
    } else {
        transferService_->deleteRemote(remotePath, false);
    }
}
