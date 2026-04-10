#include "databasedownloadcontroller.h"

#include "../services/gamebase64service.h"
#include "../services/hvscmetadataservice.h"
#include "../services/songlengthsdatabase.h"

#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QWidget>

DatabaseDownloadController::DatabaseDownloadController(QWidget *parentWidget, QObject *parent)
    : QObject(parent), parentWidget_(parentWidget)
{
}

QWidget *DatabaseDownloadController::createWidgets()
{
    auto *container = new QWidget();
    auto *layout = new QVBoxLayout(container);
    layout->setContentsMargins(0, 0, 0, 0);

    // HVSC Databases group
    auto *databaseGroup = new QGroupBox(tr("HVSC Databases"));
    auto *databaseLayout = new QVBoxLayout(databaseGroup);

    // Songlengths section
    auto *songlengthsLabel = new QLabel(tr("<b>Songlengths</b> - Accurate song durations"));
    databaseLayout->addWidget(songlengthsLabel);

    dbWidgets_.statusLabel = new QLabel(tr("Not loaded"));
    dbWidgets_.itemName = tr("HVSC Songlengths database");
    dbWidgets_.unitName = tr("entries");
    databaseLayout->addWidget(dbWidgets_.statusLabel);

    dbWidgets_.progressBar = new QProgressBar();
    dbWidgets_.progressBar->setVisible(false);
    databaseLayout->addWidget(dbWidgets_.progressBar);

    auto *songlengthsButtonLayout = new QHBoxLayout();
    dbWidgets_.button = new QPushButton(tr("Download/Update"));
    dbWidgets_.button->setToolTip(
        tr("Download the HVSC Songlengths database for accurate SID song durations"));
    connect(dbWidgets_.button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadDatabase);
    songlengthsButtonLayout->addWidget(dbWidgets_.button);
    songlengthsButtonLayout->addStretch();
    databaseLayout->addLayout(songlengthsButtonLayout);

    databaseLayout->addSpacing(8);

    // STIL section
    auto *stilLabel = new QLabel(tr("<b>STIL</b> - Tune commentary and cover info"));
    databaseLayout->addWidget(stilLabel);

    stilWidgets_.statusLabel = new QLabel(tr("Not loaded"));
    stilWidgets_.itemName = tr("STIL database");
    stilWidgets_.unitName = tr("entries");
    databaseLayout->addWidget(stilWidgets_.statusLabel);

    stilWidgets_.progressBar = new QProgressBar();
    stilWidgets_.progressBar->setVisible(false);
    databaseLayout->addWidget(stilWidgets_.progressBar);

    auto *stilButtonLayout = new QHBoxLayout();
    stilWidgets_.button = new QPushButton(tr("Download/Update"));
    stilWidgets_.button->setToolTip(
        tr("Download STIL.txt for tune commentary, history, and cover information"));
    connect(stilWidgets_.button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadStil);
    stilButtonLayout->addWidget(stilWidgets_.button);
    stilButtonLayout->addStretch();
    databaseLayout->addLayout(stilButtonLayout);

    databaseLayout->addSpacing(8);

    // BUGlist section
    auto *buglistLabel = new QLabel(tr("<b>BUGlist</b> - Known playback issues"));
    databaseLayout->addWidget(buglistLabel);

    buglistWidgets_.statusLabel = new QLabel(tr("Not loaded"));
    buglistWidgets_.itemName = tr("BUGlist database");
    buglistWidgets_.unitName = tr("entries");
    databaseLayout->addWidget(buglistWidgets_.statusLabel);

    buglistWidgets_.progressBar = new QProgressBar();
    buglistWidgets_.progressBar->setVisible(false);
    databaseLayout->addWidget(buglistWidgets_.progressBar);

    auto *buglistButtonLayout = new QHBoxLayout();
    buglistWidgets_.button = new QPushButton(tr("Download/Update"));
    buglistWidgets_.button->setToolTip(tr("Download BUGlist.txt for known SID playback issues"));
    connect(buglistWidgets_.button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadBuglist);
    buglistButtonLayout->addWidget(buglistWidgets_.button);
    buglistButtonLayout->addStretch();
    databaseLayout->addLayout(buglistButtonLayout);

    layout->addWidget(databaseGroup);

    // GameBase64 Database group
    auto *gamebaseGroup = new QGroupBox(tr("GameBase64 Database"));
    auto *gamebaseLayout = new QVBoxLayout(gamebaseGroup);

    auto *gamebaseLabel = new QLabel(tr("<b>Game Database</b> - ~29,000 C64 games"));
    gamebaseLayout->addWidget(gamebaseLabel);

    gameBase64Widgets_.statusLabel = new QLabel(tr("Not loaded"));
    gameBase64Widgets_.itemName = tr("GameBase64 database");
    gameBase64Widgets_.unitName = tr("games");
    gamebaseLayout->addWidget(gameBase64Widgets_.statusLabel);

    gameBase64Widgets_.progressBar = new QProgressBar();
    gameBase64Widgets_.progressBar->setVisible(false);
    gamebaseLayout->addWidget(gameBase64Widgets_.progressBar);

    auto *gamebaseButtonLayout = new QHBoxLayout();
    gameBase64Widgets_.button = new QPushButton(tr("Download/Update"));
    gameBase64Widgets_.button->setToolTip(
        tr("Download GameBase64 database for game information (publisher, year, genre, etc.)"));
    connect(gameBase64Widgets_.button, &QPushButton::clicked, this,
            &DatabaseDownloadController::onDownloadGameBase64);
    gamebaseButtonLayout->addWidget(gameBase64Widgets_.button);
    gamebaseButtonLayout->addStretch();
    gamebaseLayout->addLayout(gamebaseButtonLayout);

    layout->addWidget(gamebaseGroup);
    layout->addStretch();

    return container;
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
            dbWidgets_.statusLabel->setText(
                tr("Database: %1 entries loaded").arg(songlengthsDatabase_->entryCount()));
        } else if (songlengthsDatabase_->hasCachedDatabase()) {
            dbWidgets_.statusLabel->setText(tr("Database: Cached (not loaded)"));
        } else {
            dbWidgets_.statusLabel->setText(tr("Database: Not downloaded"));
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
            stilWidgets_.statusLabel->setText(
                tr("%1 entries loaded").arg(hvscMetadataService_->stilEntryCount()));
        } else if (hvscMetadataService_->hasCachedStil()) {
            stilWidgets_.statusLabel->setText(tr("Cached (not loaded)"));
        } else {
            stilWidgets_.statusLabel->setText(tr("Not downloaded"));
        }

        if (hvscMetadataService_->isBuglistLoaded()) {
            buglistWidgets_.statusLabel->setText(
                tr("%1 entries loaded").arg(hvscMetadataService_->buglistEntryCount()));
        } else if (hvscMetadataService_->hasCachedBuglist()) {
            buglistWidgets_.statusLabel->setText(tr("Cached (not loaded)"));
        } else {
            buglistWidgets_.statusLabel->setText(tr("Not downloaded"));
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
            gameBase64Widgets_.statusLabel->setText(
                tr("%1 games loaded").arg(gameBase64Service_->gameCount()));
        } else if (gameBase64Service_->hasCachedDatabase()) {
            gameBase64Widgets_.statusLabel->setText(tr("Cached (not loaded)"));
        } else {
            gameBase64Widgets_.statusLabel->setText(tr("Not downloaded"));
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

    QMessageBox::information(parentWidget_, tr("Download Complete"),
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

    QMessageBox::warning(parentWidget_, tr("Download Failed"),
                         tr("Failed to download %1:\n%2").arg(group.itemName).arg(error));
}

// ============================================================
// SonglengthsDatabase slots
// ============================================================

void DatabaseDownloadController::onDownloadDatabase()
{
    if (songlengthsDatabase_ == nullptr) {
        QMessageBox::warning(parentWidget_, tr("Download Database"),
                             tr("Database service not available."));
        return;
    }
    startDownload(dbWidgets_);
    songlengthsDatabase_->downloadDatabase();
}

void DatabaseDownloadController::onDatabaseDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(dbWidgets_, bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onDatabaseDownloadFinished(int entryCount)
{
    handleDownloadFinished(dbWidgets_, entryCount);
}

void DatabaseDownloadController::onDatabaseDownloadFailed(const QString &error)
{
    handleDownloadFailed(dbWidgets_, error);
}

// ============================================================
// HVSCMetadataService (STIL) slots
// ============================================================

void DatabaseDownloadController::onDownloadStil()
{
    if (hvscMetadataService_ == nullptr) {
        QMessageBox::warning(parentWidget_, tr("Download STIL"),
                             tr("HVSC metadata service not available."));
        return;
    }
    startDownload(stilWidgets_);
    hvscMetadataService_->downloadStil();
}

void DatabaseDownloadController::onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(stilWidgets_, bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onStilDownloadFinished(int entryCount)
{
    handleDownloadFinished(stilWidgets_, entryCount);
}

void DatabaseDownloadController::onStilDownloadFailed(const QString &error)
{
    handleDownloadFailed(stilWidgets_, error);
}

// ============================================================
// HVSCMetadataService (BUGlist) slots
// ============================================================

void DatabaseDownloadController::onDownloadBuglist()
{
    if (hvscMetadataService_ == nullptr) {
        QMessageBox::warning(parentWidget_, tr("Download BUGlist"),
                             tr("HVSC metadata service not available."));
        return;
    }
    startDownload(buglistWidgets_);
    hvscMetadataService_->downloadBuglist();
}

void DatabaseDownloadController::onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(buglistWidgets_, bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onBuglistDownloadFinished(int entryCount)
{
    handleDownloadFinished(buglistWidgets_, entryCount);
}

void DatabaseDownloadController::onBuglistDownloadFailed(const QString &error)
{
    handleDownloadFailed(buglistWidgets_, error);
}

// ============================================================
// GameBase64Service slots
// ============================================================

void DatabaseDownloadController::onDownloadGameBase64()
{
    if (gameBase64Service_ == nullptr) {
        QMessageBox::warning(parentWidget_, tr("Download GameBase64"),
                             tr("GameBase64 service not available."));
        return;
    }
    startDownload(gameBase64Widgets_);
    gameBase64Service_->downloadDatabase();
}

void DatabaseDownloadController::onGameBase64DownloadProgress(qint64 bytesReceived,
                                                              qint64 bytesTotal)
{
    handleDownloadProgress(gameBase64Widgets_, bytesReceived, bytesTotal);
}

void DatabaseDownloadController::onGameBase64DownloadFinished(int gameCount)
{
    handleDownloadFinished(gameBase64Widgets_, gameCount);
}

void DatabaseDownloadController::onGameBase64DownloadFailed(const QString &error)
{
    handleDownloadFailed(gameBase64Widgets_, error);
}
