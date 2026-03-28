/**
 * @file test_fileactioncore.cpp
 * @brief Tests for the pure fileaction namespace functions.
 *
 * Covers preview type detection and preview data routing.
 * No Qt widgets, no network, no filesystem — pure unit tests.
 */

#include "services/fileactioncore.h"

#include <QByteArray>
#include <QTest>

class TestFileActionCore : public QObject
{
    Q_OBJECT

private slots:
    // ── detectPreviewType ───────────────────────────────────────────────────

    void testDetectPreviewType_d64_returnsDiskImage();
    void testDetectPreviewType_d71_returnsDiskImage();
    void testDetectPreviewType_d81_returnsDiskImage();
    void testDetectPreviewType_g64_returnsDiskImage();
    void testDetectPreviewType_g71_returnsDiskImage();
    void testDetectPreviewType_diskImageExtensionsAreCaseInsensitive();

    void testDetectPreviewType_sid_returnsSidMusic();
    void testDetectPreviewType_psid_returnsSidMusic();
    void testDetectPreviewType_rsid_returnsSidMusic();
    void testDetectPreviewType_sidExtensionsAreCaseInsensitive();

    void testDetectPreviewType_html_returnsHtml();
    void testDetectPreviewType_htm_returnsHtml();

    void testDetectPreviewType_cfg_returnsText();
    void testDetectPreviewType_txt_returnsText();
    void testDetectPreviewType_log_returnsText();
    void testDetectPreviewType_ini_returnsText();
    void testDetectPreviewType_md_returnsText();
    void testDetectPreviewType_json_returnsText();
    void testDetectPreviewType_xml_returnsText();

    void testDetectPreviewType_prg_returnsUnknown();
    void testDetectPreviewType_crt_returnsUnknown();
    void testDetectPreviewType_rom_returnsUnknown();
    void testDetectPreviewType_tap_returnsUnknown();
    void testDetectPreviewType_noExtension_returnsUnknown();

    // ── requiresContentFetch ────────────────────────────────────────────────

    void testRequiresContentFetch_diskImage_returnsTrue();
    void testRequiresContentFetch_sidMusic_returnsTrue();
    void testRequiresContentFetch_html_returnsTrue();
    void testRequiresContentFetch_text_returnsTrue();
    void testRequiresContentFetch_unknown_returnsFalse();

    // ── routePreviewData ────────────────────────────────────────────────────

    void testRoutePreviewData_diskImage_returnsShowDiskDirectory();
    void testRoutePreviewData_diskImage_containsCorrectDataAndPath();

    void testRoutePreviewData_sid_returnsShowSidDetails();
    void testRoutePreviewData_sid_withPlaylistManager_setsUpdatePlaylistTrue();
    void testRoutePreviewData_sid_withoutPlaylistManager_setsUpdatePlaylistFalse();

    void testRoutePreviewData_text_returnsShowTextContent();
    void testRoutePreviewData_text_decodesUtf8();

    void testRoutePreviewData_html_returnsShowTextContent();
    void testRoutePreviewData_unknown_returnsShowTextContent();
};

// ── detectPreviewType ───────────────────────────────────────────────────────

void TestFileActionCore::testDetectPreviewType_d64_returnsDiskImage()
{
    QCOMPARE(fileaction::detectPreviewType("game.d64"), fileaction::PreviewContentType::DiskImage);
}

void TestFileActionCore::testDetectPreviewType_d71_returnsDiskImage()
{
    QCOMPARE(fileaction::detectPreviewType("disk.d71"), fileaction::PreviewContentType::DiskImage);
}

void TestFileActionCore::testDetectPreviewType_d81_returnsDiskImage()
{
    QCOMPARE(fileaction::detectPreviewType("disk.d81"), fileaction::PreviewContentType::DiskImage);
}

void TestFileActionCore::testDetectPreviewType_g64_returnsDiskImage()
{
    QCOMPARE(fileaction::detectPreviewType("track.g64"), fileaction::PreviewContentType::DiskImage);
}

void TestFileActionCore::testDetectPreviewType_g71_returnsDiskImage()
{
    QCOMPARE(fileaction::detectPreviewType("track.g71"), fileaction::PreviewContentType::DiskImage);
}

void TestFileActionCore::testDetectPreviewType_diskImageExtensionsAreCaseInsensitive()
{
    QCOMPARE(fileaction::detectPreviewType("game.D64"), fileaction::PreviewContentType::DiskImage);
    QCOMPARE(fileaction::detectPreviewType("game.D81"), fileaction::PreviewContentType::DiskImage);
}

void TestFileActionCore::testDetectPreviewType_sid_returnsSidMusic()
{
    QCOMPARE(fileaction::detectPreviewType("tune.sid"), fileaction::PreviewContentType::SidMusic);
}

void TestFileActionCore::testDetectPreviewType_psid_returnsSidMusic()
{
    QCOMPARE(fileaction::detectPreviewType("tune.psid"), fileaction::PreviewContentType::SidMusic);
}

void TestFileActionCore::testDetectPreviewType_rsid_returnsSidMusic()
{
    QCOMPARE(fileaction::detectPreviewType("tune.rsid"), fileaction::PreviewContentType::SidMusic);
}

void TestFileActionCore::testDetectPreviewType_sidExtensionsAreCaseInsensitive()
{
    QCOMPARE(fileaction::detectPreviewType("tune.SID"), fileaction::PreviewContentType::SidMusic);
    QCOMPARE(fileaction::detectPreviewType("tune.PSID"), fileaction::PreviewContentType::SidMusic);
}

void TestFileActionCore::testDetectPreviewType_html_returnsHtml()
{
    QCOMPARE(fileaction::detectPreviewType("page.html"), fileaction::PreviewContentType::Html);
}

void TestFileActionCore::testDetectPreviewType_htm_returnsHtml()
{
    QCOMPARE(fileaction::detectPreviewType("page.htm"), fileaction::PreviewContentType::Html);
}

void TestFileActionCore::testDetectPreviewType_cfg_returnsText()
{
    QCOMPARE(fileaction::detectPreviewType("config.cfg"), fileaction::PreviewContentType::Text);
}

void TestFileActionCore::testDetectPreviewType_txt_returnsText()
{
    QCOMPARE(fileaction::detectPreviewType("notes.txt"), fileaction::PreviewContentType::Text);
}

void TestFileActionCore::testDetectPreviewType_log_returnsText()
{
    QCOMPARE(fileaction::detectPreviewType("app.log"), fileaction::PreviewContentType::Text);
}

void TestFileActionCore::testDetectPreviewType_ini_returnsText()
{
    QCOMPARE(fileaction::detectPreviewType("settings.ini"), fileaction::PreviewContentType::Text);
}

void TestFileActionCore::testDetectPreviewType_md_returnsText()
{
    QCOMPARE(fileaction::detectPreviewType("README.md"), fileaction::PreviewContentType::Text);
}

void TestFileActionCore::testDetectPreviewType_json_returnsText()
{
    QCOMPARE(fileaction::detectPreviewType("data.json"), fileaction::PreviewContentType::Text);
}

void TestFileActionCore::testDetectPreviewType_xml_returnsText()
{
    QCOMPARE(fileaction::detectPreviewType("config.xml"), fileaction::PreviewContentType::Text);
}

void TestFileActionCore::testDetectPreviewType_prg_returnsUnknown()
{
    QCOMPARE(fileaction::detectPreviewType("game.prg"), fileaction::PreviewContentType::Unknown);
}

void TestFileActionCore::testDetectPreviewType_crt_returnsUnknown()
{
    QCOMPARE(fileaction::detectPreviewType("cart.crt"), fileaction::PreviewContentType::Unknown);
}

void TestFileActionCore::testDetectPreviewType_rom_returnsUnknown()
{
    QCOMPARE(fileaction::detectPreviewType("basic.rom"), fileaction::PreviewContentType::Unknown);
}

void TestFileActionCore::testDetectPreviewType_tap_returnsUnknown()
{
    QCOMPARE(fileaction::detectPreviewType("tape.tap"), fileaction::PreviewContentType::Unknown);
}

void TestFileActionCore::testDetectPreviewType_noExtension_returnsUnknown()
{
    QCOMPARE(fileaction::detectPreviewType("FILEWITHNOEXT"),
             fileaction::PreviewContentType::Unknown);
}

// ── requiresContentFetch ────────────────────────────────────────────────────

void TestFileActionCore::testRequiresContentFetch_diskImage_returnsTrue()
{
    QVERIFY(fileaction::requiresContentFetch(fileaction::PreviewContentType::DiskImage));
}

void TestFileActionCore::testRequiresContentFetch_sidMusic_returnsTrue()
{
    QVERIFY(fileaction::requiresContentFetch(fileaction::PreviewContentType::SidMusic));
}

void TestFileActionCore::testRequiresContentFetch_html_returnsTrue()
{
    QVERIFY(fileaction::requiresContentFetch(fileaction::PreviewContentType::Html));
}

void TestFileActionCore::testRequiresContentFetch_text_returnsTrue()
{
    QVERIFY(fileaction::requiresContentFetch(fileaction::PreviewContentType::Text));
}

void TestFileActionCore::testRequiresContentFetch_unknown_returnsFalse()
{
    QVERIFY(!fileaction::requiresContentFetch(fileaction::PreviewContentType::Unknown));
}

// ── routePreviewData ────────────────────────────────────────────────────────

void TestFileActionCore::testRoutePreviewData_diskImage_returnsShowDiskDirectory()
{
    const QByteArray data = QByteArray("disk", 4);
    auto action = fileaction::routePreviewData("/SD/game.d64", data, false);
    QVERIFY(std::holds_alternative<fileaction::ShowDiskDirectory>(action));
}

void TestFileActionCore::testRoutePreviewData_diskImage_containsCorrectDataAndPath()
{
    const QByteArray data{"disk_bytes", 10};
    auto action = fileaction::routePreviewData("/SD/game.d64", data, false);
    const auto &show = std::get<fileaction::ShowDiskDirectory>(action);
    QCOMPARE(show.data, data);
    QCOMPARE(show.path, QString("/SD/game.d64"));
}

void TestFileActionCore::testRoutePreviewData_sid_returnsShowSidDetails()
{
    const QByteArray data{"PSID", 4};
    auto action = fileaction::routePreviewData("/HVSC/tune.sid", data, false);
    QVERIFY(std::holds_alternative<fileaction::ShowSidDetails>(action));
}

void TestFileActionCore::testRoutePreviewData_sid_withPlaylistManager_setsUpdatePlaylistTrue()
{
    const QByteArray data{"PSID", 4};
    auto action = fileaction::routePreviewData("/HVSC/tune.sid", data, /*hasPlaylistManager=*/true);
    const auto &show = std::get<fileaction::ShowSidDetails>(action);
    QVERIFY(show.updatePlaylist);
}

void TestFileActionCore::testRoutePreviewData_sid_withoutPlaylistManager_setsUpdatePlaylistFalse()
{
    const QByteArray data{"PSID", 4};
    auto action =
        fileaction::routePreviewData("/HVSC/tune.sid", data, /*hasPlaylistManager=*/false);
    const auto &show = std::get<fileaction::ShowSidDetails>(action);
    QVERIFY(!show.updatePlaylist);
}

void TestFileActionCore::testRoutePreviewData_text_returnsShowTextContent()
{
    const QByteArray data{"Hello, World!", 13};
    auto action = fileaction::routePreviewData("/SD/notes.txt", data, false);
    QVERIFY(std::holds_alternative<fileaction::ShowTextContent>(action));
}

void TestFileActionCore::testRoutePreviewData_text_decodesUtf8()
{
    const QString expected = "Hello\nWorld";
    const QByteArray utf8 = expected.toUtf8();
    auto action = fileaction::routePreviewData("/SD/notes.txt", utf8, false);
    const auto &show = std::get<fileaction::ShowTextContent>(action);
    QCOMPARE(show.content, expected);
}

void TestFileActionCore::testRoutePreviewData_html_returnsShowTextContent()
{
    const QByteArray data{"<html></html>", 13};
    auto action = fileaction::routePreviewData("/SD/page.html", data, false);
    QVERIFY(std::holds_alternative<fileaction::ShowTextContent>(action));
}

void TestFileActionCore::testRoutePreviewData_unknown_returnsShowTextContent()
{
    const QByteArray data{"\x00\x01\x02", 3};
    auto action = fileaction::routePreviewData("/SD/game.prg", data, false);
    QVERIFY(std::holds_alternative<fileaction::ShowTextContent>(action));
}

QTEST_GUILESS_MAIN(TestFileActionCore)
#include "test_fileactioncore.moc"
