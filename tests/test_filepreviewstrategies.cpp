/**
 * @file test_filepreviewstrategies.cpp
 * @brief Unit tests for file preview strategies.
 *
 * Tests verify:
 * - Each strategy correctly identifies files it can handle
 * - Strategy canHandle() edge cases
 * - Error handling for corrupt/invalid data
 *
 * Note: FilePreviewFactory was removed as it was not used in production code.
 * Strategy selection logic is now tested directly via canHandle().
 */

#include "ui/filepreview/defaultfilepreview.h"
#include "ui/filepreview/diskimagepreview.h"
#include "ui/filepreview/sidfilepreview.h"
#include "ui/filepreview/textfilepreview.h"

#include <QWidget>
#include <QtTest>

class TestFilePreviewStrategies : public QObject
{
    Q_OBJECT

private slots:
    // === DiskImagePreview canHandle Tests ===

    void testDiskImagePreview_CanHandle_D64()
    {
        DiskImagePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/disk.d64"));
        QVERIFY(strategy.canHandle("/path/to/disk.D64"));
    }

    void testDiskImagePreview_CanHandle_D71()
    {
        DiskImagePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/disk.d71"));
        QVERIFY(strategy.canHandle("/path/to/disk.D71"));
    }

    void testDiskImagePreview_CanHandle_D81()
    {
        DiskImagePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/disk.d81"));
        QVERIFY(strategy.canHandle("/path/to/disk.D81"));
    }

    void testDiskImagePreview_CannotHandle_Prg()
    {
        DiskImagePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/game.prg"));
    }

    void testDiskImagePreview_CannotHandle_Sid()
    {
        DiskImagePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/music.sid"));
    }

    void testDiskImagePreview_CannotHandle_Txt()
    {
        DiskImagePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/readme.txt"));
    }

    // === SidFilePreview canHandle Tests ===

    void testSidFilePreview_CanHandle_Sid()
    {
        SidFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/music.sid"));
        QVERIFY(strategy.canHandle("/path/to/music.SID"));
    }

    void testSidFilePreview_CannotHandle_D64()
    {
        SidFilePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/disk.d64"));
    }

    void testSidFilePreview_CannotHandle_Prg()
    {
        SidFilePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/game.prg"));
    }

    void testSidFilePreview_CannotHandle_Txt()
    {
        SidFilePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/readme.txt"));
    }

    // === TextFilePreview canHandle Tests ===

    void testTextFilePreview_CanHandle_Txt()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/readme.txt"));
        QVERIFY(strategy.canHandle("/path/to/readme.TXT"));
    }

    void testTextFilePreview_CanHandle_Cfg()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/config.cfg"));
        QVERIFY(strategy.canHandle("/path/to/config.CFG"));
    }

    void testTextFilePreview_CanHandle_Html()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/page.html"));
        QVERIFY(strategy.canHandle("/path/to/page.HTML"));
    }

    void testTextFilePreview_CanHandle_Htm()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/page.htm"));
        QVERIFY(strategy.canHandle("/path/to/page.HTM"));
    }

    void testTextFilePreview_CanHandle_Json()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/data.json"));
    }

    void testTextFilePreview_CanHandle_Xml()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/data.xml"));
    }

    void testTextFilePreview_CanHandle_Ini()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/settings.ini"));
    }

    void testTextFilePreview_CanHandle_Md()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/readme.md"));
    }

    void testTextFilePreview_CanHandle_Log()
    {
        TextFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/app.log"));
    }

    void testTextFilePreview_CannotHandle_D64()
    {
        TextFilePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/disk.d64"));
    }

    void testTextFilePreview_CannotHandle_Sid()
    {
        TextFilePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/music.sid"));
    }

    void testTextFilePreview_CannotHandle_Prg()
    {
        TextFilePreview strategy;
        QVERIFY(!strategy.canHandle("/path/to/game.prg"));
    }

    // === DefaultFilePreview canHandle Tests ===

    void testDefaultFilePreview_CanHandle_AnyFile()
    {
        DefaultFilePreview strategy;
        QVERIFY(strategy.canHandle("/path/to/any.xyz"));
        QVERIFY(strategy.canHandle("/path/to/file.prg"));
        QVERIFY(strategy.canHandle("/path/to/noextension"));
        QVERIFY(strategy.canHandle(""));
    }

    // === Widget Creation Tests ===

    void testTextFilePreview_CreatesWidget()
    {
        TextFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        QVERIFY(widget != nullptr);
        delete widget;
    }

    void testDiskImagePreview_CreatesWidget()
    {
        DiskImagePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        QVERIFY(widget != nullptr);
        delete widget;
    }

    void testSidFilePreview_CreatesWidget()
    {
        SidFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        QVERIFY(widget != nullptr);
        delete widget;
    }

    void testDefaultFilePreview_CreatesWidget()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        QVERIFY(widget != nullptr);
        delete widget;
    }

    // === Error Handling Tests ===

    void testTextFilePreview_ShowError_DoesNotCrash()
    {
        TextFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showError("Test error message");
        delete widget;
    }

    void testDiskImagePreview_ShowError_DoesNotCrash()
    {
        DiskImagePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showError("Test error message");
        delete widget;
    }

    void testSidFilePreview_ShowError_DoesNotCrash()
    {
        SidFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showError("Test error message");
        delete widget;
    }

    void testDefaultFilePreview_ShowError_DoesNotCrash()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showError("Test error message");
        delete widget;
    }

    // === Clear Tests ===

    void testTextFilePreview_Clear_DoesNotCrash()
    {
        TextFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.clear();
        delete widget;
    }

    void testDiskImagePreview_Clear_DoesNotCrash()
    {
        DiskImagePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.clear();
        delete widget;
    }

    void testSidFilePreview_Clear_DoesNotCrash()
    {
        SidFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.clear();
        delete widget;
    }

    void testDefaultFilePreview_Clear_DoesNotCrash()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.clear();
        delete widget;
    }

    // === ShowLoading Tests ===

    void testTextFilePreview_ShowLoading_DoesNotCrash()
    {
        TextFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showLoading("/path/to/file.txt");
        delete widget;
    }

    void testDiskImagePreview_ShowLoading_DoesNotCrash()
    {
        DiskImagePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showLoading("/path/to/disk.d64");
        delete widget;
    }

    void testSidFilePreview_ShowLoading_DoesNotCrash()
    {
        SidFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showLoading("/path/to/music.sid");
        delete widget;
    }

    void testDefaultFilePreview_ShowLoading_DoesNotCrash()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showLoading("/path/to/file.xyz");
        delete widget;
    }

    // === Preview Tests with Empty Data ===

    void testTextFilePreview_ShowPreview_EmptyData()
    {
        TextFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showPreview("/path/to/empty.txt", QByteArray());
        delete widget;
    }

    void testDiskImagePreview_ShowPreview_EmptyData()
    {
        DiskImagePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showPreview("/path/to/empty.d64", QByteArray());
        delete widget;
    }

    void testSidFilePreview_ShowPreview_EmptyData()
    {
        SidFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showPreview("/path/to/empty.sid", QByteArray());
        delete widget;
    }

    void testDefaultFilePreview_ShowPreview_EmptyData()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.showPreview("/path/to/empty.xyz", QByteArray());
        delete widget;
    }

    // === Preview Tests with Invalid Data ===

    void testTextFilePreview_ShowPreview_ValidUtf8()
    {
        TextFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        QByteArray textData = "Hello, World!\nThis is a test.";
        strategy.showPreview("/path/to/test.txt", textData);
        delete widget;
    }

    void testTextFilePreview_ShowPreview_Html()
    {
        TextFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        QByteArray htmlData = "<html><body><h1>Test</h1></body></html>";
        strategy.showPreview("/path/to/test.html", htmlData);
        delete widget;
    }

    void testDiskImagePreview_ShowPreview_InvalidData()
    {
        DiskImagePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        // Invalid disk image data - should show error without crashing
        QByteArray invalidData = "This is not a valid disk image";
        strategy.showPreview("/path/to/invalid.d64", invalidData);
        delete widget;
    }

    void testSidFilePreview_ShowPreview_InvalidData()
    {
        SidFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        // Invalid SID data - should show error without crashing
        QByteArray invalidData = "This is not a valid SID file";
        strategy.showPreview("/path/to/invalid.sid", invalidData);
        delete widget;
    }

    // === DefaultFilePreview setFileDetails Tests ===

    void testDefaultFilePreview_SetFileDetails_Small()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.setFileDetails("/path/to/file.prg", 100, "Program");
        delete widget;
    }

    void testDefaultFilePreview_SetFileDetails_KB()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.setFileDetails("/path/to/file.prg", 15000, "Program");
        delete widget;
    }

    void testDefaultFilePreview_SetFileDetails_MB()
    {
        DefaultFilePreview strategy;
        QWidget *widget = strategy.createPreviewWidget(nullptr);
        strategy.setFileDetails("/path/to/file.prg", 2500000, "Program");
        delete widget;
    }
};

QTEST_MAIN(TestFilePreviewStrategies)
#include "test_filepreviewstrategies.moc"
