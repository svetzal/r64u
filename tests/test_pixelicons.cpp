/**
 * @file test_pixelicons.cpp
 * @brief Unit tests for the QIcon renderer in ui/pixelicons.h.
 *
 * Tests verify:
 * - Every icon renders to a non-null QIcon with 16px and 32px renditions
 * - The same Icon returns the cached QIcon (same cacheKey)
 * - Pixels take the palette colour of their row character
 * - The Disabled mode is a 45% alpha copy
 * - Every FileType maps to a non-null icon, folders to the Folder icon
 */

#include "ui/pixelicons.h"

#include <QtTest>

using pixeliconcore::Icon;

class TestPixelIcons : public QObject
{
    Q_OBJECT

private slots:

    void testIcon_everyIcon_isNonNullWithSixteenAndThirtyTwoPixelSizes()
    {
        for (Icon icon : pixeliconcore::AllIcons) {
            const QIcon rendered = pixelicons::icon(icon);
            QVERIFY(!rendered.isNull());
            const QList<QSize> sizes = rendered.availableSizes(QIcon::Normal);
            QVERIFY(sizes.contains(QSize(16, 16)));
            QVERIFY(sizes.contains(QSize(32, 32)));
        }
    }

    void testIcon_sameIcon_returnsCachedInstance()
    {
        const QIcon first = pixelicons::icon(Icon::Play);
        const QIcon second = pixelicons::icon(Icon::Play);
        QCOMPARE(first.cacheKey(), second.cacheKey());
        QVERIFY(first.cacheKey() != pixelicons::icon(Icon::Run).cacheKey());
    }

    void testIcon_pixelsTakeThePaletteColour()
    {
        // StarFilled row 8, column 7 is a 'y' (yellow) pixel; row 0 is transparent.
        const QImage image = pixelicons::icon(Icon::StarFilled).pixmap(16, 16).toImage();
        QCOMPARE(image.pixelColor(7, 8), QColor(0xC9, 0xD4, 0x87));
        QCOMPARE(image.pixelColor(0, 0).alpha(), 0);
    }

    void testIcon_retinaRendition_isNearestNeighbourUpscale()
    {
        const QImage small = pixelicons::icon(Icon::StarFilled).pixmap(16, 16).toImage();
        const QImage large = pixelicons::icon(Icon::StarFilled).pixmap(32, 32).toImage();
        QCOMPARE(large.size(), QSize(32, 32));
        for (int y = 0; y < 16; ++y) {
            for (int x = 0; x < 16; ++x) {
                QCOMPARE(large.pixelColor(2 * x, 2 * y), small.pixelColor(x, y));
                QCOMPARE(large.pixelColor(2 * x + 1, 2 * y + 1), small.pixelColor(x, y));
            }
        }
    }

    void testIcon_disabledMode_isFortyFivePercentAlpha()
    {
        const QIcon rendered = pixelicons::icon(Icon::Cancel);
        const QList<QSize> sizes = rendered.availableSizes(QIcon::Disabled);
        QVERIFY(sizes.contains(QSize(16, 16)));
        QVERIFY(sizes.contains(QSize(32, 32)));

        const QImage disabled = rendered.pixmap(QSize(16, 16), QIcon::Disabled).toImage();
        const int alpha = disabled.pixelColor(7, 7).alpha();
        QVERIFY2(alpha > 100 && alpha < 130, qPrintable(QString::number(alpha)));
    }

    void testFileTypeIcon_everyType_mapsToNonNullIcon()
    {
        const filetype::FileType types[] = {
            filetype::FileType::Unknown,   filetype::FileType::Directory,
            filetype::FileType::SidMusic,  filetype::FileType::ModMusic,
            filetype::FileType::Program,   filetype::FileType::Cartridge,
            filetype::FileType::DiskImage, filetype::FileType::TapeImage,
            filetype::FileType::Rom,       filetype::FileType::Config,
        };
        for (filetype::FileType type : types) {
            QVERIFY(!pixelicons::fileTypeIcon(type).isNull());
        }
    }

    void testFileTypeIcon_directoryAndProgram_mapToTheirIcons()
    {
        QCOMPARE(pixelicons::fileTypeIcon(filetype::FileType::Directory).cacheKey(),
                 pixelicons::icon(Icon::Folder).cacheKey());
        QCOMPARE(pixelicons::fileTypeIcon(filetype::FileType::Program).cacheKey(),
                 pixelicons::icon(Icon::Program).cacheKey());
        QCOMPARE(pixelicons::fileTypeIcon(filetype::FileType::Unknown).cacheKey(),
                 pixelicons::icon(Icon::File).cacheKey());
    }
};

QTEST_MAIN(TestPixelIcons)
#include "test_pixelicons.moc"
