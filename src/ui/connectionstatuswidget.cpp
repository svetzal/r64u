#include "connectionstatuswidget.h"

#include "core/themecore.h"

#include <QHBoxLayout>

namespace {
constexpr int kLedSize = 10;  // visual.md 6.6
}

ConnectionStatusWidget::ConnectionStatusWidget(QWidget *parent) : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(4, 0, 4, 0);
    layout->setSpacing(8);

    indicator_ = new QLabel();
    indicator_->setFixedSize(kLedSize, kLedSize);
    layout->addWidget(indicator_);

    statusLabel_ = new QLabel(tr("Disconnected"));
    layout->addWidget(statusLabel_);

    hostnameLabel_ = new QLabel();
    hostnameLabel_->setVisible(false);
    layout->addWidget(hostnameLabel_);

    firmwareLabel_ = new QLabel();
    firmwareLabel_->setVisible(false);
    layout->addWidget(firmwareLabel_);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { updateDisplay(); });

    updateDisplay();
}

void ConnectionStatusWidget::setConnected(bool connected)
{
    connected_ = connected;
    connecting_ = false;
    if (!connected_) {
        hostnameLabel_->clear();
        hostnameLabel_->setVisible(false);
        firmwareLabel_->clear();
        firmwareLabel_->setVisible(false);
    }
    updateDisplay();
}

void ConnectionStatusWidget::setConnecting(bool connecting)
{
    connecting_ = connecting;
    if (connecting_) {
        connected_ = false;
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
    const themecore::Tokens tokens = themecore::currentTokens();

    QColor led = tokens.ledOff;
    QString status = tr("Disconnected");
    if (connecting_) {
        led = tokens.stateWarning;
        status = tr("Connecting…");
    } else if (connected_) {
        led = tokens.stateConnected;
        status = tr("Connected");
    }

    statusLabel_->setText(status);
    indicator_->setStyleSheet(QStringLiteral("background-color: %1; border-radius: %2px;")
                                  .arg(led.name())
                                  .arg(kLedSize / 2));
    firmwareLabel_->setStyleSheet(QStringLiteral("color: %1;").arg(tokens.textMuted.name()));
}
