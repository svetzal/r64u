#ifndef C64SCREENWIDGET_H
#define C64SCREENWIDGET_H

#include <QStringList>
#include <QWidget>

class QPainter;

/**
 * @brief Painting shared by every view that shows a C64 display.
 */
namespace c64screen {

/// Default C64 text screen: 40 columns by 25 rows of 8x8 cells.
constexpr int DefaultColumns = 40;
constexpr int DefaultRows = 25;

/**
 * @brief Paints a C64 display (border, screen and text) into @a target.
 *
 * The area of @a target outside the border takes the theme's inset colour.
 * In light mode the screen is VIC-II blue with white text inside a light
 * blue border; in dark mode the screen is black with light blue text and
 * border. Lines are drawn from @a scrollOffset, one per row, clipped to the
 * screen. When there are more lines than rows a one-cell scroll indicator
 * is drawn in the right edge of the border.
 */
void paint(QPainter &painter, const QRect &target, const QStringList &lines, Qt::ColorScheme scheme,
           int scrollOffset, int columns = DefaultColumns, int rows = DefaultRows);

}  // namespace c64screen

/**
 * @brief A widget that looks like a C64 text screen.
 *
 * Text is rendered in C64 Pro Mono at an integer pixel scale, on the classic
 * VIC-II colours, inside a border. Lines beyond the visible rows scroll with
 * the mouse wheel and the Up/Down/PageUp/PageDown keys.
 */
class C64ScreenWidget : public QWidget
{
    Q_OBJECT

public:
    explicit C64ScreenWidget(QWidget *parent = nullptr);

    /**
     * @brief Upper-cases ASCII letters only.
     *
     * C64 Pro Mono's default glyph set is the uppercase/graphics set, so
     * lowercase ASCII looks out of place; everything else passes through.
     */
    [[nodiscard]] static QString petsciiUpper(const QString &text);

    /// Replaces the content with @a text folded to the current column count.
    void setText(const QString &text);

    /// Replaces the content with @a lines exactly as given (no wrapping).
    void setLines(const QStringList &lines);

    [[nodiscard]] const QStringList &lines() const { return lines_; }

    void setColumns(int columns);
    [[nodiscard]] int columns() const { return columns_; }

    void setRows(int rows);
    [[nodiscard]] int rows() const { return rows_; }

    /// The integer pixel scale at the widget's current size.
    [[nodiscard]] int scale() const;

    /// Index of the first visible line.
    [[nodiscard]] int scrollOffset() const { return scrollOffset_; }

    /// Scrolls so that @a offset is the first visible line, clamped to the content.
    void setScrollOffset(int offset);

    /// The largest offset that still shows a full page of lines (0 when everything fits).
    [[nodiscard]] int maxScrollOffset() const;

    [[nodiscard]] QSize sizeHint() const override;
    [[nodiscard]] QSize minimumSizeHint() const override;

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;

private:
    void rewrapIfText();

    QStringList lines_;
    QString text_;  ///< Source of lines_ when set through setText(); null otherwise
    int columns_ = c64screen::DefaultColumns;
    int rows_ = c64screen::DefaultRows;
    int scrollOffset_ = 0;
};

#endif  // C64SCREENWIDGET_H
