#include "databasedownloadcontroller.h"

#include "imessagepresenter.h"
#include "qmessageboxpresenter.h"

#include "../services/gamebase64service.h"
#include "../services/hvscmetadataservice.h"
#include "../services/songlengthsdatabase.h"

DatabaseDownloadController::DatabaseDownloadController(QWidget *parentWidget, QObject *parent)
    : QObject(parent), parentWidget_(parentWidget), view_(new DatabaseDownloadView(this)),
      messagePresenter_(std::make_unique<QMessageBoxPresenter>())
{
}

DatabaseDownloadController::~DatabaseDownloadController() = default;

QWidget *DatabaseDownloadController::createWidgets()
{
    QWidget *widget = view_->createWidget();

    // Wire button signals now that the widget groups are populated
    connect(view_->songlengths().button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadDatabase);
    connect(view_->stil().button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadStil);
    connect(view_->buglist().button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadBuglist);
    connect(view_->gameBase64().button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadGameBase64);

    return widget;
}

void DatabaseDownloadController::setMessagePresenter(std::unique_ptr<IMessagePresenter> presenter)
{
    messagePresenter_ = std::move(presenter);
}

// ============================================================
// Service injection
// ============================================================

void DatabaseDownloadController::setSonglengthsDatabase(SonglengthsDatabase *database)
{
    if (songlengthsDatabase_ != nullptr) {
        disconnect(songlengthsDatabase_, nullptr, this, nullptr);
    }

    songlengthsDatabase_ = database;

    if (songlengthsDatabase_ != nullptr) {
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadProgress, this,
                &DatabaseDownloadController::onDatabaseDownloadProgress);
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadFinished, this,
                &DatabaseDownloadController::onDatabaseDownloadFinished);
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadFailed, this,
                &DatabaseDownloadController::onDatabaseDownloadFailed);

        if (songlengthsDatabase_->isLoaded()) {
            view_->songlengths().statusLabel->setText(
                tr("Database: %1 entries loaded").arg(songlengthsDatabase_->entryCount()));
        } else if (songlengthsDatabase_->hasCachedDatabase()) {
            view_->songlengths().statusLabel->setText(tr("Database: Cached (not loaded)"));
        } else {
            view_->songlengths().statusLabel->setText(tr("Database: Not downloaded"));
        }
    }
}

void DatabaseDownloadController::setHVSCMetadataService(HVSCMetadataService *service)
{
    if (hvscMetadataService_ != nullptr) {
        disconnect(hvscMetadataService_, nullptr, this, nullptr);
    }

    hvscMetadataService_ = service;

    if (hvscMetadataService_ != nullptr) {
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadProgress, this,
                &DatabaseDownloadController::onStilDownloadProgress);
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadFinished, this,
                &DatabaseDownloadController::onStilDownloadFinished);
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadFailed, this,
                &DatabaseDownloadController::onStilDownloadFailed);

        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadProgress, this,
                &DatabaseDownloadController::onBuglistDownloadProgress);
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadFinished, this,
                &DatabaseDownloadController::onBuglistDownloadFinished);
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadFailed, this,
                &DatabaseDownloadController::onBuglistDownloadFailed);

        if (hvscMetadataService_->isStilLoaded()) {
            view_->stil().statusLabel->setText(
                tr("%1 entries loaded").arg(hvscMetadataService_->stilEntryCount()));
        } else if (hvscMetadataService_->hasCachedStil()) {
            view_->stil().statusLabel->setText(tr("Cached (not loaded)"));
        } else {
            view_->stil().statusLabel->setText(tr("Not downloaded"));
        }

        if (hvscMetadataService_->isBuglistLoaded()) {
            view_->buglist().statusLabel->setText(
                tr("%1 entries loaded").arg(hvscMetadataService_->buglistEntryCount()));
        } else if (hvscMetadataService_->hasCachedBuglist()) {
            view_->buglist().statusLabel->setText(tr("Cached (not loaded)"));
        } else {
            view_->buglist().statusLabel->setText(tr("Not downloaded"));
        }
    }
}

void DatabaseDownloadController::setGameBase64Service(GameBase64Service *service)
{
    if (gameBase64Service_ != nullptr) {
        disconnect(gameBase64Service_, nullptr, this, nullptr);
    }

    gameBase64Service_ = service;

    if (gameBase64Service_ != nullptr) {
        connect(gameBase64Service_, &GameBase64Service::downloadProgress, this,
                &DatabaseDownloadController::onGameBase64DownloadProgress);
        connect(gameBase64Service_, &GameBase64Service::downloadFinished, this,
                &DatabaseDownloadController::onGameBase64DownloadFinished);
        connect(gameBase64Service_, &GameBase64Service::downloadFailed, this,
                &DatabaseDownloadController::onGameBase64DownloadFailed);

        if (gameBase64Service_->isLoaded()) {
            view_->gameBase64().statusLabel->setText(
                tr("%1 games loaded").arg(gameBase64Service_->gameCount()));
        } else if (gameBase64Service_->hasCachedDatabase()) {
            view_->gameBase64().statusLabel->setText(tr("Cached (not loaded)"));
        } else {
            view_->gameBase64().statusLabel->setText(tr("Not downloaded"));
        }
    }
}

// ============================================================
// Shared download helpers
// ============================================================

void DatabaseDownloadController::startDownload(DownloadWidgetGroup &group)
{
    group.button->setEnabled(false);
    group.progressBar->setVisible(true);
    group.progressBar->setValue(0);
    group.statusLabel->setText(tr("Downloading..."));
}

void DatabaseDownloadController::handleDownloadProgress(DownloadWidgetGroup &group,
                                                        qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        group.progressBar->setMaximum(static_cast<int>(bytesTotal));
        group.progressBar->setValue(static_cast<int>(bytesReceived));
    } else {
        group.progressBar->setMaximum(0);  // Indeterminate
    }
}

void DatabaseDownloadController::handleDownloadFinished(DownloadWidgetGroup &group, int count)
{
    group.button->setEnabled(true);
    group.progressBar->setVisible(false);
    group.statusLabel->setText(tr("%1 %2 loaded").arg(count).arg(group.unitName));

    messagePresenter_->showInfo(parentWidget_, tr("Download Complete"),
                                tr("Successfully downloaded %1.\n%2 %3 loaded.")
                                    .arg(group.itemName)
                                    .arg(count)
                                    .arg(group.unitName));
}

void DatabaseDownloadController::handleDownloadFailed(DownloadWidgetGroup &group,
                                                      const QString &error)
{
    group.button->setEnabled(true);
    group.progressBar->setVisible(false);
    group.statusLabel->setText(tr("Download failed"));

    messagePresenter_->showWarning(parentWidget_, tr("Download Failed"),
                                   tr("Failed to download %1:\n%2").arg(group.itemName).arg(error));
}

// ============================================================
// SonglengthsDatabase slots
// ============================================================

void DatabaseDownloadController::onDownloadDatabase()
{
    if (songlengthsDatabase_ == nullptr) {
        messagePresenter_->showWarning(parentWidget_, tr("Download Database"),
                                       tr("Database service not available."));
        return;
    }
    startDownload(view_->songlengths());
    songlengthsDatabase_->downloadDatabase();
}

void DatabaseDownloadController::onDatabaseDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(view_->songlengths(), bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onDatabaseDownloadFinished(int entryCount)
{
    handleDownloadFinished(view_->songlengths(), entryCount);
}

void DatabaseDownloadController::onDatabaseDownloadFailed(const QString &error)
{
    handleDownloadFailed(view_->songlengths(), error);
}

// ============================================================
// HVSCMetadataService (STIL) slots
// ============================================================

void DatabaseDownloadController::onDownloadStil()
{
    if (hvscMetadataService_ == nullptr) {
        messagePresenter_->showWarning(parentWidget_, tr("Download STIL"),
                                       tr("HVSC metadata service not available."));
        return;
    }
    startDownload(view_->stil());
    hvscMetadataService_->downloadStil();
}

void DatabaseDownloadController::onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(view_->stil(), bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onStilDownloadFinished(int entryCount)
{
    handleDownloadFinished(view_->stil(), entryCount);
}

void DatabaseDownloadController::onStilDownloadFailed(const QString &error)
{
    handleDownloadFailed(view_->stil(), error);
}

// ============================================================
// HVSCMetadataService (BUGlist) slots
// ============================================================

void DatabaseDownloadController::onDownloadBuglist()
{
    if (hvscMetadataService_ == nullptr) {
        messagePresenter_->showWarning(parentWidget_, tr("Download BUGlist"),
                                       tr("HVSC metadata service not available."));
        return;
    }
    startDownload(view_->buglist());
    hvscMetadataService_->downloadBuglist();
}

void DatabaseDownloadController::onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(view_->buglist(), bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onBuglistDownloadFinished(int entryCount)
{
    handleDownloadFinished(view_->buglist(), entryCount);
}

void DatabaseDownloadController::onBuglistDownloadFailed(const QString &error)
{
    handleDownloadFailed(view_->buglist(), error);
}

// ============================================================
// GameBase64Service slots
// ============================================================

void DatabaseDownloadController::onDownloadGameBase64()
{
    if (gameBase64Service_ == nullptr) {
        messagePresenter_->showWarning(parentWidget_, tr("Download GameBase64"),
                                       tr("GameBase64 service not available."));
        return;
    }
    startDownload(view_->gameBase64());
    gameBase64Service_->downloadDatabase();
}

void DatabaseDownloadController::onGameBase64DownloadProgress(qint64 bytesReceived,
                                                              qint64 bytesTotal)
{
    handleDownloadProgress(view_->gameBase64(), bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onGameBase64DownloadFinished(int gameCount)
{
    handleDownloadFinished(view_->gameBase64(), gameCount);
}

void DatabaseDownloadController::onGameBase64DownloadFailed(const QString &error)
{
    handleDownloadFailed(view_->gameBase64(), error);
}
