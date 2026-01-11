#include "connectionstatuswidget.h"

#include <QHBoxLayout>

ConnectionStatusWidget::ConnectionStatusWidget(QWidget *parent)
    : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(8);

    statusLabel_ = new QLabel(tr("Disconnected"));
    layout->addWidget(statusLabel_);

    hostnameLabel_ = new QLabel();
    hostnameLabel_->setVisible(false);
    layout->addWidget(hostnameLabel_);

    firmwareLabel_ = new QLabel();
    firmwareLabel_->setStyleSheet("color: #666;");
    firmwareLabel_->setVisible(false);
    layout->addWidget(firmwareLabel_);

    indicator_ = new QLabel();
    indicator_->setFixedSize(12, 12);
    layout->addWidget(indicator_);

    updateDisplay();
}

void ConnectionStatusWidget::setConnected(bool connected)
{
    connected_ = connected;
    if (!connected_) {
        hostnameLabel_->clear();
        hostnameLabel_->setVisible(false);
        firmwareLabel_->clear();
        firmwareLabel_->setVisible(false);
    }
    updateDisplay();
}

void ConnectionStatusWidget::setHostname(const QString &hostname)
{
    hostnameLabel_->setText(hostname);
    hostnameLabel_->setVisible(!hostname.isEmpty() && connected_);
}

void ConnectionStatusWidget::setFirmwareVersion(const QString &version)
{
    firmwareLabel_->setText(QString("(%1)").arg(version));
    firmwareLabel_->setVisible(!version.isEmpty() && connected_);
}

void ConnectionStatusWidget::updateDisplay()
{
    statusLabel_->setText(connected_ ? tr("Connected") : tr("Disconnected"));

    QString color = connected_ ? "#22c55e" : "#ef4444";  // green-500 / red-500
    indicator_->setStyleSheet(
        QString("background-color: %1; border-radius: 6px;").arg(color));
}
