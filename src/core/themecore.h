#ifndef THEMECORE_H
#define THEMECORE_H

#include <QColor>
#include <QGuiApplication>
#include <QStyleHints>

/**
 * @brief Colour tokens for the r64u visual design (see visual.md section 3).
 *
 * Pure data and pure functions: no widget state, no I/O. Widgets that need a
 * semantic colour call `tokensFor(effectiveScheme())` and read the field they need.
 */
namespace themecore {

/**
 * @brief The complete set of semantic colours for one colour scheme.
 */
struct Tokens
{
    // Backgrounds
    QColor bgApp;    ///< Window / breadbin plastic
    QColor bgPanel;  ///< Inner panels
    QColor bgInset;  ///< Recessed areas (headers, pressed buttons, path badges)

    // Borders
    QColor borderSubtle;
    QColor borderStrong;

    // Text
    QColor textPrimary;
    QColor textSecondary;
    QColor textMuted;
    QColor textInverted;

    // C64 rainbow accents
    QColor accentRed;
    QColor accentOrange;
    QColor accentYellow;
    QColor accentGreen;
    QColor accentBlue;

    // State colours
    QColor stateConnected;
    QColor stateWarning;
    QColor stateError;
    QColor stateInfo;

    // LED indicator
    QColor ledOn;
    QColor ledOff;

    // VIC-II colours used by the C64-look text views
    QColor vicBlue;       ///< Classic C64 screen blue
    QColor vicLightBlue;  ///< Classic C64 text light blue
};

/**
 * @brief Returns the light "beige plastic" palette (visual.md 3.1).
 */
[[nodiscard]] inline Tokens lightTokens()
{
    Tokens t;
    t.bgApp = QColor("#E8DFC8");
    t.bgPanel = QColor("#F4EEDD");
    t.bgInset = QColor("#E1D6BB");
    t.borderSubtle = QColor("#CFC5AA");
    t.borderStrong = QColor("#B7AE95");
    t.textPrimary = QColor("#1E1E1E");
    t.textSecondary = QColor("#4A4A4A");
    t.textMuted = QColor("#6F6F6F");
    t.textInverted = QColor("#FFFFFF");
    t.accentRed = QColor("#D6403A");
    t.accentOrange = QColor("#E8842A");
    t.accentYellow = QColor("#E6C229");
    t.accentGreen = QColor("#3FA45B");
    t.accentBlue = QColor("#3A7BD5");
    t.stateConnected = QColor("#3FA45B");
    t.stateWarning = QColor("#E6C229");
    t.stateError = QColor("#C7372F");
    t.stateInfo = QColor("#3A7BD5");
    t.ledOn = QColor("#FF3B30");
    t.ledOff = QColor("#7A1A17");
    t.vicBlue = QColor("#4040E8");
    t.vicLightBlue = QColor("#887ECB");
    return t;
}

/**
 * @brief Returns the dark "desk mat" palette (visual.md 3.2).
 *
 * State and LED colours are not specified separately for dark mode in the
 * design spec, so they follow the light palette (the rainbow is never inverted).
 */
[[nodiscard]] inline Tokens darkTokens()
{
    Tokens t = lightTokens();
    t.bgApp = QColor("#1E1F22");
    t.bgPanel = QColor("#2A2C31");
    t.bgInset = QColor("#18191C");
    t.borderSubtle = QColor("#3A3C42");
    t.borderStrong = QColor("#4A4D55");
    t.textPrimary = QColor("#F5F5F5");
    t.textSecondary = QColor("#C9C9C9");
    t.textMuted = QColor("#9A9A9A");
    t.textInverted = QColor("#000000");
    t.accentRed = QColor("#E04B44");
    t.accentOrange = QColor("#F19A3E");
    t.accentYellow = QColor("#F1D04B");
    t.accentGreen = QColor("#4EC07A");
    t.accentBlue = QColor("#4A8DFF");
    t.stateConnected = t.accentGreen;
    t.stateWarning = t.accentYellow;
    t.stateInfo = t.accentBlue;
    return t;
}

/**
 * @brief Returns the token set for a colour scheme. Unknown is treated as Light.
 */
[[nodiscard]] inline Tokens tokensFor(Qt::ColorScheme scheme)
{
    return scheme == Qt::ColorScheme::Dark ? darkTokens() : lightTokens();
}

/**
 * @brief The colour scheme the platform currently reports, with Unknown mapped to Light.
 */
[[nodiscard]] inline Qt::ColorScheme effectiveScheme()
{
    const Qt::ColorScheme scheme = QGuiApplication::styleHints()->colorScheme();
    return scheme == Qt::ColorScheme::Dark ? Qt::ColorScheme::Dark : Qt::ColorScheme::Light;
}

/**
 * @brief Convenience: the tokens for the platform's current colour scheme.
 */
[[nodiscard]] inline Tokens currentTokens()
{
    return tokensFor(effectiveScheme());
}

}  // namespace themecore

#endif  // THEMECORE_H
