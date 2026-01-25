#include "preferencesdialog.h"
#include "../services/c64urestclient.h"
#include "../services/credentialstore.h"
#include "../services/songlengthsdatabase.h"

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

    // HVSC Database settings group
    auto *databaseGroup = new QGroupBox(tr("HVSC Songlengths Database"));
    auto *databaseLayout = new QVBoxLayout(databaseGroup);

    databaseStatusLabel_ = new QLabel(tr("Database: Not loaded"));
    databaseLayout->addWidget(databaseStatusLabel_);

    databaseProgressBar_ = new QProgressBar();
    databaseProgressBar_->setVisible(false);
    databaseLayout->addWidget(databaseProgressBar_);

    auto *buttonLayout = new QHBoxLayout();
    downloadDatabaseButton_ = new QPushButton(tr("Download/Update Database"));
    downloadDatabaseButton_->setToolTip(tr("Download the HVSC Songlengths database for accurate SID song durations"));
    connect(downloadDatabaseButton_, &QPushButton::clicked,
            this, &PreferencesDialog::onDownloadDatabase);
    buttonLayout->addWidget(downloadDatabaseButton_);
    buttonLayout->addStretch();
    databaseLayout->addLayout(buttonLayout);

    mainLayout->addWidget(databaseGroup);

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
