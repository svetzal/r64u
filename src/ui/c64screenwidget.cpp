#include "c64screenwidget.h"

#include "core/c64screencore.h"
#include "core/themecore.h"

#include <QFont>
#include <QFontMetrics>
#include <QGuiApplication>
#include <QKeyEvent>
#include <QPainter>
#include <QStyleHints>
#include <QWheelEvent>

#include <algorithm>

namespace {

constexpr int CellPx = 8;
constexpr int BorderCells = 4;
constexpr int WheelStepLines = 3;

/// The three colours of a C64 display for one colour scheme.
struct ScreenColours
{
    QColor border;
    QColor screen;
    QColor text;
};

ScreenColours coloursFor(Qt::ColorScheme scheme, const themecore::Tokens &tokens)
{
    if (scheme == Qt::ColorScheme::Dark) {
        return {tokens.vicLightBlue, QColor(Qt::black), tokens.vicLightBlue};
    }
    return {tokens.vicLightBlue, tokens.vicBlue, tokens.textInverted};
}

QFont screenFont(int scale)
{
    QFont font(QStringLiteral("C64 Pro Mono"));
    font.setStyleHint(QFont::Monospace);
    font.setPixelSize(CellPx * scale);
    font.setStyleStrategy(QFont::NoSubpixelAntialias);
    font.setHintingPreference(QFont::PreferNoHinting);
    return font;
}

void paintScrollIndicator(QPainter &painter, const c64screencore::ScreenGeometry &geometry,
                          int lineCount, int rows, int scrollOffset, const ScreenColours &colours)
{
    const int cell = CellPx * geometry.scale;
    const QRect track(geometry.screen.right() + 1 + cell, geometry.screen.top(), cell,
                      geometry.screen.height());
    painter.fillRect(track, colours.border.darker(125));

    const int thumbHeight = std::max(cell, track.height() * rows / lineCount);
    const int travel = track.height() - thumbHeight;
    const int maxOffset = lineCount - rows;
    const int thumbTop = track.top() + (maxOffset > 0 ? travel * scrollOffset / maxOffset : 0);
    painter.fillRect(QRect(track.left(), thumbTop, cell, thumbHeight), colours.text);
}

}  // namespace

namespace c64screen {

void paint(QPainter &painter, const QRect &target, const QStringList &lines, Qt::ColorScheme scheme,
           int scrollOffset, int columns, int rows)
{
    const themecore::Tokens tokens = themecore::tokensFor(scheme);
    const ScreenColours colours = coloursFor(scheme, tokens);

    c64screencore::ScreenGeometry geometry =
        c64screencore::layoutScreen(target.size(), columns, rows, CellPx, BorderCells);
    geometry.border.translate(target.topLeft());
    geometry.screen.translate(target.topLeft());

    painter.save();
    painter.fillRect(target, tokens.bgInset);
    painter.fillRect(geometry.border, colours.border);
    painter.fillRect(geometry.screen, colours.screen);

    if (lines.size() > rows) {
        paintScrollIndicator(painter, geometry, static_cast<int>(lines.size()), rows,
                             std::clamp(scrollOffset, 0, static_cast<int>(lines.size()) - rows),
                             colours);
    }

    painter.setClipRect(geometry.screen);
    painter.setPen(colours.text);
    painter.setFont(screenFont(geometry.scale));
    const int ascent = QFontMetrics(painter.font()).ascent();
    const int rowPitch = CellPx * geometry.scale;

    const int first = std::max(0, scrollOffset);
    const int last = std::min(static_cast<int>(lines.size()), first + rows);
    for (int index = first; index < last; ++index) {
        const int rowTop = geometry.screen.top() + (index - first) * rowPitch;
        painter.drawText(geometry.screen.left(), rowTop + ascent, lines.at(index));
    }
    painter.restore();
}

}  // namespace c64screen

C64ScreenWidget::C64ScreenWidget(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::StrongFocus);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, this,
            qOverload<>(&QWidget::update));
}

QString C64ScreenWidget::petsciiUpper(const QString &text)
{
    QString upper = text;
    for (QChar &ch : upper) {
        if (ch >= QLatin1Char('a') && ch <= QLatin1Char('z')) {
            ch = ch.toUpper();
        }
    }
    return upper;
}

void C64ScreenWidget::setText(const QString &text)
{
    text_ = text;
    lines_ = c64screencore::wrapToColumns(text, columns_);
    scrollOffset_ = 0;
    update();
}

void C64ScreenWidget::setLines(const QStringList &lines)
{
    text_ = QString();
    lines_ = lines;
    scrollOffset_ = 0;
    update();
}

void C64ScreenWidget::setColumns(int columns)
{
    if (columns_ == columns || columns < 1) {
        return;
    }
    columns_ = columns;
    rewrapIfText();
    updateGeometry();
    update();
}

void C64ScreenWidget::setRows(int rows)
{
    if (rows_ == rows || rows < 1) {
        return;
    }
    rows_ = rows;
    setScrollOffset(scrollOffset_);
    updateGeometry();
    update();
}

void C64ScreenWidget::rewrapIfText()
{
    if (!text_.isNull()) {
        lines_ = c64screencore::wrapToColumns(text_, columns_);
        setScrollOffset(scrollOffset_);
    }
}

int C64ScreenWidget::scale() const
{
    return c64screencore::layoutScreen(size(), columns_, rows_, CellPx, BorderCells).scale;
}

int C64ScreenWidget::maxScrollOffset() const
{
    return std::max(0, static_cast<int>(lines_.size()) - rows_);
}

void C64ScreenWidget::setScrollOffset(int offset)
{
    const int clamped = std::clamp(offset, 0, maxScrollOffset());
    if (clamped != scrollOffset_) {
        scrollOffset_ = clamped;
        update();
    }
}

QSize C64ScreenWidget::sizeHint() const
{
    return {(columns_ + 2 * BorderCells) * CellPx, (rows_ + 2 * BorderCells) * CellPx};
}

QSize C64ScreenWidget::minimumSizeHint() const
{
    return sizeHint();
}

void C64ScreenWidget::paintEvent(QPaintEvent * /*event*/)
{
    QPainter painter(this);
    c64screen::paint(painter, rect(), lines_, themecore::effectiveScheme(), scrollOffset_, columns_,
                     rows_);
}

void C64ScreenWidget::wheelEvent(QWheelEvent *event)
{
    if (maxScrollOffset() == 0) {
        event->ignore();
        return;
    }
    const int steps = event->angleDelta().y() / 120;
    setScrollOffset(scrollOffset_ - steps * WheelStepLines);
    event->accept();
}

void C64ScreenWidget::keyPressEvent(QKeyEvent *event)
{
    switch (event->key()) {
    case Qt::Key_Up:
        setScrollOffset(scrollOffset_ - 1);
        break;
    case Qt::Key_Down:
        setScrollOffset(scrollOffset_ + 1);
        break;
    case Qt::Key_PageUp:
        setScrollOffset(scrollOffset_ - rows_);
        break;
    case Qt::Key_PageDown:
        setScrollOffset(scrollOffset_ + rows_);
        break;
    default:
        QWidget::keyPressEvent(event);
        return;
    }
    event->accept();
}
