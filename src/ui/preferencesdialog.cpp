#include "preferencesdialog.h"
#include "../services/c64urestclient.h"
#include "../services/credentialstore.h"
#include "../services/songlengthsdatabase.h"
#include "../services/hvscmetadataservice.h"
#include "../services/gamebase64service.h"

#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFormLayout>
#include <QGroupBox>
#include <QDialogButtonBox>
#include <QPushButton>
#include <QLabel>
#include <QFileDialog>
#include <QSettings>
#include <QStandardPaths>
#include <QMessageBox>
#include <QApplication>

PreferencesDialog::PreferencesDialog(QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(tr("Preferences"));
    setupUi();
    loadSettings();
}

PreferencesDialog::~PreferencesDialog() = default;

void PreferencesDialog::setupUi()
{
    auto *mainLayout = new QVBoxLayout(this);

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

    mainLayout->addWidget(deviceGroup);

    // Application settings group
    auto *appGroup = new QGroupBox(tr("Application"));
    auto *appLayout = new QFormLayout(appGroup);

    auto *pathLayout = new QHBoxLayout();
    localDirEdit_ = new QLineEdit();
    localDirEdit_->setReadOnly(true);
    pathLayout->addWidget(localDirEdit_);

    auto *browseButton = new QPushButton(tr("Browse..."));
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this,
            tr("Select Local Directory"),
            localDirEdit_->text());
        if (!path.isEmpty()) {
            localDirEdit_->setText(path);
        }
    });
    pathLayout->addWidget(browseButton);

    appLayout->addRow(tr("Local Directory:"), pathLayout);

    mainLayout->addWidget(appGroup);

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

    mainLayout->addWidget(driveGroup);

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

    mainLayout->addWidget(viewGroup);

    // HVSC Databases settings group
    auto *databaseGroup = new QGroupBox(tr("HVSC Databases"));
    auto *databaseLayout = new QVBoxLayout(databaseGroup);

    // Songlengths section
    auto *songlengthsLabel = new QLabel(tr("<b>Songlengths</b> - Accurate song durations"));
    databaseLayout->addWidget(songlengthsLabel);

    databaseStatusLabel_ = new QLabel(tr("Not loaded"));
    databaseLayout->addWidget(databaseStatusLabel_);

    databaseProgressBar_ = new QProgressBar();
    databaseProgressBar_->setVisible(false);
    databaseLayout->addWidget(databaseProgressBar_);

    auto *songlengthsButtonLayout = new QHBoxLayout();
    downloadDatabaseButton_ = new QPushButton(tr("Download/Update"));
    downloadDatabaseButton_->setToolTip(tr("Download the HVSC Songlengths database for accurate SID song durations"));
    connect(downloadDatabaseButton_, &QPushButton::clicked,
            this, &PreferencesDialog::onDownloadDatabase);
    songlengthsButtonLayout->addWidget(downloadDatabaseButton_);
    songlengthsButtonLayout->addStretch();
    databaseLayout->addLayout(songlengthsButtonLayout);

    databaseLayout->addSpacing(12);

    // STIL section
    auto *stilLabel = new QLabel(tr("<b>STIL</b> - Tune commentary and cover info"));
    databaseLayout->addWidget(stilLabel);

    stilStatusLabel_ = new QLabel(tr("Not loaded"));
    databaseLayout->addWidget(stilStatusLabel_);

    stilProgressBar_ = new QProgressBar();
    stilProgressBar_->setVisible(false);
    databaseLayout->addWidget(stilProgressBar_);

    auto *stilButtonLayout = new QHBoxLayout();
    downloadStilButton_ = new QPushButton(tr("Download/Update"));
    downloadStilButton_->setToolTip(tr("Download STIL.txt for tune commentary, history, and cover information"));
    connect(downloadStilButton_, &QPushButton::clicked,
            this, &PreferencesDialog::onDownloadStil);
    stilButtonLayout->addWidget(downloadStilButton_);
    stilButtonLayout->addStretch();
    databaseLayout->addLayout(stilButtonLayout);

    databaseLayout->addSpacing(12);

    // BUGlist section
    auto *buglistLabel = new QLabel(tr("<b>BUGlist</b> - Known playback issues"));
    databaseLayout->addWidget(buglistLabel);

    buglistStatusLabel_ = new QLabel(tr("Not loaded"));
    databaseLayout->addWidget(buglistStatusLabel_);

    buglistProgressBar_ = new QProgressBar();
    buglistProgressBar_->setVisible(false);
    databaseLayout->addWidget(buglistProgressBar_);

    auto *buglistButtonLayout = new QHBoxLayout();
    downloadBuglistButton_ = new QPushButton(tr("Download/Update"));
    downloadBuglistButton_->setToolTip(tr("Download BUGlist.txt for known SID playback issues"));
    connect(downloadBuglistButton_, &QPushButton::clicked,
            this, &PreferencesDialog::onDownloadBuglist);
    buglistButtonLayout->addWidget(downloadBuglistButton_);
    buglistButtonLayout->addStretch();
    databaseLayout->addLayout(buglistButtonLayout);

    mainLayout->addWidget(databaseGroup);

    // GameBase64 Database settings group
    auto *gamebaseGroup = new QGroupBox(tr("GameBase64 Database"));
    auto *gamebaseLayout = new QVBoxLayout(gamebaseGroup);

    auto *gamebaseLabel = new QLabel(tr("<b>Game Database</b> - ~29,000 C64 games with metadata"));
    gamebaseLayout->addWidget(gamebaseLabel);

    gameBase64StatusLabel_ = new QLabel(tr("Not loaded"));
    gamebaseLayout->addWidget(gameBase64StatusLabel_);

    gameBase64ProgressBar_ = new QProgressBar();
    gameBase64ProgressBar_->setVisible(false);
    gamebaseLayout->addWidget(gameBase64ProgressBar_);

    auto *gamebaseButtonLayout = new QHBoxLayout();
    downloadGameBase64Button_ = new QPushButton(tr("Download/Update"));
    downloadGameBase64Button_->setToolTip(tr("Download GameBase64 database for game information (publisher, year, genre, etc.)"));
    connect(downloadGameBase64Button_, &QPushButton::clicked,
            this, &PreferencesDialog::onDownloadGameBase64);
    gamebaseButtonLayout->addWidget(downloadGameBase64Button_);
    gamebaseButtonLayout->addStretch();
    gamebaseLayout->addLayout(gamebaseButtonLayout);

    mainLayout->addWidget(gamebaseGroup);

    // Buttons
    mainLayout->addStretch();

    auto *buttonBox = new QDialogButtonBox(
        QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    connect(buttonBox, &QDialogButtonBox::accepted, this, &PreferencesDialog::onAccept);
    connect(buttonBox, &QDialogButtonBox::rejected, this, &QDialog::reject);
    mainLayout->addWidget(buttonBox);

    setMinimumWidth(400);
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

    QString homePath = QStandardPaths::writableLocation(
        QStandardPaths::HomeLocation);
    localDirEdit_->setText(
        settings.value("directories/local", homePath).toString());

    defaultDriveCombo_->setCurrentIndex(
        settings.value("drive/defaultDrive", 0).toInt());

    QString mountMode = settings.value("drive/mountMode", "readwrite").toString();
    int mountIndex = mountModeCombo_->findData(mountMode);
    if (mountIndex >= 0) {
        mountModeCombo_->setCurrentIndex(mountIndex);
    }

    // Load view settings (default to Integer)
    int scalingMode = settings.value("view/scalingMode",
        static_cast<int>(VideoDisplayWidget::ScalingMode::Integer)).toInt();
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
    settings.setValue("drive/mountMode",
        mountModeCombo_->currentData().toString());

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
        QMessageBox::warning(this, tr("Test Connection"),
            tr("Please enter a host address."));
        return;
    }

    // Clean up any existing test client
    if (testClient_) {
        testClient_->deleteLater();
    }

    testClient_ = new C64URestClient(this);
    testClient_->setHost(host);
    testClient_->setPassword(passwordEdit_->text());

    connect(testClient_, &C64URestClient::infoReceived,
            this, &PreferencesDialog::onTestConnectionSuccess);
    connect(testClient_, &C64URestClient::connectionError,
            this, &PreferencesDialog::onTestConnectionError);

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

    QMessageBox::critical(this, tr("Test Connection"),
        tr("Connection failed:\n%1").arg(error));

    if (testClient_) {
        testClient_->deleteLater();
        testClient_ = nullptr;
    }
}

void PreferencesDialog::setSonglengthsDatabase(SonglengthsDatabase *database)
{
    // Disconnect from any previous database
    if (songlengthsDatabase_ != nullptr) {
        disconnect(songlengthsDatabase_, nullptr, this, nullptr);
    }

    songlengthsDatabase_ = database;

    if (songlengthsDatabase_ != nullptr) {
        // Connect signals
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadProgress,
                this, &PreferencesDialog::onDatabaseDownloadProgress);
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadFinished,
                this, &PreferencesDialog::onDatabaseDownloadFinished);
        connect(songlengthsDatabase_, &SonglengthsDatabase::downloadFailed,
                this, &PreferencesDialog::onDatabaseDownloadFailed);

        // Update status label
        if (songlengthsDatabase_->isLoaded()) {
            databaseStatusLabel_->setText(tr("Database: %1 entries loaded")
                .arg(songlengthsDatabase_->entryCount()));
        } else if (songlengthsDatabase_->hasCachedDatabase()) {
            databaseStatusLabel_->setText(tr("Database: Cached (not loaded)"));
        } else {
            databaseStatusLabel_->setText(tr("Database: Not downloaded"));
        }
    }
}

void PreferencesDialog::onDownloadDatabase()
{
    if (songlengthsDatabase_ == nullptr) {
        QMessageBox::warning(this, tr("Download Database"),
            tr("Database service not available."));
        return;
    }

    downloadDatabaseButton_->setEnabled(false);
    databaseProgressBar_->setVisible(true);
    databaseProgressBar_->setValue(0);
    databaseStatusLabel_->setText(tr("Downloading..."));

    songlengthsDatabase_->downloadDatabase();
}

void PreferencesDialog::onDatabaseDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        databaseProgressBar_->setMaximum(static_cast<int>(bytesTotal));
        databaseProgressBar_->setValue(static_cast<int>(bytesReceived));
    } else {
        databaseProgressBar_->setMaximum(0);  // Indeterminate
    }
}

void PreferencesDialog::onDatabaseDownloadFinished(int entryCount)
{
    downloadDatabaseButton_->setEnabled(true);
    databaseProgressBar_->setVisible(false);
    databaseStatusLabel_->setText(tr("Database: %1 entries loaded").arg(entryCount));

    QMessageBox::information(this, tr("Download Complete"),
        tr("Successfully downloaded HVSC Songlengths database.\n%1 entries loaded.")
            .arg(entryCount));
}

void PreferencesDialog::onDatabaseDownloadFailed(const QString &error)
{
    downloadDatabaseButton_->setEnabled(true);
    databaseProgressBar_->setVisible(false);
    databaseStatusLabel_->setText(tr("Database: Download failed"));

    QMessageBox::warning(this, tr("Download Failed"),
        tr("Failed to download database:\n%1").arg(error));
}

void PreferencesDialog::setHVSCMetadataService(HVSCMetadataService *service)
{
    // Disconnect from any previous service
    if (hvscMetadataService_ != nullptr) {
        disconnect(hvscMetadataService_, nullptr, this, nullptr);
    }

    hvscMetadataService_ = service;

    if (hvscMetadataService_ != nullptr) {
        // Connect STIL signals
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadProgress,
                this, &PreferencesDialog::onStilDownloadProgress);
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadFinished,
                this, &PreferencesDialog::onStilDownloadFinished);
        connect(hvscMetadataService_, &HVSCMetadataService::stilDownloadFailed,
                this, &PreferencesDialog::onStilDownloadFailed);

        // Connect BUGlist signals
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadProgress,
                this, &PreferencesDialog::onBuglistDownloadProgress);
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadFinished,
                this, &PreferencesDialog::onBuglistDownloadFinished);
        connect(hvscMetadataService_, &HVSCMetadataService::buglistDownloadFailed,
                this, &PreferencesDialog::onBuglistDownloadFailed);

        // Update STIL status label
        if (hvscMetadataService_->isStilLoaded()) {
            stilStatusLabel_->setText(tr("%1 entries loaded")
                .arg(hvscMetadataService_->stilEntryCount()));
        } else if (hvscMetadataService_->hasCachedStil()) {
            stilStatusLabel_->setText(tr("Cached (not loaded)"));
        } else {
            stilStatusLabel_->setText(tr("Not downloaded"));
        }

        // Update BUGlist status label
        if (hvscMetadataService_->isBuglistLoaded()) {
            buglistStatusLabel_->setText(tr("%1 entries loaded")
                .arg(hvscMetadataService_->buglistEntryCount()));
        } else if (hvscMetadataService_->hasCachedBuglist()) {
            buglistStatusLabel_->setText(tr("Cached (not loaded)"));
        } else {
            buglistStatusLabel_->setText(tr("Not downloaded"));
        }
    }
}

void PreferencesDialog::onDownloadStil()
{
    if (hvscMetadataService_ == nullptr) {
        QMessageBox::warning(this, tr("Download STIL"),
            tr("HVSC metadata service not available."));
        return;
    }

    downloadStilButton_->setEnabled(false);
    stilProgressBar_->setVisible(true);
    stilProgressBar_->setValue(0);
    stilStatusLabel_->setText(tr("Downloading..."));

    hvscMetadataService_->downloadStil();
}

void PreferencesDialog::onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        stilProgressBar_->setMaximum(static_cast<int>(bytesTotal));
        stilProgressBar_->setValue(static_cast<int>(bytesReceived));
    } else {
        stilProgressBar_->setMaximum(0);  // Indeterminate
    }
}

void PreferencesDialog::onStilDownloadFinished(int entryCount)
{
    downloadStilButton_->setEnabled(true);
    stilProgressBar_->setVisible(false);
    stilStatusLabel_->setText(tr("%1 entries loaded").arg(entryCount));

    QMessageBox::information(this, tr("Download Complete"),
        tr("Successfully downloaded STIL database.\n%1 entries loaded.")
            .arg(entryCount));
}

void PreferencesDialog::onStilDownloadFailed(const QString &error)
{
    downloadStilButton_->setEnabled(true);
    stilProgressBar_->setVisible(false);
    stilStatusLabel_->setText(tr("Download failed"));

    QMessageBox::warning(this, tr("Download Failed"),
        tr("Failed to download STIL database:\n%1").arg(error));
}

void PreferencesDialog::onDownloadBuglist()
{
    if (hvscMetadataService_ == nullptr) {
        QMessageBox::warning(this, tr("Download BUGlist"),
            tr("HVSC metadata service not available."));
        return;
    }

    downloadBuglistButton_->setEnabled(false);
    buglistProgressBar_->setVisible(true);
    buglistProgressBar_->setValue(0);
    buglistStatusLabel_->setText(tr("Downloading..."));

    hvscMetadataService_->downloadBuglist();
}

void PreferencesDialog::onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        buglistProgressBar_->setMaximum(static_cast<int>(bytesTotal));
        buglistProgressBar_->setValue(static_cast<int>(bytesReceived));
    } else {
        buglistProgressBar_->setMaximum(0);  // Indeterminate
    }
}

void PreferencesDialog::onBuglistDownloadFinished(int entryCount)
{
    downloadBuglistButton_->setEnabled(true);
    buglistProgressBar_->setVisible(false);
    buglistStatusLabel_->setText(tr("%1 entries loaded").arg(entryCount));

    QMessageBox::information(this, tr("Download Complete"),
        tr("Successfully downloaded BUGlist database.\n%1 entries loaded.")
            .arg(entryCount));
}

void PreferencesDialog::onBuglistDownloadFailed(const QString &error)
{
    downloadBuglistButton_->setEnabled(true);
    buglistProgressBar_->setVisible(false);
    buglistStatusLabel_->setText(tr("Download failed"));

    QMessageBox::warning(this, tr("Download Failed"),
        tr("Failed to download BUGlist database:\n%1").arg(error));
}

void PreferencesDialog::setGameBase64Service(GameBase64Service *service)
{
    // Disconnect from any previous service
    if (gameBase64Service_ != nullptr) {
        disconnect(gameBase64Service_, nullptr, this, nullptr);
    }

    gameBase64Service_ = service;

    if (gameBase64Service_ != nullptr) {
        // Connect signals
        connect(gameBase64Service_, &GameBase64Service::downloadProgress,
                this, &PreferencesDialog::onGameBase64DownloadProgress);
        connect(gameBase64Service_, &GameBase64Service::downloadFinished,
                this, &PreferencesDialog::onGameBase64DownloadFinished);
        connect(gameBase64Service_, &GameBase64Service::downloadFailed,
                this, &PreferencesDialog::onGameBase64DownloadFailed);

        // Update status label
        if (gameBase64Service_->isLoaded()) {
            gameBase64StatusLabel_->setText(tr("%1 games loaded")
                .arg(gameBase64Service_->gameCount()));
        } else if (gameBase64Service_->hasCachedDatabase()) {
            gameBase64StatusLabel_->setText(tr("Cached (not loaded)"));
        } else {
            gameBase64StatusLabel_->setText(tr("Not downloaded"));
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

    downloadGameBase64Button_->setEnabled(false);
    gameBase64ProgressBar_->setVisible(true);
    gameBase64ProgressBar_->setValue(0);
    gameBase64StatusLabel_->setText(tr("Downloading..."));

    gameBase64Service_->downloadDatabase();
}

void PreferencesDialog::onGameBase64DownloadProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    if (bytesTotal > 0) {
        gameBase64ProgressBar_->setMaximum(static_cast<int>(bytesTotal));
        gameBase64ProgressBar_->setValue(static_cast<int>(bytesReceived));
    } else {
        gameBase64ProgressBar_->setMaximum(0);  // Indeterminate
    }
}

void PreferencesDialog::onGameBase64DownloadFinished(int gameCount)
{
    downloadGameBase64Button_->setEnabled(true);
    gameBase64ProgressBar_->setVisible(false);
    gameBase64StatusLabel_->setText(tr("%1 games loaded").arg(gameCount));

    QMessageBox::information(this, tr("Download Complete"),
        tr("Successfully downloaded GameBase64 database.\n%1 games loaded.")
            .arg(gameCount));
}

void PreferencesDialog::onGameBase64DownloadFailed(const QString &error)
{
    downloadGameBase64Button_->setEnabled(true);
    gameBase64ProgressBar_->setVisible(false);
    gameBase64StatusLabel_->setText(tr("Download failed"));

    QMessageBox::warning(this, tr("Download Failed"),
        tr("Failed to download GameBase64 database:\n%1").arg(error));
}
