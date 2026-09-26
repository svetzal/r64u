#ifndef FILESIZECORE_H
#define FILESIZECORE_H

#include <QLocale>
#include <QString>

/**
 * @namespace filesize
 * @brief The one way a byte count is shown to the user.
 *
 * Every place that prints a size (the file lists, the details screen, the
 * transfer progress rows) goes through humanSize() so they all read alike.
 */
namespace filesize {

/**
 * @brief Formats @p bytes the way the file lists and progress rows show a size.
 *
 * Uses the locale's traditional (1024-based) units with one decimal, e.g.
 * "916 bytes", "26.9 kB", "170.8 kB", "1.5 MB".
 */
[[nodiscard]] inline QString humanSize(qint64 bytes)
{
    return QLocale().formattedDataSize(bytes, 1, QLocale::DataSizeTraditionalFormat);
}

}  // namespace filesize

#endif  // FILESIZECORE_H
