#include "pathnavigationwidget.h"

#include "core/themecore.h"

#include <QHBoxLayout>

#include <utility>

PathNavigationWidget::PathNavigationWidget(QString prefix, QWidget *parent)
    : QWidget(parent), prefix_(std::move(prefix))
{
    auto *layout = new QHBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(8);

    upButton_ = new QPushButton(tr("↑ Up"));
    upButton_->setToolTip(tr("Go to parent folder"));
    connect(upButton_, &QPushButton::clicked, this, &PathNavigationWidget::upClicked);
    layout->addWidget(upButton_);

    pathLabel_ = new QLabel();
    pathLabel_->setWordWrap(true);
    layout->addWidget(pathLabel_, 1);

    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            [this](Qt::ColorScheme) { applyAccent(); });

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
    accent_ = Accent::Blue;
    applyAccent();
}

void PathNavigationWidget::setStyleGreen()
{
    accent_ = Accent::Green;
    applyAccent();
}

void PathNavigationWidget::applyAccent()
{
    const themecore::Tokens tokens = themecore::currentTokens();
    const QColor text = accent_ == Accent::Blue ? tokens.accentBlue : tokens.accentGreen;
    pathLabel_->setStyleSheet(
        QStringLiteral("color: %1; background-color: %2; border-radius: 4px; padding: 2px 6px;")
            .arg(text.name(), tokens.bgInset.name()));
}
