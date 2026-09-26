/**
 * @file test_filedetailspanel.cpp
 * @brief Unit tests for FileDetailsPanel file-type detection and view dispatch.
 *
 * FileDetailsPanel selects the correct stacked-widget page based on file
 * extension and provides showTextContent, showDiskDirectory, showSidDetails,
 * showError, and clear operations.
 *
 * Tests verify:
 * - isTextFile() / isHtmlFile() / isDiskImageFile() / isSidFile() dispatch
 * - showFileDetails() selects the correct stack page for each type
 * - every non-HTML state is rendered as lines on the C64 screen widget
 * - showError() writes a C64-style error line
 * - clear() shows the READY. prompt
 * - contentRequested signal emitted for content-bearing types
 */

#include "ui/c64screenwidget.h"
#include "ui/filedetailspanel.h"

#include <QSignalSpy>
#include <QtTest>

class TestFileDetailsPanel : public QObject
{
    Q_OBJECT

    static QStringList screenLines(FileDetailsPanel &panel)
    {
        auto *screen = panel.findChild<C64ScreenWidget *>();
        return screen ? screen->lines() : QStringList{};
    }

private slots:

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        FileDetailsPanel panel;
        QVERIFY(true);
    }

    // =========================================================================
    // isTextFile() — extension matching
    // =========================================================================

    void testIsTextFile_Txt_ReturnsTrue()
    {
        FileDetailsPanel panel;
        QVERIFY(panel.isTextFile("/path/to/readme.txt"));
    }

    void testIsTextFile_Sid_ReturnsFalse()
    {
        FileDetailsPanel panel;
        QVERIFY(!panel.isTextFile("/path/to/music.sid"));
    }

    void testIsTextFile_D64_ReturnsFalse()
    {
        FileDetailsPanel panel;
        QVERIFY(!panel.isTextFile("/path/to/disk.d64"));
    }

    void testIsTextFile_Html_ReturnsFalse()
    {
        // html is its own type, not generic text
        FileDetailsPanel panel;
        QVERIFY(!panel.isTextFile("/path/to/page.html"));
    }

    // =========================================================================
    // isHtmlFile() — extension matching
    // =========================================================================

    void testIsHtmlFile_Html_ReturnsTrue()
    {
        FileDetailsPanel panel;
        QVERIFY(panel.isHtmlFile("/path/to/page.html"));
    }

    void testIsHtmlFile_Htm_ReturnsTrue()
    {
        FileDetailsPanel panel;
        QVERIFY(panel.isHtmlFile("/path/to/page.htm"));
    }

    void testIsHtmlFile_Txt_ReturnsFalse()
    {
        FileDetailsPanel panel;
        QVERIFY(!panel.isHtmlFile("/path/to/readme.txt"));
    }

    // =========================================================================
    // isDiskImageFile() — extension matching
    // =========================================================================

    void testIsDiskImageFile_D64_ReturnsTrue()
    {
        FileDetailsPanel panel;
        QVERIFY(panel.isDiskImageFile("/path/to/disk.d64"));
    }

    void testIsDiskImageFile_D81_ReturnsTrue()
    {
        FileDetailsPanel panel;
        QVERIFY(panel.isDiskImageFile("/path/to/disk.d81"));
    }

    void testIsDiskImageFile_Txt_ReturnsFalse()
    {
        FileDetailsPanel panel;
        QVERIFY(!panel.isDiskImageFile("/path/to/readme.txt"));
    }

    // =========================================================================
    // isSidFile() — extension matching
    // =========================================================================

    void testIsSidFile_Sid_ReturnsTrue()
    {
        FileDetailsPanel panel;
        QVERIFY(panel.isSidFile("/path/to/music.sid"));
    }

    void testIsSidFile_Txt_ReturnsFalse()
    {
        FileDetailsPanel panel;
        QVERIFY(!panel.isSidFile("/path/to/readme.txt"));
    }

    // =========================================================================
    // showFileDetails() — emits contentRequested for text files
    // =========================================================================

    void testShowFileDetails_TextFile_EmitsContentRequested()
    {
        FileDetailsPanel panel;
        QSignalSpy spy(&panel, &FileDetailsPanel::contentRequested);

        panel.showFileDetails("/path/to/readme.txt", 1024, "Text Document");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/path/to/readme.txt"));
    }

    void testShowFileDetails_HtmlFile_EmitsContentRequested()
    {
        FileDetailsPanel panel;
        QSignalSpy spy(&panel, &FileDetailsPanel::contentRequested);

        panel.showFileDetails("/path/to/page.html", 512, "HTML Document");

        QCOMPARE(spy.count(), 1);
    }

    void testShowFileDetails_DiskImage_EmitsContentRequested()
    {
        FileDetailsPanel panel;
        QSignalSpy spy(&panel, &FileDetailsPanel::contentRequested);

        panel.showFileDetails("/path/to/disk.d64", 174848, "D64 Disk Image");

        QCOMPARE(spy.count(), 1);
    }

    void testShowFileDetails_SidFile_EmitsContentRequested()
    {
        FileDetailsPanel panel;
        QSignalSpy spy(&panel, &FileDetailsPanel::contentRequested);

        panel.showFileDetails("/path/to/music.sid", 4096, "SID Music");

        QCOMPARE(spy.count(), 1);
    }

    void testShowFileDetails_DefaultFile_DoesNotEmitContentRequested()
    {
        FileDetailsPanel panel;
        QSignalSpy spy(&panel, &FileDetailsPanel::contentRequested);

        panel.showFileDetails("/path/to/game.prg", 8192, "Program");

        QCOMPARE(spy.count(), 0);
    }

    // =========================================================================
    // showError() — does not crash in any stack state
    // =========================================================================

    void testShowError_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showError("Something went wrong");
        QVERIFY(true);
    }

    void testShowError_AfterTextContent_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/readme.txt", 100, "Text");
        panel.showError("Load failed");
        QVERIFY(true);
    }

    // =========================================================================
    // showTextContent() — does not crash
    // =========================================================================

    void testShowTextContent_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/readme.txt", 100, "Text");
        panel.showTextContent("Hello, World!");
        QVERIFY(true);
    }

    void testShowTextContent_HtmlPath_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/page.html", 100, "HTML");
        panel.showTextContent("<html><body>Test</body></html>");
        QVERIFY(true);
    }

    // =========================================================================
    // showDiskDirectory() — does not crash with empty data
    // =========================================================================

    void testShowDiskDirectory_EmptyData_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showDiskDirectory(QByteArray(), "disk.d64");
        QVERIFY(true);
    }

    // =========================================================================
    // showSidDetails() — does not crash with empty data
    // =========================================================================

    void testShowSidDetails_EmptyData_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showSidDetails(QByteArray(), "music.sid");
        QVERIFY(true);
    }

    // =========================================================================
    // clear() — resets state without crash
    // =========================================================================

    void testClear_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.clear();
        QVERIFY(true);
    }

    void testClear_AfterShowDetails_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/game.prg", 1024, "Program");
        panel.clear();
        QVERIFY(true);
    }

    // =========================================================================
    // showLoading() — does not crash
    // =========================================================================

    void testShowLoading_DoesNotCrash()
    {
        FileDetailsPanel panel;
        panel.showLoading("/path/to/large.d64");
        QVERIFY(true);
    }

    // =========================================================================
    // C64 screen content
    // =========================================================================

    void testConstruct_showsEmptyPromptOnScreen()
    {
        FileDetailsPanel panel;
        const QStringList lines = screenLines(panel);
        QCOMPARE(lines.first(), QStringLiteral("    **** R64U FILE DETAILS ****"));
        QVERIFY(lines.contains(QStringLiteral("SELECT A FILE TO VIEW ITS DETAILS.")));
        QCOMPARE(lines.last(), QStringLiteral("READY."));
    }

    void testClear_afterDetails_showsEmptyPromptAgain()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/game.prg", 1024, "Program");
        panel.clear();
        QCOMPARE(screenLines(panel).last(), QStringLiteral("READY."));
        QVERIFY(screenLines(panel).contains(QStringLiteral("SELECT A FILE TO VIEW ITS DETAILS.")));
    }

    void testShowFileDetails_Program_showsBasicStyleCard()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/games/Elite.prg", 8192, "Program");
        const QStringList lines = screenLines(panel);
        QCOMPARE(lines, (QStringList{"LOAD\"ELITE.PRG\",8,1", "", "SIZE: 8.0 KB", "TYPE: PROGRAM",
                                     "", "READY."}));
    }

    void testShowFileDetails_LongName_loadLineFitsFortyColumns()
    {
        FileDetailsPanel panel;
        const QString name(60, QLatin1Char('x'));
        panel.showFileDetails("/games/" + name + ".prg", 10, "Program");
        const QString loadLine = screenLines(panel).first();
        QCOMPARE(loadLine.size(), 40);
        QVERIFY(loadLine.startsWith(QStringLiteral("LOAD\"XXXX")));
        QVERIFY(loadLine.endsWith(QStringLiteral("\",8,1")));
    }

    void testShowFileDetails_SmallFile_showsBytes()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/tiny.crt", 512, "Cartridge");
        QVERIFY(screenLines(panel).contains(QStringLiteral("SIZE: 512 BYTES")));
        QVERIFY(screenLines(panel).contains(QStringLiteral("TYPE: CARTRIDGE")));
    }

    void testShowFileDetails_DiskImage_showsLoadingLine()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/disk.d64", 174848, "D64 Disk Image");
        QCOMPARE(screenLines(panel), QStringList{"LOADING DISK DIRECTORY..."});
    }

    void testShowFileDetails_Sid_showsLoadingLine()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/music.sid", 4096, "SID Music");
        QCOMPARE(screenLines(panel), QStringList{"LOADING SID INFO..."});
    }

    void testShowFileDetails_Text_showsLoadingLine()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/readme.txt", 100, "Text");
        QCOMPARE(screenLines(panel), QStringList{"LOADING..."});
    }

    void testShowTextContent_isUpperCasedAndWrappedOnScreen()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/readme.txt", 100, "Text");
        panel.showTextContent("Hello, World!\n" + QString(45, QLatin1Char('a')));
        const QStringList lines = screenLines(panel);
        QCOMPARE(lines.size(), 3);
        QCOMPARE(lines.at(0), QStringLiteral("HELLO, WORLD!"));
        QCOMPARE(lines.at(1), QString(40, QLatin1Char('A')));
        QCOMPARE(lines.at(2), QString(5, QLatin1Char('A')));
    }

    void testShowError_onScreen_isC64StyleErrorLine()
    {
        FileDetailsPanel panel;
        panel.showFileDetails("/path/to/readme.txt", 100, "Text");
        panel.showError("Load failed");
        QCOMPARE(screenLines(panel), QStringList{"?LOAD FAILED  ERROR"});
    }

    void testShowDiskDirectory_UnknownFormat_showsParseErrorLine()
    {
        FileDetailsPanel panel;
        panel.showDiskDirectory(QByteArray(), "disk.xyz");
        QCOMPARE(screenLines(panel), QStringList{"?UNABLE TO PARSE DISK IMAGE  ERROR"});
    }

    void testShowDiskDirectory_EmptyD64_showsListingOnScreen()
    {
        FileDetailsPanel panel;
        panel.showDiskDirectory(QByteArray(), "disk.d64");
        QVERIFY(!screenLines(panel).isEmpty());
        QVERIFY(!screenLines(panel).first().startsWith(QLatin1Char('?')));
    }

    void testShowSidDetails_BadData_showsParseErrorLine()
    {
        FileDetailsPanel panel;
        panel.showSidDetails(QByteArray(), "music.sid");
        QCOMPARE(screenLines(panel), QStringList{"?UNABLE TO PARSE SID FILE  ERROR"});
    }
};

QTEST_MAIN(TestFileDetailsPanel)
#include "test_filedetailspanel.moc"
