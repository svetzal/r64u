/**
 * @file fileactioncore.cpp
 * @brief Implementation of pure file action core functions.
 */

#include "fileactioncore.h"

#include "filetypecore.h"

namespace fileaction {

PreviewContentType detectPreviewType(const QString &filename)
{
    // Delegate disk image and SID detection to the canonical authority
    filetype::FileType type = filetype::detectFromFilename(filename);
    if (type == filetype::FileType::DiskImage) {
        return PreviewContentType::DiskImage;
    }
    if (type == filetype::FileType::SidMusic) {
        return PreviewContentType::SidMusic;
    }

    // HTML and plain text are preview-specific types not in the filetype enum
    QString lower = filename.toLower();

    if (lower.endsWith(".html") || lower.endsWith(".htm")) {
        return PreviewContentType::Html;
    }

    if (lower.endsWith(".cfg") || lower.endsWith(".txt") || lower.endsWith(".log") ||
        lower.endsWith(".ini") || lower.endsWith(".md") || lower.endsWith(".json") ||
        lower.endsWith(".xml")) {
        return PreviewContentType::Text;
    }

    return PreviewContentType::Unknown;
}

bool requiresContentFetch(PreviewContentType type)
{
    return type != PreviewContentType::Unknown;
}

PreviewAction routePreviewData(const QString &remotePath, const QByteArray &data,
                               bool hasPlaylistManager)
{
    PreviewContentType type = detectPreviewType(remotePath);

    switch (type) {
    case PreviewContentType::DiskImage:
        return ShowDiskDirectory{data, remotePath};

    case PreviewContentType::SidMusic:
        return ShowSidDetails{data, remotePath, hasPlaylistManager};

    case PreviewContentType::Html:
    case PreviewContentType::Text:
    case PreviewContentType::Unknown:
    default:
        return ShowTextContent{QString::fromUtf8(data)};
    }
}

}  // namespace fileaction
