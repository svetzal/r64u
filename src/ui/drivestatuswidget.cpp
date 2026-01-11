#include "drivestatuswidget.h"

#include <QHBoxLayout>

DriveStatusWidget::DriveStatusWidget(const QString &driveName, QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    driveLabel_ = new QLabel(driveName);
    driveLabel_->setStyleSheet("font-weight: bold;");
    layout->addWidget(driveLabel_);

    imageLabel_ = new QLabel(tr("[empty]"));
    imageLabel_->setStyleSheet("color: #666;");
    layout->addWidget(imageLabel_);

    layout->addStretch();  // Push indicator and eject button to the right

    indicator_ = new QLabel();
    indicator_->setFixedSize(8, 8);
    layout->addWidget(indicator_);

    ejectButton_ = new QToolButton();
    ejectButton_->setText(tr("\u23cf"));  // Eject symbol
    ejectButton_->setToolTip(tr("Eject"));
    ejectButton_->setAutoRaise(true);
    ejectButton_->setEnabled(false);  // Disabled until a disk is mounted
    connect(ejectButton_, &QToolButton::clicked, this, &DriveStatusWidget::ejectClicked);
    layout->addWidget(ejectButton_);

    updateDisplay();
}

void DriveStatusWidget::setImageName(const QString &imageName)
{
    if (imageName.isEmpty()) {
        imageLabel_->setText(tr("[empty]"));
        imageLabel_->setStyleSheet("color: #666;");
    } else {
        imageLabel_->setText(imageName);
        imageLabel_->setStyleSheet("");
    }
}

void DriveStatusWidget::setMounted(bool mounted)
{
    mounted_ = mounted;
    ejectButton_->setEnabled(mounted);
    updateDisplay();
}

bool DriveStatusWidget::isMounted() const
{
    return mounted_;
}

void DriveStatusWidget::updateDisplay()
{
    QString color = mounted_ ? "#22c55e" : "#9ca3af";  // green-500 / gray-400
    indicator_->setStyleSheet(
        QString("background-color: %1; border-radius: 4px;").arg(color));
}
