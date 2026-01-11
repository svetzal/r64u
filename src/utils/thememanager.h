#ifndef THEMEMANAGER_H
#define THEMEMANAGER_H

#include <QObject>
#include <QString>
#include <QStringList>

/**
 * @brief Manages application themes (light/dark mode)
 * 
 * The ThemeManager handles loading and switching between visual themes
 * based on the visual.md specification. It supports:
 * - Light theme (Breadbin beige plastic)
 * - Dark theme (Charcoal desk mat)
 * - System theme (follows OS preference)
 */
class ThemeManager : public QObject
{
    Q_OBJECT

public:
    enum class Theme {
        System,   // Follow system/OS preference
        Light,    // Breadbin beige plastic
        Dark      // Charcoal desk mat
    };
    Q_ENUM(Theme)

    /**
     * @brief Get the singleton instance
     */
    static ThemeManager* instance();

    /**
     * @brief Get the current theme setting
     */
    Theme currentTheme() const;

    /**
     * @brief Get the actually applied theme (resolves System to Light/Dark)
     */
    Theme effectiveTheme() const;

    /**
     * @brief Get the list of available theme names
     */
    static QStringList availableThemes();

    /**
     * @brief Convert theme enum to display name
     */
    static QString themeName(Theme theme);

    /**
     * @brief Convert display name to theme enum
     */
    static Theme themeFromName(const QString &name);

public slots:
    /**
     * @brief Set and apply a new theme
     * @param theme The theme to apply
     */
    void setTheme(Theme theme);

    /**
     * @brief Apply the current theme (useful after app startup)
     */
    void applyTheme();

    /**
     * @brief Refresh theme based on system preference (if using System theme)
     */
    void refreshSystemTheme();

signals:
    /**
     * @brief Emitted when the theme changes
     */
    void themeChanged(Theme theme);

private:
    explicit ThemeManager(QObject *parent = nullptr);
    ~ThemeManager() override = default;

    // Prevent copying
    ThemeManager(const ThemeManager&) = delete;
    ThemeManager& operator=(const ThemeManager&) = delete;

    /**
     * @brief Load and apply a QSS file
     * @param themePath Resource path to the QSS file
     */
    void loadStyleSheet(const QString &themePath);

    /**
     * @brief Detect if system is in dark mode
     */
    bool isSystemDarkMode() const;

    Theme currentTheme_ = Theme::System;

    static ThemeManager* instance_;
};

#endif // THEMEMANAGER_H
