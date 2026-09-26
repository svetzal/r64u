#ifndef C64SCREENCORE_H
#define C64SCREENCORE_H

#include <QRect>
#include <QSize>
#include <QString>
#include <QStringList>

/**
 * @brief Pure layout and text helpers for the C64 screen views.
 *
 * No widgets, no painting: these functions decide where a character-cell
 * screen sits inside a widget and how text is folded into its columns.
 */
namespace c64screencore {

/**
 * @brief Splits @a text on newlines and folds each line to at most @a columns characters.
 *
 * A line longer than @a columns breaks at the last space inside its window
 * (the space itself is dropped); when the window holds no space the line is
 * cut hard at @a columns. Empty lines are kept. A @a columns of zero or less
 * only splits on newlines.
 */
[[nodiscard]] QStringList wrapToColumns(const QString &text, int columns);

/**
 * @brief Where a scaled character screen and its border sit inside a widget.
 */
struct ScreenGeometry
{
    int scale = 1;  ///< Integer pixel multiplier applied to every cell
    QRect border;   ///< The full display including the border, centred in the widget
    QRect screen;   ///< The character area inside the border
};

/**
 * @brief Lays out a @a columns x @a rows character screen in a widget of size @a widget.
 *
 * The screen is columns*cellPx by rows*cellPx; the border adds borderCells*cellPx
 * on each side. The scale is the largest integer at which the bordered display
 * fits the widget (never below 1), and both rectangles are centred.
 */
[[nodiscard]] ScreenGeometry layoutScreen(const QSize &widget, int columns, int rows,
                                          int cellPx = 8, int borderCells = 4);

}  // namespace c64screencore

#endif  // C64SCREENCORE_H
