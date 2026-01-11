/**
 * @file filepreviewfactory.h
 * @brief Factory for creating file preview strategies.
 *
 * Selects the appropriate preview strategy based on file type
 * by checking extensions and file characteristics.
 */

#ifndef FILEPREVIEWFACTORY_H
#define FILEPREVIEWFACTORY_H

#include "filepreviewstrategy.h"
#include <QString>
#include <memory>

/**
 * @brief Factory for creating file preview strategies.
 *
 * Examines the file path and returns the appropriate strategy:
 * - TextFilePreview for text and HTML files
 * - DiskImagePreview for disk images (.d64, .g64, etc.)
 * - SidFilePreview for SID music files
 * - DefaultFilePreview as fallback
 */
class FilePreviewFactory
{
public:
    /**
     * @brief Creates the appropriate preview strategy for a file.
     * @param path The file path (used for extension checking).
     * @return Unique pointer to the selected strategy.
     */
    [[nodiscard]] static std::unique_ptr<FilePreviewStrategy> createStrategy(const QString &path);
};

#endif // FILEPREVIEWFACTORY_H
