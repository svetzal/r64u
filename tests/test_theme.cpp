/**
 * @file test_theme.cpp
 * @brief Unit tests for the theme palette and stylesheet builders.
 *
 * Tests verify:
 * - makePalette() maps Window to bg app and Highlight to accent blue for both schemes
 * - makePalette() uses text muted for disabled text
 * - styleSheet() is non-empty, styles the tab bar keycaps, and has no unresolved placeholders
 * - styleSheet() carries the rainbow stripe only on the selected tab
 * - apply() installs the palette and stylesheet on top of the Fusion style
 * - The stylesheet parses cleanly when real widgets are polished with it
 */

#include "ui/theme.h"

#include <QApplication>
#include <QProgressBar>
#include <QPushButton>
#include <QRegularExpression>
#include <QStyle>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QTreeWidget>
#include <QVBoxLayout>
#include <QtTest>

class TestTheme : public QObject
{
    Q_OBJECT

private slots:

    void testMakePalette_windowIsBgApp_data()
    {
        QTest::addColumn<int>("scheme");
        QTest::newRow("light") << static_cast<int>(Qt::ColorScheme::Light);
        QTest::newRow("dark") << static_cast<int>(Qt::ColorScheme::Dark);
    }

    void testMakePalette_windowIsBgApp()
    {
        QFETCH(int, scheme);
        const auto tokens = themecore::tokensFor(static_cast<Qt::ColorScheme>(scheme));
        const QPalette palette = theme::makePalette(tokens);

        QCOMPARE(palette.color(QPalette::Window), tokens.bgApp);
        QCOMPARE(palette.color(QPalette::Base), tokens.bgPanel);
        QCOMPARE(palette.color(QPalette::Highlight), tokens.accentBlue);
        QCOMPARE(palette.color(QPalette::WindowText), tokens.textPrimary);
    }

    void testMakePalette_disabledTextIsMuted()
    {
        const auto tokens = themecore::tokensFor(Qt::ColorScheme::Light);
        const QPalette palette = theme::makePalette(tokens);

        QCOMPARE(palette.color(QPalette::Disabled, QPalette::Text), tokens.textMuted);
        QCOMPARE(palette.color(QPalette::Disabled, QPalette::ButtonText), tokens.textMuted);
    }

    void testStyleSheet_isNonEmptyAndStylesTabBar()
    {
        const QString css = theme::styleSheet(themecore::tokensFor(Qt::ColorScheme::Light));

        QVERIFY(!css.isEmpty());
        QVERIFY(css.contains("QTabBar::tab"));
        QVERIFY(css.contains("QTabBar::tab:selected"));
    }

    void testStyleSheet_hasNoUnresolvedPlaceholders()
    {
        const QString css = theme::styleSheet(themecore::tokensFor(Qt::ColorScheme::Dark));
        QVERIFY2(!css.contains('%'), "every %token% placeholder must be substituted");
    }

    void testStyleSheet_rainbowOnlyOnSelectedTab()
    {
        const auto tokens = themecore::tokensFor(Qt::ColorScheme::Light);
        const QString css = theme::styleSheet(tokens);

        QCOMPARE(css.count("qlineargradient"), 1);
        const qsizetype gradientAt = css.indexOf("qlineargradient");
        const qsizetype selectedTabAt = css.indexOf("QTabBar::tab:selected");
        QVERIFY(selectedTabAt >= 0 && gradientAt > selectedTabAt);
        QVERIFY(css.contains(tokens.accentRed.name()));
        QVERIFY(css.contains(tokens.accentGreen.name()));
    }

    void testStyleSheet_usesSchemeColours()
    {
        const auto light = themecore::tokensFor(Qt::ColorScheme::Light);
        const auto dark = themecore::tokensFor(Qt::ColorScheme::Dark);

        QVERIFY(theme::styleSheet(light).contains(light.bgPanel.name()));
        QVERIFY(theme::styleSheet(dark).contains(dark.bgPanel.name()));
        QVERIFY(!theme::styleSheet(dark).contains(light.bgPanel.name()));
    }

    void testApply_selectsFusionAndInstallsPalette()
    {
        auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
        QVERIFY(app != nullptr);

        theme::apply(*app);

        const auto tokens = themecore::currentTokens();
        QCOMPARE(QApplication::palette().color(QPalette::Window), tokens.bgApp);
        QVERIFY(!app->styleSheet().isEmpty());

        // While a stylesheet is active QApplication::style() is the anonymous
        // stylesheet proxy; clearing it exposes the base style that apply() chose.
        app->setStyleSheet(QString());
        QCOMPARE(QApplication::style()->objectName().toLower(), QString("fusion"));
    }

    void testApply_styleSheetParsesForStyledWidgets()
    {
        auto *app = qobject_cast<QApplication *>(QCoreApplication::instance());
        QVERIFY(app != nullptr);
        QTest::failOnWarning(QRegularExpression(".*[Cc]ould not parse.*"));
        QTest::failOnWarning(QRegularExpression(".*[Uu]nknown property.*"));

        theme::apply(*app);

        QWidget window;
        auto *layout = new QVBoxLayout(&window);
        auto *tabs = new QTabWidget;
        auto *tree = new QTreeWidget;
        tree->setHeaderLabels({"Name", "Size"});
        tabs->addTab(tree, "Explore");
        tabs->addTab(new QWidget, "Transfer");
        layout->addWidget(tabs);
        auto *toolBar = new QToolBar;
        toolBar->addWidget(new QToolButton);
        layout->addWidget(toolBar);
        layout->addWidget(new QPushButton("Up"));
        auto *progress = new QProgressBar;
        progress->setValue(40);
        layout->addWidget(progress);

        window.show();
        QVERIFY(QTest::qWaitForWindowExposed(&window));
        QCoreApplication::processEvents();

        app->setStyleSheet(QString());
    }
};

QTEST_MAIN(TestTheme)
#include "test_theme.moc"
