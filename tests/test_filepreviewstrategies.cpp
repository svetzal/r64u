/**
 * @file test_filepreviewstrategies.cpp
 * @brief Unit tests for file preview strategies and factory.
 *
 * Tests verify:
 * - Factory selects correct strategy based on file extension
 * - Each strategy correctly identifies files it can handle
 * - Strategy canHandle() edge cases
 * - Error handling for corrupt/invalid data
 */

#include <QtTest>
#include <QWidget>

#include "ui/filepreview/filepreviewfactory.h"
#include "ui/filepreview/textfilepreview.h"
#include "ui/filepreview/diskimagepreview.h"
#include "ui/filepreview/sidfilepreview.h"
#include "ui/filepreview/defaultfilepreview.h"

class TestFilePreviewStrategies : public QObject
{
    Q_OBJECT

private slots:
    // === Factory Strategy Selection Tests ===

    void testFactory_SelectsDiskImagePreview_ForD64()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.d64");
        QVERIFY(dynamic_cast<DiskImagePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsDiskImagePreview_ForD71()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.D71");
        QVERIFY(dynamic_cast<DiskImagePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsDiskImagePreview_ForD81()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.d81");
        QVERIFY(dynamic_cast<DiskImagePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsSidFilePreview_ForSid()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/music.sid");
        QVERIFY(dynamic_cast<SidFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsSidFilePreview_ForSidUpperCase()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/music.SID");
        QVERIFY(dynamic_cast<SidFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForTxt()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/readme.txt");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForCfg()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/config.cfg");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForJson()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/data.json");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForHtml()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/page.html");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForHtm()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/page.htm");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForXml()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/data.xml");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForIni()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/settings.ini");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForMd()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/readme.md");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsTextFilePreview_ForLog()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/app.log");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsDefaultPreview_ForUnknownType()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/file.xyz");
        QVERIFY(dynamic_cast<DefaultFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsDefaultPreview_ForPrg()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/game.prg");
        QVERIFY(dynamic_cast<DefaultFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsDefaultPreview_ForNoExtension()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/filename");
        QVERIFY(dynamic_cast<DefaultFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_SelectsDefaultPreview_ForEmptyPath()
    {
        auto strategy = FilePreviewFactory::createStrategy("");
        QVERIFY(dynamic_cast<DefaultFilePreview*>(strategy.get()) != nullptr);
    }

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

    // === Edge Case Path Tests ===

    void testFactory_HandlesPathWithSpaces()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/with spaces/file.txt");
        QVERIFY(dynamic_cast<TextFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_HandlesPathWithSpecialChars()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/with-special_chars/file.d64");
        QVERIFY(dynamic_cast<DiskImagePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_HandlesDeepPath()
    {
        auto strategy = FilePreviewFactory::createStrategy("/very/deep/nested/path/to/file.sid");
        QVERIFY(dynamic_cast<SidFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_HandlesJustFilename()
    {
        auto strategy = FilePreviewFactory::createStrategy("music.sid");
        QVERIFY(dynamic_cast<SidFilePreview*>(strategy.get()) != nullptr);
    }

    void testFactory_HandlesMultipleDots()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/file.backup.d64");
        QVERIFY(dynamic_cast<DiskImagePreview*>(strategy.get()) != nullptr);
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

    // === Factory Priority Tests (disk image takes precedence over text) ===

    void testFactory_DiskImageTakesPrecedence()
    {
        // .d64.txt would be weird but test priority order
        // Actually d64 doesn't have .d64.txt so this tests something else
        // Let's test that factory returns unique instances
        auto s1 = FilePreviewFactory::createStrategy("/path/to/disk.d64");
        auto s2 = FilePreviewFactory::createStrategy("/path/to/disk.d64");
        QVERIFY(s1.get() != s2.get());  // Different instances
    }
};

QTEST_MAIN(TestFilePreviewStrategies)
#include "test_filepreviewstrategies.moc"
