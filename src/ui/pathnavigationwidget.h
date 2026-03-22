#ifndef PATHNAVIGATIONWIDGET_H
#define PATHNAVIGATIONWIDGET_H

#include <QLabel>
#include <QPushButton>
#include <QWidget>

class PathNavigationWidget : public QWidget
{
    Q_OBJECT

public:
    explicit PathNavigationWidget(QString prefix, QWidget *parent = nullptr);

    void setPath(const QString &path);
    QString path() const;
    void setUpEnabled(bool enabled);

    void setStyleBlue();
    void setStyleGreen();

signals:
    void upClicked();

private:
    QString prefix_;
    QString currentPath_;
    QPushButton *upButton_ = nullptr;
    QLabel *pathLabel_ = nullptr;
};

#endif  // PATHNAVIGATIONWIDGET_H
