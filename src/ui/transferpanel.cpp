#include "transferpanel.h"

#include "localfilebrowserwidget.h"
#include "remotefilebrowserwidget.h"
#include "transferprogresscontainer.h"

#include "models/remotefilemodel.h"
#include "models/transferqueue.h"
#include "services/deviceconnection.h"
#include "services/remotefileoperations.h"
#include "services/transferservice.h"
#include "utils/logging.h"

#include <QDir>
#include <QFileInfo>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

TransferPanel::TransferPanel(DeviceConnection *connection, RemoteFileModel *model,
                             TransferService *transferService, QWidget *parent)
    : QWidget(parent), deviceConnection_(connection), transferService_(transferService)
{
    // These dependencies are required - assert in debug builds
    Q_ASSERT(deviceConnection_ && "DeviceConnection is required");
    Q_ASSERT(model && "RemoteFileModel is required");
    Q_ASSERT(transferService_ && "TransferService is required");

    IFtpClient *ftpClient = deviceConnection_ ? deviceConnection_->ftpClient() : nullptr;
    remoteBrowser_ = new RemoteFileBrowserWidget(model, this);
    fileOperations_ = new RemoteFileOperations(ftpClient, this);
    remoteBrowser_->setFileOperations(fileOperations_);
    localBrowser_ = new LocalFileBrowserWidget(this);
    progressContainer_ = new TransferProgressContainer(this);

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
    layout->addWidget(progressContainer_);
}

void TransferPanel::setupConnections()
{
    // Subscribe to device connection state changes
    if (deviceConnection_) {
        connect(deviceConnection_, &DeviceConnection::stateChanged, this,
                &TransferPanel::onConnectionStateChanged);
    }

    // Connect upload/download/delete requests to transfer queue
    if (localBrowser_) {
        connect(localBrowser_, &LocalFileBrowserWidget::uploadRequested, this,
                &TransferPanel::onUploadRequested);
        connect(localBrowser_, &LocalFileBrowserWidget::statusMessage, this,
                &TransferPanel::statusMessage);
        connect(localBrowser_, &LocalFileBrowserWidget::selectionChanged, this,
                &TransferPanel::selectionChanged);
        connect(localBrowser_, &LocalFileBrowserWidget::selectionChanged, this, [this]() {
            if (localBrowser_ && deviceConnection_) {
                localBrowser_->setUploadEnabled(deviceConnection_->canPerformOperations());
            }
        });
    }

    if (remoteBrowser_) {
        connect(remoteBrowser_, &RemoteFileBrowserWidget::downloadRequested, this,
                &TransferPanel::onDownloadRequested);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::deleteRequested, this,
                &TransferPanel::onDeleteRequested);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::statusMessage, this,
                &TransferPanel::statusMessage);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::selectionChanged, this,
                &TransferPanel::selectionChanged);
        connect(remoteBrowser_, &RemoteFileBrowserWidget::selectionChanged, this, [this]() {
            if (remoteBrowser_ && deviceConnection_) {
                remoteBrowser_->setDownloadEnabled(deviceConnection_->canPerformOperations());
            }
        });
    }

    if (progressContainer_) {
        connect(progressContainer_, &TransferProgressContainer::statusMessage, this,
                &TransferPanel::statusMessage);
        connect(progressContainer_, &TransferProgressContainer::clearStatusMessages, this,
                &TransferPanel::clearStatusMessages);
        progressContainer_->setTransferService(transferService_);
    }

    // Forward status messages from transfer service
    if (transferService_) {
        connect(transferService_, &TransferService::statusMessage, this,
                &TransferPanel::statusMessage);

        // Suppress auto-refresh during queue operations to prevent constant reloading
        connect(transferService_, &TransferService::operationStarted, this, [this]() {
            if (remoteBrowser_) {
                refreshSuppressor_ =
                    std::make_unique<RemoteFileBrowserWidget::AutoRefreshSuppressor>(
                        remoteBrowser_);
            }
        });
        connect(transferService_, &TransferService::allOperationsCompleted, this, [this]() {
            refreshSuppressor_.reset();
            if (remoteBrowser_) {
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

void TransferPanel::saveSettings() const
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
        qCDebug(LogUi) << "onUploadRequested: transferService_ is null, skipping upload of"
                       << localPath;
        return;
    }

    QString remoteDir = remoteBrowser_ ? remoteBrowser_->currentDirectory() : QString("/");
    if (remoteDir.isEmpty()) {
        remoteDir = "/";
    }

    if (isDirectory) {
        if (!transferService_->uploadDirectory(localPath, remoteDir))
            qCWarning(LogUi) << "Upload not queued (disconnected):" << localPath;
    } else {
        if (!transferService_->uploadFile(localPath, remoteDir))
            qCWarning(LogUi) << "Upload not queued (disconnected):" << localPath;
    }
}

void TransferPanel::onDownloadRequested(const QString &remotePath, bool isDirectory)
{
    if (!transferService_ || !localBrowser_) {
        qCDebug(LogUi) << "onDownloadRequested: transferService_ or localBrowser_ is null, "
                          "skipping download of"
                       << remotePath;
        return;
    }

    QString downloadDir = localBrowser_->currentDirectory();

    if (isDirectory) {
        if (!transferService_->downloadDirectory(remotePath, downloadDir))
            qCWarning(LogUi) << "Download not queued (disconnected):" << remotePath;
    } else {
        if (!transferService_->downloadFile(remotePath, downloadDir))
            qCWarning(LogUi) << "Download not queued (disconnected):" << remotePath;
    }
}

void TransferPanel::onDeleteRequested(const QString &remotePath, bool isDirectory)
{
    if (!transferService_) {
        qCDebug(LogUi) << "onDeleteRequested: transferService_ is null, skipping delete of"
                       << remotePath;
        return;
    }

    if (isDirectory) {
        if (!transferService_->deleteRecursive(remotePath))
            qCWarning(LogUi) << "Delete not queued (disconnected):" << remotePath;
    } else {
        if (!transferService_->deleteRemote(remotePath, false))
            qCWarning(LogUi) << "Delete not queued (disconnected):" << remotePath;
    }
}
