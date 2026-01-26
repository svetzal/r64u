#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>
#include <QLabel>
#include <QPushButton>
#include <QProgressBar>

#include "../services/c64urestclient.h"
#include "videodisplaywidget.h"

class SonglengthsDatabase;
class HVSCMetadataService;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog() override;

    void setSonglengthsDatabase(SonglengthsDatabase *database);
    void setHVSCMetadataService(HVSCMetadataService *service);

private slots:
    void onAccept();
    void onTestConnection();
    void onTestConnectionSuccess(const DeviceInfo &info);
    void onTestConnectionError(const QString &error);

    // Songlengths Database slots
    void onDownloadDatabase();
    void onDatabaseDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDatabaseDownloadFinished(int entryCount);
    void onDatabaseDownloadFailed(const QString &error);

    // STIL/BUGlist slots
    void onDownloadStil();
    void onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onStilDownloadFinished(int entryCount);
    void onStilDownloadFailed(const QString &error);

    void onDownloadBuglist();
    void onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onBuglistDownloadFinished(int entryCount);
    void onBuglistDownloadFailed(const QString &error);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // Device settings
    QLineEdit *hostEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QCheckBox *autoConnectCheck_ = nullptr;

    // Application settings
    QLineEdit *localDirEdit_ = nullptr;
    QComboBox *defaultDriveCombo_ = nullptr;
    QComboBox *mountModeCombo_ = nullptr;

    // View settings
    QComboBox *scalingModeCombo_ = nullptr;

    // Test connection
    C64URestClient *testClient_ = nullptr;

    // Songlengths Database UI
    SonglengthsDatabase *songlengthsDatabase_ = nullptr;
    QLabel *databaseStatusLabel_ = nullptr;
    QPushButton *downloadDatabaseButton_ = nullptr;
    QProgressBar *databaseProgressBar_ = nullptr;

    // HVSC Metadata Service UI
    HVSCMetadataService *hvscMetadataService_ = nullptr;
    QLabel *stilStatusLabel_ = nullptr;
    QPushButton *downloadStilButton_ = nullptr;
    QProgressBar *stilProgressBar_ = nullptr;
    QLabel *buglistStatusLabel_ = nullptr;
    QPushButton *downloadBuglistButton_ = nullptr;
    QProgressBar *buglistProgressBar_ = nullptr;
};

#endif // PREFERENCESDIALOG_H
