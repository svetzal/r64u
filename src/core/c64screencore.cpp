#include "c64screencore.h"

#include <algorithm>

namespace c64screencore {

namespace {

/// Folds one newline-free line into @a out, at most @a columns characters per entry.
void wrapLine(QString rest, int columns, QStringList &out)
{
    while (rest.size() > columns) {
        int cut = columns;
        if (rest.at(columns) != QLatin1Char(' ')) {
            cut = rest.left(columns).lastIndexOf(QLatin1Char(' '));
        }
        if (cut <= 0) {
            out.append(rest.left(columns));
            rest = rest.mid(columns);
        } else {
            out.append(rest.left(cut));
            rest = rest.mid(cut + 1);
        }
    }
    out.append(rest);
}

}  // namespace

QStringList wrapToColumns(const QString &text, int columns)
{
    QStringList out;
    QString normalized = text;
    normalized.remove(QLatin1Char('\r'));
    const QStringList lines = normalized.split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        if (columns <= 0) {
            out.append(line);
        } else {
            wrapLine(line, columns, out);
        }
    }
    return out;
}

ScreenGeometry layoutScreen(const QSize &widget, int columns, int rows, int cellPx, int borderCells)
{
    ScreenGeometry geometry;

    const int unitWidth = (columns + 2 * borderCells) * cellPx;
    const int unitHeight = (rows + 2 * borderCells) * cellPx;
    if (unitWidth <= 0 || unitHeight <= 0) {
        return geometry;
    }

    geometry.scale =
        std::max(1, std::min(widget.width() / unitWidth, widget.height() / unitHeight));

    const int borderWidth = unitWidth * geometry.scale;
    const int borderHeight = unitHeight * geometry.scale;
    geometry.border = QRect((widget.width() - borderWidth) / 2,
                            (widget.height() - borderHeight) / 2, borderWidth, borderHeight);

    const int inset = borderCells * cellPx * geometry.scale;
    geometry.screen = QRect(geometry.border.x() + inset, geometry.border.y() + inset,
                            columns * cellPx * geometry.scale, rows * cellPx * geometry.scale);
    return geometry;
}

}  // namespace c64screencore
