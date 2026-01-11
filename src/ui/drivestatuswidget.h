#ifndef DRIVESTATUSWIDGET_H
#define DRIVESTATUSWIDGET_H

#include <QWidget>
#include <QLabel>
#include <QToolButton>

class DriveStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DriveStatusWidget(const QString &driveName, QWidget *parent = nullptr);

    void setImageName(const QString &imageName);
    void setMounted(bool mounted);
    bool isMounted() const;

signals:
    void ejectClicked();

private:
    QLabel *driveLabel_ = nullptr;
    QLabel *imageLabel_ = nullptr;
    QLabel *indicator_ = nullptr;
    QToolButton *ejectButton_ = nullptr;
    bool mounted_ = false;

    void updateDisplay();
};

#endif // DRIVESTATUSWIDGET_H
