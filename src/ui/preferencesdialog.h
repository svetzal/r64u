#ifndef PREFERENCESDIALOG_H
#define PREFERENCESDIALOG_H

#include <QDialog>
#include <QLineEdit>
#include <QCheckBox>
#include <QComboBox>

class PreferencesDialog : public QDialog
{
    Q_OBJECT

public:
    explicit PreferencesDialog(QWidget *parent = nullptr);
    ~PreferencesDialog() override;

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
    QLineEdit *downloadPathEdit_ = nullptr;
    QComboBox *defaultDriveCombo_ = nullptr;
    QComboBox *mountModeCombo_ = nullptr;
};

#endif // PREFERENCESDIALOG_H
