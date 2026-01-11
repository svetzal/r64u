#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>

#include "../services/c64urestclient.h"
#include "videodisplaywidget.h"

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog() override;

private slots:
    void onAccept();
    void onTestConnection();
    void onTestConnectionSuccess(const DeviceInfo &info);
    void onTestConnectionError(const QString &error);

private:
    void setupUi();
    void loadSettings();
    void saveSettings();

    // Device settings
    QLineEdit *hostEdit_ = nullptr;
    QLineEdit *passwordEdit_ = nullptr;
    QCheckBox *autoConnectCheck_ = nullptr;

    // Appearance settings
    QComboBox *themeCombo_ = nullptr;

    // Application settings
    QLineEdit *localDirEdit_ = nullptr;
    QComboBox *defaultDriveCombo_ = nullptr;
    QComboBox *mountModeCombo_ = nullptr;

    // View settings
    QComboBox *scalingModeCombo_ = nullptr;

    // Test connection
    C64URestClient *testClient_ = nullptr;
};

#endif // PREFERENCESDIALOG_H
