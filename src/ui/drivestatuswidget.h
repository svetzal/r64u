#ifndef DRIVESTATUSWIDGET_H
#define DRIVESTATUSWIDGET_H

#include <QLabel>
#include <QToolButton>
#include <QWidget>

class DriveStatusWidget : public QWidget
{
    Q_OBJECT

public:
    explicit DriveStatusWidget(const QString &driveName, QWidget *parent = nullptr);

    /// Shows the mounted image name, elided to fit; the full name goes in the tooltip.
    void setImageName(const QString &imageName);
    void setMounted(bool mounted);
    bool isMounted() const;

signals:
    void ejectClicked();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    QLabel *driveLabel_ = nullptr;
    QLabel *imageLabel_ = nullptr;
    QLabel *indicator_ = nullptr;
    QToolButton *ejectButton_ = nullptr;
    QString imageName_;
    bool mounted_ = false;

    void updateDisplay();
    void updateImageLabel();
};

#endif  // DRIVESTATUSWIDGET_H
