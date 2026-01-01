#include "preferencesdialog.h"

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
    downloadPathEdit_ = new QLineEdit();
    downloadPathEdit_->setReadOnly(true);
    pathLayout->addWidget(downloadPathEdit_);

    auto *browseButton = new QPushButton(tr("Browse..."));
    connect(browseButton, &QPushButton::clicked, this, [this]() {
        QString path = QFileDialog::getExistingDirectory(this,
            tr("Select Download Directory"),
            downloadPathEdit_->text());
        if (!path.isEmpty()) {
            downloadPathEdit_->setText(path);
        }
    });
    pathLayout->addWidget(browseButton);

    appLayout->addRow(tr("Download Path:"), pathLayout);

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

    hostEdit_->setText(settings.value("device/host", "").toString());
    // Note: Password should be loaded from secure storage in production
    passwordEdit_->setText(settings.value("device/password", "").toString());
    autoConnectCheck_->setChecked(settings.value("device/autoConnect", false).toBool());

    QString defaultDownloadPath = QStandardPaths::writableLocation(
        QStandardPaths::DownloadLocation);
    downloadPathEdit_->setText(
        settings.value("app/downloadPath", defaultDownloadPath).toString());

    defaultDriveCombo_->setCurrentIndex(
        settings.value("drive/defaultDrive", 0).toInt());

    QString mountMode = settings.value("drive/mountMode", "readwrite").toString();
    int mountIndex = mountModeCombo_->findData(mountMode);
    if (mountIndex >= 0) {
        mountModeCombo_->setCurrentIndex(mountIndex);
    }
}

void PreferencesDialog::saveSettings()
{
    QSettings settings;

    settings.setValue("device/host", hostEdit_->text());
    // Note: Password should be saved to secure storage in production
    settings.setValue("device/password", passwordEdit_->text());
    settings.setValue("device/autoConnect", autoConnectCheck_->isChecked());

    settings.setValue("app/downloadPath", downloadPathEdit_->text());

    settings.setValue("drive/defaultDrive", defaultDriveCombo_->currentIndex());
    settings.setValue("drive/mountMode",
        mountModeCombo_->currentData().toString());
}

void PreferencesDialog::onAccept()
{
    saveSettings();
    accept();
}

void PreferencesDialog::onTestConnection()
{
    // TODO: Implement connection test using REST client
    QMessageBox::information(this, tr("Test Connection"),
        tr("Connection test not yet implemented.\n"
           "Will test connection to: %1").arg(hostEdit_->text()));
}
