#include "preferencesdialog.h"

#include "../services/c64urestclient.h"
#include "../services/credentialstore.h"
#include "../services/gamebase64service.h"
#include "../services/hvscmetadataservice.h"
#include "../services/songlengthsdatabase.h"

#include <QApplication>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setupUi();
    loadSettings();
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

    // Two-column layout: settings on left, databases on right
    auto *columnsLayout = new QHBoxLayout();

    // ========== LEFT COLUMN: Settings ==========
    auto *leftColumn = new QVBoxLayout();

    // Device settings group
    auto *deviceGroup = new QGroupBox(tr("Device Connection"));
    auto *deviceLayout = new QFormLayout(deviceGroup);

    hostEdit_ = new QLineEdit();
    hostEdit_->setPlaceholderText(tr("e.g., 192.168.1.100 or c64u"));
    deviceLayout->addRow(tr("Host:"), hostEdit_);

    passwordEdit_ = new QLineEdit();
    passwordEdit_->setEchoMode(QLineEdit::Password);
    passwordEdit_->setPlaceholderText(tr("Leave empty if not configured"));
    deviceLayout->addRow(tr("Password:"), passwordEdit_);

    auto *testButton = new QPushButton(tr("Test Connection"));
    connect(testButton, &QPushButton::clicked, this, &PreferencesDialog::onTestConnection);
    deviceLayout->addRow("", testButton);

    autoConnectCheck_ = new QCheckBox(tr("Connect automatically on startup"));
    deviceLayout->addRow("", autoConnectCheck_);

    leftColumn->addWidget(deviceGroup);

    // Application settings group
    auto *appGroup = new QGroupBox(tr("Application"));
    auto *appLayout = new QFormLayout(appGroup);

    auto *pathLayout = new QHBoxLayout();
    localDirEdit_ = new QLineEdit();
    localDirEdit_->setReadOnly(true);
    pathLayout->addWidget(localDirEdit_);

    auto *browseButton = new QPushButton(tr("Browse..."));
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this, tr("Select Local Directory"),
                                                         localDirEdit_->text());
        if (!path.isEmpty()) {
            localDirEdit_->setText(path);
        }
    });
    pathLayout->addWidget(browseButton);

    appLayout->addRow(tr("Local Directory:"), pathLayout);

    leftColumn->addWidget(appGroup);

    // Drive settings group
    auto *driveGroup = new QGroupBox(tr("Drive Defaults"));
    auto *driveLayout = new QFormLayout(driveGroup);

    defaultDriveCombo_ = new QComboBox();
    defaultDriveCombo_->addItem(tr("Drive A"));
    defaultDriveCombo_->addItem(tr("Drive B"));
    driveLayout->addRow(tr("Default Drive:"), defaultDriveCombo_);

    mountModeCombo_ = new QComboBox();
    mountModeCombo_->addItem(tr("Read/Write"), "readwrite");
    mountModeCombo_->addItem(tr("Read Only"), "readonly");
    mountModeCombo_->addItem(tr("Unlinked"), "unlinked");
    driveLayout->addRow(tr("Mount Mode:"), mountModeCombo_);

    leftColumn->addWidget(driveGroup);

    // View settings group
    auto *viewGroup = new QGroupBox(tr("Video Display"));
    auto *viewLayout = new QFormLayout(viewGroup);

    scalingModeCombo_ = new QComboBox();
    scalingModeCombo_->addItem(tr("Sharp (Nearest Neighbor)"),
                               static_cast<int>(VideoDisplayWidget::ScalingMode::Sharp));
    scalingModeCombo_->addItem(tr("Smooth (Bilinear)"),
                               static_cast<int>(VideoDisplayWidget::ScalingMode::Smooth));
    scalingModeCombo_->addItem(tr("Integer (Pixel Perfect)"),
                               static_cast<int>(VideoDisplayWidget::ScalingMode::Integer));
    viewLayout->addRow(tr("Scaling Mode:"), scalingModeCombo_);

    leftColumn->addWidget(viewGroup);
    leftColumn->addStretch();

    columnsLayout->addLayout(leftColumn);

    // ========== RIGHT COLUMN: Databases ==========
    auto *rightColumn = new QVBoxLayout();

    // HVSC Databases settings group
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
    connect(dbWidgets_.button, &QPushButton::clicked, this, &PreferencesDialog::onDownloadDatabase);
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
    connect(stilWidgets_.button, &QPushButton::clicked, this, &PreferencesDialog::onDownloadStil);
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
            &PreferencesDialog::onDownloadBuglist);
    buglistButtonLayout->addWidget(buglistWidgets_.button);
    buglistButtonLayout->addStretch();
    databaseLayout->addLayout(buglistButtonLayout);

    rightColumn->addWidget(databaseGroup);

    // GameBase64 Database settings group
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
            &PreferencesDialog::onDownloadGameBase64);
    gamebaseButtonLayout->addWidget(gameBase64Widgets_.button);
    gamebaseButtonLayout->addStretch();
    gamebaseLayout->addLayout(gamebaseButtonLayout);

    rightColumn->addWidget(gamebaseGroup);
    rightColumn->addStretch();

    columnsLayout->addLayout(rightColumn);

    mainLayout->addLayout(columnsLayout);

    // Buttons at the bottom
    auto *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setMinimumWidth(700);
}

void PreferencesDialog::loadSettings()
{
    QSettings settings;

    QString host = settings.value("device/host", "").toString();
    hostEdit_->setText(host);

    // Load password from secure storage
    if (!host.isEmpty()) {
        QString password = CredentialStore::retrievePassword("r64u", host);
        passwordEdit_->setText(password);
    }

    autoConnectCheck_->setChecked(settings.value("device/autoConnect", false).toBool());

    QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
    localDirEdit_->setText(settings.value("directories/local", homePath).toString());

    defaultDriveCombo_->setCurrentIndex(settings.value("drive/defaultDrive", 0).toInt());

    QString mountMode = settings.value("drive/mountMode", "readwrite").toString();
    int mountIndex = mountModeCombo_->findData(mountMode);
    if (mountIndex >= 0) {
        mountModeCombo_->setCurrentIndex(mountIndex);
    }

    // Load view settings (default to Integer)
    int scalingMode =
        settings
            .value("view/scalingMode", static_cast<int>(VideoDisplayWidget::ScalingMode::Integer))
            .toInt();
    int scalingIndex = scalingModeCombo_->findData(scalingMode);
    if (scalingIndex >= 0) {
        scalingModeCombo_->setCurrentIndex(scalingIndex);
    }
}

void PreferencesDialog::saveSettings()
{
    QSettings settings;

    QString host = hostEdit_->text().trimmed();
    settings.setValue("device/host", host);

    // Save password to secure storage (Keychain on macOS)
    if (!host.isEmpty()) {
        CredentialStore::storePassword("r64u", host, passwordEdit_->text());
    }

    // Remove the insecure password from QSettings if it exists
    settings.remove("device/password");

    settings.setValue("device/autoConnect", autoConnectCheck_->isChecked());

    settings.setValue("directories/local", localDirEdit_->text());

    settings.setValue("drive/defaultDrive", defaultDriveCombo_->currentIndex());
    settings.setValue("drive/mountMode", mountModeCombo_->currentData().toString());

    // Save view settings
    settings.setValue("view/scalingMode", scalingModeCombo_->currentData().toInt());
}

void PreferencesDialog::onAccept()
{
    saveSettings();
    accept();
}

void PreferencesDialog::onTestConnection()
{
    QString host = hostEdit_->text().trimmed();
    if (host.isEmpty()) {
        QMessageBox::warning(this, tr("Test Connection"), tr("Please enter a host address."));
        return;
    }

    // Clean up any existing test client
    if (testClient_) {
        testClient_->deleteLater();
    }

    testClient_ = new C64URestClient(this);
    testClient_->setHost(host);
    testClient_->setPassword(passwordEdit_->text());

    connect(testClient_, &IRestClient::infoReceived, this,
            &PreferencesDialog::onTestConnectionSuccess);
    connect(testClient_, &IRestClient::connectionError, this,
            &PreferencesDialog::onTestConnectionError);

    // Show waiting cursor
    QApplication::setOverrideCursor(Qt::WaitCursor);

    testClient_->getInfo();
}

void PreferencesDialog::onTestConnectionSuccess(const DeviceInfo &info)
{
    QApplication::restoreOverrideCursor();

    QString message = tr("Connection successful!\n\n"
                         "Device: %1\n"
                         "Firmware: %2\n"
                         "Hostname: %3")
                          .arg(info.product)
                          .arg(info.firmwareVersion)
                          .arg(info.hostname);

    QMessageBox::information(this, tr("Test Connection"), message);

    if (testClient_) {
        testClient_->deleteLater();
        testClient_ = nullptr;
    }
}

void PreferencesDialog::onTestConnectionError(const QString &error)
{
    QApplication::restoreOverrideCursor();

    QMessageBox::critical(this, tr("Test Connection"), tr("Connection failed:\n%1").arg(error));

    if (testClient_) {
        testClient_->deleteLater();
        testClient_ = nullptr;
    }
}

// ============================================================
// Shared download helpers
// ============================================================

void PreferencesDialog::startDownload(DownloadWidgetGroup &group)
{
    group.button->setEnabled(false);
    group.progressBar->setVisible(true);
    group.progressBar->setValue(0);
    group.statusLabel->setText(tr("Downloading..."));
}

void PreferencesDialog::handleDownloadProgress(DownloadWidgetGroup &group, qint64 bytesReceived,
                                               qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        group.progressBar->setMaximum(static_cast<int>(bytesTotal));
        group.progressBar->setValue(static_cast<int>(bytesReceived));
    } else {
        group.progressBar->setMaximum(0);  // Indeterminate
    }
}

void PreferencesDialog::handleDownloadFinished(DownloadWidgetGroup &group, int count)
{
    group.button->setEnabled(true);
    group.progressBar->setVisible(false);
    group.statusLabel->setText(tr("%1 %2 loaded").arg(count).arg(group.unitName));

    QMessageBox::information(this, tr("Download Complete"),
                             tr("Successfully downloaded %1.\n%2 %3 loaded.")
                                 .arg(group.itemName)
                                 .arg(count)
                                 .arg(group.unitName));
}

void PreferencesDialog::handleDownloadFailed(DownloadWidgetGroup &group, const QString &error)
{
    group.button->setEnabled(true);
    group.progressBar->setVisible(false);
    group.statusLabel->setText(tr("Download failed"));

    QMessageBox::warning(this, tr("Download Failed"),
                         tr("Failed to download %1:\n%2").arg(group.itemName).arg(error));
}

// ============================================================
// SonglengthsDatabase
// ============================================================

void PreferencesDialog::setSonglengthsDatabase(SonglengthsDatabase *database)
{
    // Disconnect from any previous database
    if (songlengthsDatabase_ != nullptr) {
        disconnect(songlengthsDatabase_, nullptr, this, nullptr);
    }

    songlengthsDatabase_ = database;

    if (songlengthsDatabase_ != nullptr) {
        // Connect signals
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadProgress, this,
                &PreferencesDialog::onDatabaseDownloadProgress);
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadFinished, this,
                &PreferencesDialog::onDatabaseDownloadFinished);
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadFailed, this,
                &PreferencesDialog::onDatabaseDownloadFailed);

        // Update status label
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

void PreferencesDialog::onDownloadDatabase()
{
    if (songlengthsDatabase_ == nullptr) {
        QMessageBox::warning(this, tr("Download Database"), tr("Database service not available."));
        return;
    }

    startDownload(dbWidgets_);
    songlengthsDatabase_->downloadDatabase();
}

void PreferencesDialog::onDatabaseDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(dbWidgets_, bytesReceived, bytesTotal);
}

void PreferencesDialog::onDatabaseDownloadFinished(int entryCount)
{
    handleDownloadFinished(dbWidgets_, entryCount);
}

void PreferencesDialog::onDatabaseDownloadFailed(const QString &error)
{
    handleDownloadFailed(dbWidgets_, error);
}

// ============================================================
// HVSCMetadataService (STIL + BUGlist)
// ============================================================

void PreferencesDialog::setHVSCMetadataService(HVSCMetadataService *service)
{
    // Disconnect from any previous service
    if (hvscMetadataService_ != nullptr) {
        disconnect(hvscMetadataService_, nullptr, this, nullptr);
    }

    hvscMetadataService_ = service;

    if (hvscMetadataService_ != nullptr) {
        // Connect STIL signals
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadProgress, this,
                &PreferencesDialog::onStilDownloadProgress);
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadFinished, this,
                &PreferencesDialog::onStilDownloadFinished);
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadFailed, this,
                &PreferencesDialog::onStilDownloadFailed);

        // Connect BUGlist signals
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadProgress, this,
                &PreferencesDialog::onBuglistDownloadProgress);
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadFinished, this,
                &PreferencesDialog::onBuglistDownloadFinished);
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadFailed, this,
                &PreferencesDialog::onBuglistDownloadFailed);

        // Update STIL status label
        if (hvscMetadataService_->isStilLoaded()) {
            stilWidgets_.statusLabel->setText(
                tr("%1 entries loaded").arg(hvscMetadataService_->stilEntryCount()));
        } else if (hvscMetadataService_->hasCachedStil()) {
            stilWidgets_.statusLabel->setText(tr("Cached (not loaded)"));
        } else {
            stilWidgets_.statusLabel->setText(tr("Not downloaded"));
        }

        // Update BUGlist status label
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

void PreferencesDialog::onDownloadStil()
{
    if (hvscMetadataService_ == nullptr) {
        QMessageBox::warning(this, tr("Download STIL"), tr("HVSC metadata service not available."));
        return;
    }

    startDownload(stilWidgets_);
    hvscMetadataService_->downloadStil();
}

void PreferencesDialog::onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(stilWidgets_, bytesReceived, bytesTotal);
}

void PreferencesDialog::onStilDownloadFinished(int entryCount)
{
    handleDownloadFinished(stilWidgets_, entryCount);
}

void PreferencesDialog::onStilDownloadFailed(const QString &error)
{
    handleDownloadFailed(stilWidgets_, error);
}

void PreferencesDialog::onDownloadBuglist()
{
    if (hvscMetadataService_ == nullptr) {
        QMessageBox::warning(this, tr("Download BUGlist"),
                             tr("HVSC metadata service not available."));
        return;
    }

    startDownload(buglistWidgets_);
    hvscMetadataService_->downloadBuglist();
}

void PreferencesDialog::onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(buglistWidgets_, bytesReceived, bytesTotal);
}

void PreferencesDialog::onBuglistDownloadFinished(int entryCount)
{
    handleDownloadFinished(buglistWidgets_, entryCount);
}

void PreferencesDialog::onBuglistDownloadFailed(const QString &error)
{
    handleDownloadFailed(buglistWidgets_, error);
}

// ============================================================
// GameBase64Service
// ============================================================

void PreferencesDialog::setGameBase64Service(GameBase64Service *service)
{
    // Disconnect from any previous service
    if (gameBase64Service_ != nullptr) {
        disconnect(gameBase64Service_, nullptr, this, nullptr);
    }

    gameBase64Service_ = service;

    if (gameBase64Service_ != nullptr) {
        // Connect signals
        connect(gameBase64Service_, &GameBase64Service::downloadProgress, this,
                &PreferencesDialog::onGameBase64DownloadProgress);
        connect(gameBase64Service_, &GameBase64Service::downloadFinished, this,
                &PreferencesDialog::onGameBase64DownloadFinished);
        connect(gameBase64Service_, &GameBase64Service::downloadFailed, this,
                &PreferencesDialog::onGameBase64DownloadFailed);

        // Update status label
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

void PreferencesDialog::onDownloadGameBase64()
{
    if (gameBase64Service_ == nullptr) {
        QMessageBox::warning(this, tr("Download GameBase64"),
                             tr("GameBase64 service not available."));
        return;
    }

    startDownload(gameBase64Widgets_);
    gameBase64Service_->downloadDatabase();
}

void PreferencesDialog::onGameBase64DownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    handleDownloadProgress(gameBase64Widgets_, bytesReceived, bytesTotal);
}

void PreferencesDialog::onGameBase64DownloadFinished(int gameCount)
{
    handleDownloadFinished(gameBase64Widgets_, gameCount);
}

void PreferencesDialog::onGameBase64DownloadFailed(const QString &error)
{
    handleDownloadFailed(gameBase64Widgets_, error);
}
