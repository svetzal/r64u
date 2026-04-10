#include "preferencesdialog.h"

#include "connectiontester.h"
#include "databasedownloadcontroller.h"

#include "../services/c64urestclient.h"
#include "../services/credentialstore.h"

#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QSettings>
#include <QStandardPaths>
#include <QVBoxLayout>

PreferencesDialog::PreferencesDialog(QWidget *parent) : QDialog(parent)
{
    downloadController_ = new DatabaseDownloadController(this, this);
    connectionTester_ = new ConnectionTester(this, this);

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
    rightColumn->addWidget(downloadController_->createWidgets());

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

    if (!host.isEmpty()) {
        CredentialStore::storePassword("r64u", host, passwordEdit_->text());
    }

    settings.remove("device/password");

    settings.setValue("device/autoConnect", autoConnectCheck_->isChecked());
    settings.setValue("directories/local", localDirEdit_->text());
    settings.setValue("drive/defaultDrive", defaultDriveCombo_->currentIndex());
    settings.setValue("drive/mountMode", mountModeCombo_->currentData().toString());
    settings.setValue("view/scalingMode", scalingModeCombo_->currentData().toInt());
}

void PreferencesDialog::onAccept()
{
    saveSettings();
    accept();
}

void PreferencesDialog::onTestConnection()
{
    connectionTester_->startTest(hostEdit_->text().trimmed(), passwordEdit_->text());
}

void PreferencesDialog::setSonglengthsDatabase(SonglengthsDatabase *database)
{
    downloadController_->setSonglengthsDatabase(database);
}

void PreferencesDialog::setHVSCMetadataService(HVSCMetadataService *service)
{
    downloadController_->setHVSCMetadataService(service);
}

void PreferencesDialog::setGameBase64Service(GameBase64Service *service)
{
    downloadController_->setGameBase64Service(service);
}
