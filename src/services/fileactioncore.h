/**
 * @file fileactioncore.h
 * @brief Pure core functions for file preview type detection and routing.
 *
 * All functions in this namespace are pure: they take immutable input and return
 * results with no side effects. This enables comprehensive unit testing and
 * clean separation from UI and service concerns.
 *
 * ## Responsibilities
 *
 * - Classify files by their preview content type (disk image, SID, HTML, text)
 * - Route received preview data to the correct display action
 *
 * ## Architecture
 *
 * - **Pure core** (this file): type classification, routing decisions
 * - **Imperative shell** (FileDetailsPanel, ExplorePanel): widget updates, service calls
 */

#ifndef FILEACTIONCORE_H
#define FILEACTIONCORE_H

#include <QByteArray>
#include <QString>

#include <variant>

namespace fileaction {

// ---------------------------------------------------------------------------
// Preview content type classification
// ---------------------------------------------------------------------------

/**
 * @brief Classifies how a file's content should be displayed in the details panel.
 *
 * Used to determine both whether to fetch file content from the device and
 * how to render it once received.
 */
enum class PreviewContentType {
    DiskImage,  ///< Disk image file — render as C64 directory listing
    SidMusic,   ///< SID music file — render as metadata with HVSC/GameBase64 info
    Html,       ///< HTML file — render in HTML browser widget
    Text,       ///< Plain text / config file — render as plain text
    Unknown     ///< Binary or unrecognized file — show file info only, no content fetch
};

/**
 * @brief Detect how a file should be previewed based on its filename extension.
 *
 * Extension matching is case-insensitive. The mapping is:
 * - .d64, .d71, .d81, .g64, .g71 → DiskImage
 * - .sid, .psid, .rsid → SidMusic
 * - .html, .htm → Html
 * - .cfg, .txt, .log, .ini, .md, .json, .xml → Text
 * - Anything else → Unknown
 *
 * @param filename Filename (with extension) to classify.
 * @return The preview content type.
 */
[[nodiscard]] PreviewContentType detectPreviewType(const QString &filename);

/**
 * @brief Returns true if the content type should trigger a content fetch from the device.
 *
 * DiskImage, SidMusic, Html, and Text all require content to display;
 * Unknown files show only file metadata (size, type) without fetching content.
 *
 * @param type The preview content type.
 * @return True if file content should be requested.
 */
[[nodiscard]] bool requiresContentFetch(PreviewContentType type);

// ---------------------------------------------------------------------------
// Preview data routing
// ---------------------------------------------------------------------------

/**
 * @brief Action result: display received data as a C64 disk directory listing.
 */
struct ShowDiskDirectory
{
    QByteArray data;  ///< Raw disk image bytes
    QString path;     ///< Remote path of the file (used for filename display)
};

/**
 * @brief Action result: display received data as SID file metadata.
 */
struct ShowSidDetails
{
    QByteArray data;      ///< Raw SID file bytes
    QString path;         ///< Remote path of the file
    bool updatePlaylist;  ///< True if playlist manager is available for duration update
};

/**
 * @brief Action result: display received data as text (plain or HTML).
 */
struct ShowTextContent
{
    QString content;  ///< UTF-8 decoded text content
};

/**
 * @brief Union of all possible preview display actions.
 *
 * The imperative shell (ExplorePanel) uses std::visit to dispatch to the
 * appropriate FileDetailsPanel method.
 */
using PreviewAction = std::variant<ShowDiskDirectory, ShowSidDetails, ShowTextContent>;

/**
 * @brief Route received preview data to the appropriate display action.
 *
 * Determines what to do with file content received from the preview service,
 * based on file type and application state. This is the canonical place for
 * the "what do we do with this file's bytes?" decision.
 *
 * @param remotePath Remote path of the file that was previewed.
 * @param data Raw file content bytes.
 * @param hasPlaylistManager True if a PlaylistManager is available for SID duration updates.
 * @return The action the shell should take to display the content.
 */
[[nodiscard]] PreviewAction routePreviewData(const QString &remotePath, const QByteArray &data,
                                             bool hasPlaylistManager);

}  // namespace fileaction

#endif  // FILEACTIONCORE_H
