/**
 * @file test_c64screenwidget.cpp
 * @brief Unit tests for C64ScreenWidget: wrapping, scrolling, scale and rendering.
 */

#include "ui/c64screenwidget.h"

#include <QtTest>

class TestC64ScreenWidget : public QObject
{
    Q_OBJECT

    static QStringList numberedLines(int count)
    {
        QStringList lines;
        for (int i = 0; i < count; ++i) {
            lines.append(QStringLiteral("LINE %1").arg(i));
        }
        return lines;
    }

private slots:
    // =========================================================================
    // Content
    // =========================================================================

    void testSetText_wrapsToColumns()
    {
        C64ScreenWidget screen;
        screen.setText(QStringLiteral("HELLO WORLD AGAIN\nREADY."));
        screen.setColumns(11);
        QCOMPARE(screen.lines(), (QStringList{"HELLO WORLD", "AGAIN", "READY."}));
    }

    void testSetText_defaultsToFortyColumns()
    {
        C64ScreenWidget screen;
        screen.setText(QString(50, QLatin1Char('X')));
        QCOMPARE(screen.lines().size(), 2);
        QCOMPARE(screen.lines().at(0).size(), 40);
    }

    void testSetLines_keepsLinesExactly()
    {
        C64ScreenWidget screen;
        const QString wide(60, QLatin1Char('#'));
        screen.setLines({wide, QString()});
        QCOMPARE(screen.lines(), (QStringList{wide, QString()}));
    }

    void testSetColumns_afterSetLines_doesNotRewrap()
    {
        C64ScreenWidget screen;
        const QString wide(60, QLatin1Char('#'));
        screen.setLines({wide});
        screen.setColumns(20);
        QCOMPARE(screen.lines(), QStringList{wide});
    }

    void testPetsciiUpper_upperCasesAsciiLettersOnly()
    {
        QCOMPARE(C64ScreenWidget::petsciiUpper(QStringLiteral("load\"game\",8,1 ünï")),
                 QStringLiteral("LOAD\"GAME\",8,1 üNï"));
    }

    // =========================================================================
    // Scrolling
    // =========================================================================

    void testScroll_noScrollWhenLinesFit()
    {
        C64ScreenWidget screen;
        screen.setLines(numberedLines(25));
        QCOMPARE(screen.maxScrollOffset(), 0);
        screen.setScrollOffset(5);
        QCOMPARE(screen.scrollOffset(), 0);
    }

    void testScroll_clampsToLastPage()
    {
        C64ScreenWidget screen;
        screen.setLines(numberedLines(40));
        QCOMPARE(screen.maxScrollOffset(), 15);
        screen.setScrollOffset(100);
        QCOMPARE(screen.scrollOffset(), 15);
        screen.setScrollOffset(-3);
        QCOMPARE(screen.scrollOffset(), 0);
    }

    void testScroll_setTextResetsOffset()
    {
        C64ScreenWidget screen;
        screen.setLines(numberedLines(40));
        screen.setScrollOffset(10);
        screen.setText(QStringLiteral("NEW"));
        QCOMPARE(screen.scrollOffset(), 0);
    }

    void testScroll_setRowsReclampsOffset()
    {
        C64ScreenWidget screen;
        screen.setLines(numberedLines(40));
        screen.setScrollOffset(15);
        screen.setRows(30);
        QCOMPARE(screen.scrollOffset(), 10);
    }

    void testScroll_keysMoveByLineAndPage()
    {
        C64ScreenWidget screen;
        screen.setLines(numberedLines(100));
        QTest::keyClick(&screen, Qt::Key_Down);
        QCOMPARE(screen.scrollOffset(), 1);
        QTest::keyClick(&screen, Qt::Key_PageDown);
        QCOMPARE(screen.scrollOffset(), 26);
        QTest::keyClick(&screen, Qt::Key_Up);
        QCOMPARE(screen.scrollOffset(), 25);
        QTest::keyClick(&screen, Qt::Key_PageUp);
        QCOMPARE(screen.scrollOffset(), 0);
    }

    void testScroll_wheelMovesThreeLinesPerNotch()
    {
        C64ScreenWidget screen;
        screen.setLines(numberedLines(100));
        QWheelEvent down(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, -120), Qt::NoButton,
                         Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&screen, &down);
        QCOMPARE(screen.scrollOffset(), 3);
        QWheelEvent up(QPointF(10, 10), QPointF(10, 10), QPoint(), QPoint(0, 120), Qt::NoButton,
                       Qt::NoModifier, Qt::NoScrollPhase, false);
        QCoreApplication::sendEvent(&screen, &up);
        QCOMPARE(screen.scrollOffset(), 0);
    }

    // =========================================================================
    // Geometry
    // =========================================================================

    void testSizeHint_isOneScaleUnit()
    {
        C64ScreenWidget screen;
        QCOMPARE(screen.sizeHint(), QSize(384, 264));
        QCOMPARE(screen.minimumSizeHint(), screen.sizeHint());
    }

    void testScale_followsWidgetSize()
    {
        C64ScreenWidget screen;
        screen.resize(100, 100);
        QCOMPARE(screen.scale(), 1);
        screen.resize(384, 264);
        QCOMPARE(screen.scale(), 1);
        screen.resize(800, 600);
        QCOMPARE(screen.scale(), 2);
        screen.resize(1200, 800);
        QCOMPARE(screen.scale(), 3);
    }

    void testScale_customColumnsAndRows()
    {
        C64ScreenWidget screen;
        screen.setColumns(20);
        screen.setRows(10);
        QCOMPARE(screen.sizeHint(), QSize(224, 144));
        screen.resize(448, 288);
        QCOMPARE(screen.scale(), 2);
    }

    // =========================================================================
    // Rendering
    // =========================================================================

    void testGrab_rendersWithoutError()
    {
        C64ScreenWidget screen;
        screen.resize(800, 600);
        screen.setLines(numberedLines(40));
        screen.setScrollOffset(5);
        const QPixmap pixmap = screen.grab();
        QVERIFY(!pixmap.isNull());
        QCOMPARE(pixmap.size(), QSize(800, 600));
    }
};

QTEST_MAIN(TestC64ScreenWidget)
#include "test_c64screenwidget.moc"
