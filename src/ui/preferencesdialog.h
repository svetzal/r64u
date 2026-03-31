#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include "videodisplaywidget.h"

#include "../services/irestclient.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QProgressBar>
#include <QPushButton>

class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog() override;

    void setSonglengthsDatabase(SonglengthsDatabase *database);
    void setHVSCMetadataService(HVSCMetadataService *service);
    void setGameBase64Service(GameBase64Service *service);

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

    // GameBase64 slots
    void onDownloadGameBase64();
    void onGameBase64DownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onGameBase64DownloadFinished(int gameCount);
    void onGameBase64DownloadFailed(const QString &error);

private:
    struct DownloadWidgetGroup
    {
        QPushButton *button = nullptr;
        QProgressBar *progressBar = nullptr;
        QLabel *statusLabel = nullptr;
        QString itemName;  ///< e.g., "HVSC Songlengths database"
        QString unitName;  ///< e.g., "entries"
    };

    void setupUi();
    void loadSettings();
    void saveSettings();

    void startDownload(DownloadWidgetGroup &group);
    void handleDownloadProgress(DownloadWidgetGroup &group, qint64 bytesReceived,
                                qint64 bytesTotal);
    void handleDownloadFinished(DownloadWidgetGroup &group, int count);
    void handleDownloadFailed(DownloadWidgetGroup &group, const QString &error);

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
    IRestClient *testClient_ = nullptr;

    // Download widget groups
    SonglengthsDatabase *songlengthsDatabase_ = nullptr;
    DownloadWidgetGroup dbWidgets_;

    HVSCMetadataService *hvscMetadataService_ = nullptr;
    DownloadWidgetGroup stilWidgets_;
    DownloadWidgetGroup buglistWidgets_;

    GameBase64Service *gameBase64Service_ = nullptr;
    DownloadWidgetGroup gameBase64Widgets_;
};

#endif  // PREFERENCESDIALOG_H
