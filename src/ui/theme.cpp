#include "theme.h"

#include <QApplication>
#include <QStyleFactory>
#include <QStyleHints>

namespace {

/// Blends two colours; `amount` is the weight of `b` (0 = all `a`, 1 = all `b`).
QColor blend(const QColor &a, const QColor &b, double amount)
{
    const double keep = 1.0 - amount;
    return QColor::fromRgbF(a.redF() * keep + b.redF() * amount,
                            a.greenF() * keep + b.greenF() * amount,
                            a.blueF() * keep + b.blueF() * amount);
}

/// `colour` at 50% alpha as a QSS rgba() literal.
QString halfAlpha(const QColor &colour)
{
    return QStringLiteral("rgba(%1, %2, %3, 128)")
        .arg(colour.red())
        .arg(colour.green())
        .arg(colour.blue());
}

/// Hard-edged five-stripe C64 rainbow as a horizontal QSS gradient.
QString rainbowGradient(const themecore::Tokens &t)
{
    return QStringLiteral("qlineargradient(x1:0, y1:0, x2:1, y2:0,"
                          " stop:0 %1, stop:0.199 %1, stop:0.2 %2, stop:0.399 %2,"
                          " stop:0.4 %3, stop:0.599 %3, stop:0.6 %4, stop:0.799 %4,"
                          " stop:0.8 %5, stop:1 %5)")
        .arg(t.accentRed.name(), t.accentOrange.name(), t.accentYellow.name(), t.accentGreen.name(),
             t.accentBlue.name());
}

void applyColors(QApplication &app)
{
    const themecore::Tokens tokens = themecore::currentTokens();
    app.setPalette(theme::makePalette(tokens));
    app.setStyleSheet(theme::styleSheet(tokens));
}

}  // namespace

namespace theme {

QPalette makePalette(const themecore::Tokens &t)
{
    QPalette p;

    p.setColor(QPalette::Window, t.bgApp);
    p.setColor(QPalette::WindowText, t.textPrimary);
    p.setColor(QPalette::Base, t.bgPanel);
    p.setColor(QPalette::AlternateBase, blend(t.bgPanel, t.bgInset, 0.5));
    p.setColor(QPalette::Text, t.textPrimary);
    p.setColor(QPalette::PlaceholderText, t.textMuted);
    p.setColor(QPalette::Button, t.bgPanel);
    p.setColor(QPalette::ButtonText, t.textPrimary);
    p.setColor(QPalette::BrightText, t.textInverted);
    p.setColor(QPalette::ToolTipBase, t.bgPanel);
    p.setColor(QPalette::ToolTipText, t.textPrimary);
    p.setColor(QPalette::Highlight, t.accentBlue);
    p.setColor(QPalette::HighlightedText, t.textInverted);
    p.setColor(QPalette::Link, t.accentBlue);
    p.setColor(QPalette::LinkVisited, t.accentBlue.darker(120));

    // Frame shading used by Fusion for bevels and separators
    p.setColor(QPalette::Light, t.bgPanel.lighter(105));
    p.setColor(QPalette::Midlight, t.bgInset);
    p.setColor(QPalette::Mid, t.borderSubtle);
    p.setColor(QPalette::Dark, t.borderStrong);
    p.setColor(QPalette::Shadow, t.borderStrong.darker(130));

    // Disabled widgets read as muted text, not ghosted chrome
    p.setColor(QPalette::Disabled, QPalette::WindowText, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::Text, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::ButtonText, t.textMuted);
    p.setColor(QPalette::Disabled, QPalette::Highlight, t.borderSubtle);
    p.setColor(QPalette::Disabled, QPalette::HighlightedText, t.textMuted);

    return p;
}

QString styleSheet(const themecore::Tokens &t)
{
    // The stylesheet only adds shape (radii, insets, borders); colours that
    // widgets get from the palette are left to the palette.
    QString css = QStringLiteral(R"(
QMainWindow, QDialog {
    background-color: %bgApp%;
}

/* Panels: off-white cards sitting on the beige plastic */
QTabWidget::pane {
    background-color: %bgPanel%;
    border: 1px solid %borderSubtle%;
    border-radius: 6px;
}
QGroupBox {
    background-color: %bgPanel%;
    border: 1px solid %borderSubtle%;
    border-radius: 6px;
    margin-top: 1.2em;
    padding-top: 4px;
}
QGroupBox::title {
    subcontrol-origin: margin;
    left: 8px;
    padding: 0 4px;
    color: %textSecondary%;
}
QTreeView, QListView, QTableView {
    background-color: %bgPanel%;
    alternate-background-color: %alternateBase%;
    border: 1px solid %borderSubtle%;
    border-radius: 6px;
    selection-background-color: %accentBlue%;
    selection-color: %textInverted%;
}

/* Tab bar: rounded keycaps, the selected one carries the rainbow stripe */
QTabBar::tab {
    background-color: %bgInset%;
    color: %textSecondary%;
    border: 1px solid %borderSubtle%;
    border-bottom: 1px solid %borderSubtle%;
    border-top-left-radius: 6px;
    border-top-right-radius: 6px;
    padding: 6px 14px;
    margin-top: 3px;
    margin-right: 2px;
}
QTabBar::tab:hover:!selected {
    background-color: %alternateBase%;
}
QTabBar::tab:selected {
    background-color: %bgPanel%;
    color: %textPrimary%;
    margin-top: 0;
    border-bottom: 4px solid %rainbow%;
}

/* Toolbars are transparent strips with a ridge below */
QToolBar {
    background: transparent;
    border: none;
    border-bottom: 1px solid %borderSubtle%;
    padding: 2px;
    spacing: 4px;
}
QToolBar::separator {
    background-color: %borderSubtle%;
    width: 1px;
    margin: 4px 4px;
}

/* Buttons: flat until touched, pressed in when clicked */
QToolButton {
    background: transparent;
    border: 1px solid transparent;
    border-radius: 4px;
    padding: 4px 8px;
}
QToolButton:hover {
    background-color: %bgInset%;
    border-color: %borderSubtle%;
}
QToolButton:pressed, QToolButton:checked {
    background-color: %bgInset%;
    border: 1px solid %borderStrong%;
    padding-top: 5px;
    padding-bottom: 3px;
}
QToolButton:disabled {
    color: %textMuted%;
}

/* Guarded actions (Reboot, Power Off): a faint red ring that hardens on approach */
QToolButton#DangerAction {
    border: 1px solid %accentRedFaint%;
}
QToolButton#DangerAction:hover {
    border-color: %accentRed%;
}
QToolButton#DangerAction:pressed {
    background-color: %bgInset%;
    border: 1px solid %accentRed%;
    padding-top: 5px;
    padding-bottom: 3px;
}
QToolButton#DangerAction:disabled {
    border-color: %borderSubtle%;
}

/* min-height is the content box: 18 + 2*4 padding + 2*1 border = 28px */
QPushButton {
    background-color: %bgPanel%;
    color: %textPrimary%;
    border: 1px solid %borderStrong%;
    border-radius: 4px;
    padding: 4px 12px;
    min-height: 18px;
}
QPushButton:hover {
    background-color: %bgInset%;
}
QPushButton:pressed, QPushButton:checked {
    background-color: %bgInset%;
    border-color: %borderStrong%;
    padding-top: 5px;
    padding-bottom: 3px;
}
QPushButton:default {
    border-color: %accentBlue%;
}
QPushButton:disabled {
    color: %textMuted%;
    border-color: %borderSubtle%;
}

/* Text entry */
QLineEdit, QSpinBox, QDoubleSpinBox, QComboBox {
    background-color: %bgPanel%;
    border: 1px solid %borderStrong%;
    border-radius: 4px;
    padding: 3px 6px;
    selection-background-color: %accentBlue%;
    selection-color: %textInverted%;
}
QLineEdit:disabled, QSpinBox:disabled, QDoubleSpinBox:disabled, QComboBox:disabled {
    color: %textMuted%;
    border-color: %borderSubtle%;
}

/* Table headers: flat recessed strip */
QHeaderView {
    background-color: %bgInset%;
}
QHeaderView::section {
    background-color: %bgInset%;
    color: %textSecondary%;
    border: none;
    border-right: 1px solid %borderSubtle%;
    border-bottom: 1px solid %borderSubtle%;
    padding: 4px 6px;
}
QHeaderView::section:last, QHeaderView::section:only-one {
    border-right: none;
}

QSplitter::handle {
    background: transparent;
}

QStatusBar {
    background-color: %bgInset%;
    border-top: 1px solid %borderSubtle%;
}
QStatusBar::item {
    border: none;
}

QProgressBar {
    background-color: %bgInset%;
    border: 1px solid %borderSubtle%;
    border-radius: 6px;
    text-align: center;
    color: %textPrimary%;
}
QProgressBar::chunk {
    background-color: %accentBlue%;
    border-radius: 5px;
}

/* Slim rounded scrollbars with no arrow buttons */
QScrollBar:vertical {
    background: transparent;
    width: 10px;
    margin: 0;
}
QScrollBar:horizontal {
    background: transparent;
    height: 10px;
    margin: 0;
}
QScrollBar::handle:vertical {
    background-color: %borderStrong%;
    border-radius: 5px;
    min-height: 24px;
}
QScrollBar::handle:horizontal {
    background-color: %borderStrong%;
    border-radius: 5px;
    min-width: 24px;
}
QScrollBar::handle:hover {
    background-color: %textMuted%;
}
QScrollBar::add-line, QScrollBar::sub-line {
    width: 0;
    height: 0;
    border: none;
    background: none;
}
QScrollBar::add-page, QScrollBar::sub-page {
    background: transparent;
}

QToolTip {
    background-color: %bgPanel%;
    color: %textPrimary%;
    border: 1px solid %borderSubtle%;
    padding: 4px;
}
)");

    const QList<QPair<QString, QString>> substitutions = {
        {"%bgApp%", t.bgApp.name()},
        {"%bgPanel%", t.bgPanel.name()},
        {"%bgInset%", t.bgInset.name()},
        {"%alternateBase%", blend(t.bgPanel, t.bgInset, 0.5).name()},
        {"%borderSubtle%", t.borderSubtle.name()},
        {"%borderStrong%", t.borderStrong.name()},
        {"%textPrimary%", t.textPrimary.name()},
        {"%textSecondary%", t.textSecondary.name()},
        {"%textMuted%", t.textMuted.name()},
        {"%textInverted%", t.textInverted.name()},
        {"%accentBlue%", t.accentBlue.name()},
        {"%accentRed%", t.accentRed.name()},
        {"%accentRedFaint%", halfAlpha(t.accentRed)},
        {"%rainbow%", rainbowGradient(t)},
    };
    for (const auto &[placeholder, value] : substitutions) {
        css.replace(placeholder, value);
    }
    return css;
}

void apply(QApplication &app)
{
    // Fusion renders the stylesheet identically on every platform. The macOS
    // menu bar stays native: that is a QMenuBar attribute, not a style choice.
    app.setStyle(QStyleFactory::create(QStringLiteral("Fusion")));
    applyColors(app);
}

void installColorSchemeWatcher(QApplication &app)
{
    QObject::connect(QGuiApplication::styleHints(), &QStyleHints::colorSchemeChanged, &app,
                     [&app](Qt::ColorScheme) { applyColors(app); });
}

}  // namespace theme
