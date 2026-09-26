#ifndef PIXELICONS_H
#define PIXELICONS_H

#include "core/filetypecore.h"
#include "core/pixeliconcore.h"

#include <QHash>
#include <QIcon>
#include <QImage>
#include <QPainter>
#include <QPixmap>

/**
 * @brief Renders the pixel icon set in `core/pixeliconcore.h` to QIcons.
 *
 * Each icon is drawn once, at 16px and as a 32px nearest-neighbour upscale for
 * Retina displays, in Normal and (45% alpha) Disabled modes, and then cached.
 */
namespace pixelicons {

/// Alpha applied to the Disabled rendition of every icon.
inline constexpr qreal DisabledOpacity = 0.45;

namespace detail {

inline QImage paintRows(pixeliconcore::Icon icon)
{
    using pixeliconcore::Size;
    QImage image(Size, Size, QImage::Format_ARGB32);
    image.fill(Qt::transparent);
    const pixeliconcore::Rows &rows = pixeliconcore::rows(icon);
    for (int y = 0; y < Size; ++y) {
        for (int x = 0; x < Size; ++x) {
            if (const auto rgb = pixeliconcore::colourFor(rows[y][x])) {
                image.setPixel(x, y, qRgb(rgb->r, rgb->g, rgb->b));
            }
        }
    }
    return image;
}

inline QImage withOpacity(const QImage &source, qreal opacity)
{
    QImage faded(source.size(), QImage::Format_ARGB32);
    faded.fill(Qt::transparent);
    QPainter painter(&faded);
    painter.setOpacity(opacity);
    painter.drawImage(0, 0, source);
    return faded;
}

inline QIcon render(pixeliconcore::Icon icon)
{
    const QImage base = paintRows(icon);
    const QImage retina =
        base.scaled(base.size() * 2, Qt::IgnoreAspectRatio, Qt::FastTransformation);

    QIcon result;
    result.addPixmap(QPixmap::fromImage(base), QIcon::Normal);
    result.addPixmap(QPixmap::fromImage(retina), QIcon::Normal);
    result.addPixmap(QPixmap::fromImage(withOpacity(base, DisabledOpacity)), QIcon::Disabled);
    result.addPixmap(QPixmap::fromImage(withOpacity(retina, DisabledOpacity)), QIcon::Disabled);
    return result;
}

}  // namespace detail

/**
 * @brief The QIcon for @p icon, rendered on first use and cached thereafter.
 *
 * Copies of the returned QIcon share one cache key, so callers may compare
 * `cacheKey()` to tell which icon an action is showing.
 */
inline QIcon icon(pixeliconcore::Icon icon)
{
    static QHash<int, QIcon> cache;
    const int key = static_cast<int>(icon);
    auto it = cache.find(key);
    if (it == cache.end()) {
        it = cache.insert(key, detail::render(icon));
    }
    return *it;
}

/**
 * @brief The file-type icon shown beside a file or folder in the browsers.
 */
inline QIcon fileTypeIcon(filetype::FileType type)
{
    using pixeliconcore::Icon;
    switch (type) {
    case filetype::FileType::Directory:
        return icon(Icon::Folder);
    case filetype::FileType::SidMusic:
        return icon(Icon::Sid);
    case filetype::FileType::ModMusic:
        return icon(Icon::Mod);
    case filetype::FileType::Program:
        return icon(Icon::Program);
    case filetype::FileType::Cartridge:
        return icon(Icon::Cartridge);
    case filetype::FileType::DiskImage:
        return icon(Icon::DiskImage);
    case filetype::FileType::TapeImage:
        return icon(Icon::Tape);
    case filetype::FileType::Rom:
        return icon(Icon::Rom);
    case filetype::FileType::Config:
        return icon(Icon::Config);
    case filetype::FileType::Unknown:
        break;
    }
    return icon(Icon::File);
}

}  // namespace pixelicons

#endif  // PIXELICONS_H
