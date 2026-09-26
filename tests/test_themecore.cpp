/**
 * @file test_themecore.cpp
 * @brief Unit tests for the themecore colour tokens.
 *
 * Tests verify:
 * - Light and dark schemes produce different backgrounds and text colours
 * - Every token is a valid colour in both schemes
 * - The rainbow accents are never inverted (both schemes keep a warm red, cool blue)
 * - tokensFor(Unknown) falls back to the light palette
 * - effectiveScheme() only ever reports Light or Dark
 */

#include "core/themecore.h"

#include <QtTest>

class TestThemeCore : public QObject
{
    Q_OBJECT

private slots:

    void testTokensFor_lightAndDarkDiffer()
    {
        const auto light = themecore::tokensFor(Qt::ColorScheme::Light);
        const auto dark = themecore::tokensFor(Qt::ColorScheme::Dark);

        QVERIFY(light.bgApp != dark.bgApp);
        QVERIFY(light.bgPanel != dark.bgPanel);
        QVERIFY(light.textPrimary != dark.textPrimary);
        QVERIFY(light.textMuted != dark.textMuted);
    }

    void testTokensFor_light_isBeigePlastic()
    {
        const auto light = themecore::tokensFor(Qt::ColorScheme::Light);
        QCOMPARE(light.bgApp.name(), QString("#e8dfc8"));
        QCOMPARE(light.textPrimary.name(), QString("#1e1e1e"));
    }

    void testTokensFor_dark_isDeskMat()
    {
        const auto dark = themecore::tokensFor(Qt::ColorScheme::Dark);
        QCOMPARE(dark.bgApp.name(), QString("#1e1f22"));
        QCOMPARE(dark.textPrimary.name(), QString("#f5f5f5"));
    }

    void testTokensFor_unknown_fallsBackToLight()
    {
        const auto unknown = themecore::tokensFor(Qt::ColorScheme::Unknown);
        const auto light = themecore::tokensFor(Qt::ColorScheme::Light);
        QCOMPARE(unknown.bgApp, light.bgApp);
        QCOMPARE(unknown.textPrimary, light.textPrimary);
    }

    void testTokens_allValid_data()
    {
        QTest::addColumn<int>("scheme");
        QTest::newRow("light") << static_cast<int>(Qt::ColorScheme::Light);
        QTest::newRow("dark") << static_cast<int>(Qt::ColorScheme::Dark);
    }

    void testTokens_allValid()
    {
        QFETCH(int, scheme);
        const auto t = themecore::tokensFor(static_cast<Qt::ColorScheme>(scheme));

        const QList<QColor> all = {
            t.bgApp,        t.bgPanel,       t.bgInset,     t.borderSubtle, t.borderStrong,
            t.textPrimary,  t.textSecondary, t.textMuted,   t.textInverted, t.accentRed,
            t.accentOrange, t.accentYellow,  t.accentGreen, t.accentBlue,   t.stateConnected,
            t.stateWarning, t.stateError,    t.stateInfo,   t.ledOn,        t.ledOff,
            t.vicBlue,      t.vicLightBlue,
        };
        for (const QColor &c : all) {
            QVERIFY(c.isValid());
        }
    }

    void testTokens_rainbowIsNotInverted()
    {
        const auto light = themecore::tokensFor(Qt::ColorScheme::Light);
        const auto dark = themecore::tokensFor(Qt::ColorScheme::Dark);

        // Red stays red-dominant and blue stays blue-dominant in both schemes.
        QVERIFY(light.accentRed.red() > light.accentRed.blue());
        QVERIFY(dark.accentRed.red() > dark.accentRed.blue());
        QVERIFY(light.accentBlue.blue() > light.accentBlue.red());
        QVERIFY(dark.accentBlue.blue() > dark.accentBlue.red());
    }

    void testTokens_vicBluesMatchC64Look()
    {
        const auto t = themecore::tokensFor(Qt::ColorScheme::Light);
        QCOMPARE(t.vicBlue.name(), QString("#4040e8"));
        QCOMPARE(t.vicLightBlue.name(), QString("#887ecb"));
    }

    void testEffectiveScheme_isLightOrDark()
    {
        const Qt::ColorScheme scheme = themecore::effectiveScheme();
        QVERIFY(scheme == Qt::ColorScheme::Light || scheme == Qt::ColorScheme::Dark);
    }

    void testCurrentTokens_matchEffectiveScheme()
    {
        const auto expected = themecore::tokensFor(themecore::effectiveScheme());
        QCOMPARE(themecore::currentTokens().bgApp, expected.bgApp);
    }
};

QTEST_MAIN(TestThemeCore)
#include "test_themecore.moc"
