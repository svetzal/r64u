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

    /// Shows the path as a blue badge (remote locations).
    void setStyleBlue();
    /// Shows the path as a green badge (local locations).
    void setStyleGreen();

signals:
    void upClicked();

private:
    enum class Accent { Blue, Green };

    void applyAccent();

    QString prefix_;
    QString currentPath_;
    Accent accent_ = Accent::Blue;
    QPushButton *upButton_ = nullptr;
    QLabel *pathLabel_ = nullptr;
};

#endif  // PATHNAVIGATIONWIDGET_H
