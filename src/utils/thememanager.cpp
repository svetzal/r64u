#include "thememanager.h"

#include <QApplication>
#include <QFile>
#include <QStyle>
#include <QStyleHints>
#include <QSettings>
#include <QDebug>

ThemeManager* ThemeManager::instance_ = nullptr;

ThemeManager* ThemeManager::instance()
{
    if (!instance_) {
        instance_ = new ThemeManager(qApp);
    }
    return instance_;
}

ThemeManager::ThemeManager(QObject *parent)
    : QObject(parent)
{
    // Load saved theme preference
    QSettings settings;
    int savedTheme = settings.value("appearance/theme", static_cast<int>(Theme::System)).toInt();
    currentTheme_ = static_cast<Theme>(savedTheme);

    // Connect to system theme changes
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    connect(qApp->styleHints(), &QStyleHints::colorSchemeChanged,
            this, &ThemeManager::refreshSystemTheme);
#endif
}

ThemeManager::Theme ThemeManager::currentTheme() const
{
    return currentTheme_;
}

ThemeManager::Theme ThemeManager::effectiveTheme() const
{
    if (currentTheme_ == Theme::System) {
        return isSystemDarkMode() ? Theme::Dark : Theme::Light;
    }
    return currentTheme_;
}

QStringList ThemeManager::availableThemes()
{
    return QStringList() << "System" << "Light" << "Dark";
}

QString ThemeManager::themeName(Theme theme)
{
    switch (theme) {
    case Theme::System:
        return QStringLiteral("System");
    case Theme::Light:
        return QStringLiteral("Light");
    case Theme::Dark:
        return QStringLiteral("Dark");
    }
    return QStringLiteral("System");
}

ThemeManager::Theme ThemeManager::themeFromName(const QString &name)
{
    if (name == QStringLiteral("Light")) {
        return Theme::Light;
    } else if (name == QStringLiteral("Dark")) {
        return Theme::Dark;
    }
    return Theme::System;
}

void ThemeManager::setTheme(Theme theme)
{
    if (currentTheme_ != theme) {
        currentTheme_ = theme;

        // Save preference
        QSettings settings;
        settings.setValue("appearance/theme", static_cast<int>(theme));

        applyTheme();
        emit themeChanged(theme);
    }
}

void ThemeManager::applyTheme()
{
    Theme effective = effectiveTheme();
    QString themePath;

    switch (effective) {
    case Theme::Light:
        themePath = QStringLiteral(":/themes/theme-light.qss");
        break;
    case Theme::Dark:
        themePath = QStringLiteral(":/themes/theme-dark.qss");
        break;
    default:
        themePath = QStringLiteral(":/themes/theme-light.qss");
        break;
    }

    loadStyleSheet(themePath);
}

void ThemeManager::refreshSystemTheme()
{
    if (currentTheme_ == Theme::System) {
        applyTheme();
        emit themeChanged(currentTheme_);
    }
}

void ThemeManager::loadStyleSheet(const QString &themePath)
{
    QFile file(themePath);
    if (file.open(QFile::ReadOnly | QFile::Text)) {
        QString styleSheet = QString::fromUtf8(file.readAll());
        qApp->setStyleSheet(styleSheet);
        file.close();
    } else {
        qWarning() << "Failed to load theme:" << themePath;
    }
}

bool ThemeManager::isSystemDarkMode() const
{
#if QT_VERSION >= QT_VERSION_CHECK(6, 5, 0)
    return qApp->styleHints()->colorScheme() == Qt::ColorScheme::Dark;
#else
    // Fallback for older Qt versions: check palette
    QPalette palette = qApp->palette();
    QColor windowColor = palette.color(QPalette::Window);
    // If the window color is dark, we're probably in dark mode
    return windowColor.lightness() < 128;
#endif
}
