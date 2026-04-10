#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include "videodisplaywidget.h"

#include "../services/irestclient.h"

#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QLineEdit>

class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;
class DatabaseDownloadController;
class ConnectionTester;

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

    // Extracted controllers (owned via Qt parent)
    DatabaseDownloadController *downloadController_ = nullptr;
    ConnectionTester *connectionTester_ = nullptr;
};

#endif  // PREFERENCESDIALOG_H
