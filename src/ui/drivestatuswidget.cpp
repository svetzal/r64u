#include "drivestatuswidget.h"

#include "core/themecore.h"

#include <QFontMetrics>
#include <QHBoxLayout>
#include <QResizeEvent>

namespace {
constexpr int kLedSize = 10;  // visual.md 6.6
}

DriveStatusWidget::DriveStatusWidget(const QString &driveName, QWidget *parent) : QWidget(parent)
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(4);

    driveLabel_ = new QLabel(driveName);
    driveLabel_->setStyleSheet("font-weight: bold;");
    layout->addWidget(driveLabel_);

    imageLabel_ = new QLabel();
    imageLabel_->setTextFormat(Qt::PlainText);
    // Let the label shrink below its text so long names elide instead of widening the panel
    imageLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
    layout->addWidget(imageLabel_, 1);

    indicator_ = new QLabel();
    indicator_->setFixedSize(kLedSize, kLedSize);
    layout->addWidget(indicator_);

    ejectButton_ = new QToolButton();
    ejectButton_->setText(tr("⏏"));  // Eject symbol
    ejectButton_->setToolTip(tr("Eject"));
    ejectButton_->setAutoRaise(true);
    connect(ejectButton_, &QToolButton::clicked, this, &DriveStatusWidget::ejectClicked);
    layout->addWidget(ejectButton_);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) {
                updateDisplay();
                updateImageLabel();
            });

    updateImageLabel();
    updateDisplay();
}

void DriveStatusWidget::setImageName(const QString &imageName)
{
    imageName_ = imageName;
    updateImageLabel();
}

void DriveStatusWidget::setMounted(bool mounted)
{
    mounted_ = mounted;
    updateDisplay();
}

bool DriveStatusWidget::isMounted() const
{
    return mounted_;
}

void DriveStatusWidget::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateImageLabel();
}

void DriveStatusWidget::updateImageLabel()
{
    if (imageName_.isEmpty()) {
        imageLabel_->setText(tr("[empty]"));
        imageLabel_->setToolTip(QString());
        imageLabel_->setStyleSheet(
            QStringLiteral("color: %1;").arg(themecore::currentTokens().textMuted.name()));
        return;
    }

    const QFontMetrics metrics(imageLabel_->font());
    imageLabel_->setText(metrics.elidedText(imageName_, Qt::ElideMiddle, imageLabel_->width()));
    imageLabel_->setToolTip(imageName_);
    imageLabel_->setStyleSheet(QString());
}

void DriveStatusWidget::updateDisplay()
{
    const themecore::Tokens tokens = themecore::currentTokens();
    const QColor led = mounted_ ? tokens.stateConnected : tokens.borderStrong;
    indicator_->setStyleSheet(QStringLiteral("background-color: %1; border-radius: %2px;")
                                  .arg(led.name())
                                  .arg(kLedSize / 2));
    ejectButton_->setEnabled(mounted_);
}
