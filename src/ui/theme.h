#ifndef THEME_H
#define THEME_H

#include "core/themecore.h"

#include <QPalette>
#include <QString>

class QApplication;

/**
 * @brief Applies the r64u visual design (visual.md) to the running application.
 *
 * The palette and stylesheet are derived from `themecore::Tokens`, so light and
 * dark mode share one definition. `apply()` selects the Fusion style so the
 * stylesheet renders identically on every platform; the macOS menu bar stays
 * native because that is a QMenuBar property, not a style property.
 */
namespace theme {

/**
 * @brief Builds a QPalette whose roles map onto the design tokens.
 */
[[nodiscard]] QPalette makePalette(const themecore::Tokens &tokens);

/**
 * @brief Builds the application stylesheet for the given tokens.
 */
[[nodiscard]] QString styleSheet(const themecore::Tokens &tokens);

/**
 * @brief Sets the Fusion style, the palette and the stylesheet for the current colour scheme.
 */
void apply(QApplication &app);

/**
 * @brief Re-applies the theme whenever the platform colour scheme changes.
 */
void installColorSchemeWatcher(QApplication &app);

}  // namespace theme

#endif  // THEME_H
