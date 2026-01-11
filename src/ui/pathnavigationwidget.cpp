#include "pathnavigationwidget.h"

#include <QHBoxLayout>

PathNavigationWidget::PathNavigationWidget(const QString &prefix, QWidget *parent)
    : QWidget(parent)
    , prefix_(prefix)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    upButton_ = new QPushButton(tr("\u2191 Up"));
    upButton_->setToolTip(tr("Go to parent folder"));
    connect(upButton_, &QPushButton::clicked, this, &PathNavigationWidget::upClicked);
    layout->addWidget(upButton_);

    pathLabel_ = new QLabel();
    pathLabel_->setWordWrap(true);
    layout->addWidget(pathLabel_, 1);

    setStyleBlue();
    setPath("/");
}

void PathNavigationWidget::setPath(const QString &path)
{
    currentPath_ = path;
    pathLabel_->setText(QString("%1 %2").arg(prefix_, path));
}

QString PathNavigationWidget::path() const
{
    return currentPath_;
}

void PathNavigationWidget::setUpEnabled(bool enabled)
{
    upButton_->setEnabled(enabled);
}

void PathNavigationWidget::setStyleBlue()
{
    pathLabel_->setStyleSheet(
        "color: #0066cc; padding: 2px; background-color: #f0f8ff; border-radius: 3px;");
}

void PathNavigationWidget::setStyleGreen()
{
    pathLabel_->setStyleSheet(
        "color: #006600; padding: 2px; background-color: #f0fff0; border-radius: 3px;");
}
