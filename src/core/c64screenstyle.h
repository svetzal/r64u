#ifndef C64SCREENSTYLE_H
#define C64SCREENSTYLE_H

#include "core/themecore.h"

#include <QString>

/**
 * @brief Stylesheet fragments shared by the C64-look text views.
 *
 * Header-only so the preview widgets and FileDetailsPanel share one definition
 * of the classic screen colours without another compilation unit.
 */
namespace c64screen {

/// QTextBrowser rule giving the classic C64 screen: white on VIC-II blue in
/// light mode, VIC-II light blue on black in dark mode.
[[nodiscard]] inline QString c64TextBrowserStyle(Qt::ColorScheme scheme)
{
    const themecore::Tokens tokens = themecore::tokensFor(scheme);
    if (scheme == Qt::ColorScheme::Dark) {
        return QStringLiteral("QTextBrowser {"
                              "  background-color: #000000;"
                              "  color: %1;"
                              "  border: 1px solid %1;"
                              "  padding: 8px;"
                              "}")
            .arg(tokens.vicLightBlue.name());
    }
    return QStringLiteral("QTextBrowser {"
                          "  background-color: %1;"
                          "  color: %2;"
                          "  border: 1px solid %3;"
                          "  padding: 8px;"
                          "}")
        .arg(tokens.vicBlue.name(), tokens.textInverted.name(), tokens.vicBlue.darker(140).name());
}

/// Rule for secondary "nothing selected" / status labels.
[[nodiscard]] inline QString mutedTextStyle()
{
    return QStringLiteral("color: %1;").arg(themecore::currentTokens().textMuted.name());
}

}  // namespace c64screen

#endif  // C64SCREENSTYLE_H
