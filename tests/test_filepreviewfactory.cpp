/**
 * @file test_filepreviewfactory.cpp
 * @brief Unit tests for FilePreviewFactory strategy selection.
 *
 * FilePreviewFactory::createStrategy() examines the file extension and returns
 * the appropriate preview strategy.  These tests verify:
 *
 * - Returns DiskImagePreview for .d64, .d71, .d81, .g64
 * - Returns SidFilePreview for .sid
 * - Returns TextFilePreview for .txt, .html, .htm
 * - Returns DefaultFilePreview for unknown extensions
 *
 * The returned strategy must also pass its own canHandle() for the given path.
 */

#include "ui/filepreview/defaultfilepreview.h"
#include "ui/filepreview/diskimagepreview.h"
#include "ui/filepreview/filepreviewfactory.h"
#include "ui/filepreview/sidfilepreview.h"
#include "ui/filepreview/textfilepreview.h"

#include <QtTest>

class TestFilePreviewFactory : public QObject
{
    Q_OBJECT

private slots:

    // =========================================================================
    // DiskImagePreview — returned for disk image extensions
    // =========================================================================

    void testCreateStrategy_D64_ReturnsDiskImagePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.d64");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DiskImagePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_D71_ReturnsDiskImagePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.d71");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DiskImagePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_D81_ReturnsDiskImagePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.d81");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DiskImagePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_G64_ReturnsDiskImagePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.g64");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DiskImagePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_D64Uppercase_ReturnsDiskImagePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.D64");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DiskImagePreview *>(strategy.get()) != nullptr);
    }

    // =========================================================================
    // SidFilePreview — returned for .sid
    // =========================================================================

    void testCreateStrategy_Sid_ReturnsSidFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/music.sid");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<SidFilePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_SidUppercase_ReturnsSidFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/music.SID");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<SidFilePreview *>(strategy.get()) != nullptr);
    }

    // =========================================================================
    // TextFilePreview — returned for text / HTML extensions
    // =========================================================================

    void testCreateStrategy_Txt_ReturnsTextFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/readme.txt");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<TextFilePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_Html_ReturnsTextFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/page.html");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<TextFilePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_Htm_ReturnsTextFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/page.htm");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<TextFilePreview *>(strategy.get()) != nullptr);
    }

    // =========================================================================
    // DefaultFilePreview — returned for unknown extensions
    // =========================================================================

    void testCreateStrategy_Prg_ReturnsDefaultFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/game.prg");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DefaultFilePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_UnknownExt_ReturnsDefaultFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/file.xyz");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DefaultFilePreview *>(strategy.get()) != nullptr);
    }

    void testCreateStrategy_NoExtension_ReturnsDefaultFilePreview()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/noextension");
        QVERIFY(strategy != nullptr);
        QVERIFY(dynamic_cast<DefaultFilePreview *>(strategy.get()) != nullptr);
    }

    // =========================================================================
    // Strategy canHandle() round-trip for each factory result
    // =========================================================================

    void testCreateStrategy_D64_CanHandle()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/disk.d64");
        QVERIFY(strategy->canHandle("/path/to/disk.d64"));
    }

    void testCreateStrategy_Sid_CanHandle()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/music.sid");
        QVERIFY(strategy->canHandle("/path/to/music.sid"));
    }

    void testCreateStrategy_Txt_CanHandle()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/readme.txt");
        QVERIFY(strategy->canHandle("/path/to/readme.txt"));
    }

    void testCreateStrategy_Prg_CanHandle()
    {
        auto strategy = FilePreviewFactory::createStrategy("/path/to/game.prg");
        QVERIFY(strategy->canHandle("/path/to/game.prg"));
    }
};

QTEST_MAIN(TestFilePreviewFactory)
#include "test_filepreviewfactory.moc"
