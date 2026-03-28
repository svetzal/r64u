/**
 * @file fileactioncore.cpp
 * @brief Implementation of pure file action core functions.
 */

#include "fileactioncore.h"

namespace fileaction {

PreviewContentType detectPreviewType(const QString &filename)
{
    QString lower = filename.toLower();

    // Disk image formats
    if (lower.endsWith(".d64") || lower.endsWith(".d71") || lower.endsWith(".d81") ||
        lower.endsWith(".g64") || lower.endsWith(".g71")) {
        return PreviewContentType::DiskImage;
    }

    // SID music formats
    if (lower.endsWith(".sid") || lower.endsWith(".psid") || lower.endsWith(".rsid")) {
        return PreviewContentType::SidMusic;
    }

    // HTML formats
    if (lower.endsWith(".html") || lower.endsWith(".htm")) {
        return PreviewContentType::Html;
    }

    // Plain text / config formats
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
