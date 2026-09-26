#ifndef DROPCORE_H
#define DROPCORE_H

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

/**
 * @brief Pure decisions behind drag and drop between the file panes.
 *
 * The widgets translate a drop event into a Source, the dropped entries and a
 * DropTarget; this namespace decides what, if anything, the drop means. It
 * also owns the wire format of the remote-paths MIME payload so the model that
 * writes it and the views that read it agree on it.
 */
namespace dropcore {

/// The MIME type carrying remote (device) paths between views of this app.
inline constexpr const char *kRemotePathsMimeType = "application/x-r64u-remote-paths";

/// One of the two file panes in Transfer mode.
enum class Pane { Local, Remote };

/// Where a drag started.
enum class Source {
    LocalPane,   ///< The local pane of Transfer mode
    RemotePane,  ///< A view of the device listing (Transfer or Explore)
    External     ///< Another application, e.g. the Finder
};

/// One dragged file or directory.
struct DropEntry
{
    QString path;
    bool isDirectory = false;
    qint64 size = 0;  ///< Bytes when known, 0 for a directory or an unknown size
};

/// Where a drop landed.
struct DropTarget
{
    Pane pane = Pane::Local;
    QString currentDirectory;     ///< The pane's directory, the fallback destination
    bool onDirectoryRow = false;  ///< The cursor was over a directory row
    QString rowPath;              ///< That directory's path when onDirectoryRow
};

/// What a drop asks the transfer queue to do.
struct DropPlan
{
    enum class Kind { Upload, Download };
    Kind kind = Kind::Upload;
    QString destinationDirectory;
    QList<DropEntry> entries;
};

/**
 * @brief Whether a drag from @p source may be dropped on @p pane at all.
 *
 * Local and external files may be dropped on the remote pane (upload); remote
 * files on the local pane (download). A pane never accepts its own files and
 * the local pane never accepts external ones: neither would be a transfer,
 * and moving the user's own files behind their back is not on offer.
 */
[[nodiscard]] bool canAccept(Source source, Pane pane);

/**
 * @brief Classifies a drag by what it carries and where it came from.
 * @param fromThisView The drag started in the view it is now over.
 * @param thisPane The pane that view shows.
 * @param hasRemotePaths The payload carries kRemotePathsMimeType.
 * @param hasUrls The payload carries file URLs (text/uri-list).
 * @return The source, or nullopt when the payload is not one this app reads.
 */
[[nodiscard]] std::optional<Source> classifySource(bool fromThisView, Pane thisPane,
                                                   bool hasRemotePaths, bool hasUrls);

/**
 * @brief Plans the transfer a drop asks for.
 * @return The plan, or nullopt when the drop means nothing (see canAccept) or
 *         @p entries is empty. The destination is the directory row under the
 *         cursor when there is one, otherwise the pane's current directory.
 */
[[nodiscard]] std::optional<DropPlan> planDrop(Source source, const QList<DropEntry> &entries,
                                               const DropTarget &target);

/**
 * @brief Encodes remote entries for kRemotePathsMimeType.
 *
 * One entry per line: a `d` or `f` kind letter, the size and the full path,
 * tab-separated.
 */
[[nodiscard]] QByteArray encodeRemoteEntries(const QList<DropEntry> &entries);

/// Decodes a payload written by encodeRemoteEntries(); malformed lines are skipped.
[[nodiscard]] QList<DropEntry> decodeRemoteEntries(const QByteArray &payload);

/**
 * @brief The row an item ends up on after being dragged to an insertion point.
 * @param fromRow The row the item is dragged from.
 * @param insertionRow The gap it is dropped in, counted in the list as it is
 *        before the move (0 = before the first row, count = after the last).
 * @return The item's final row, i.e. the @c to of a move(from, to).
 */
[[nodiscard]] int reorderDestination(int fromRow, int insertionRow);

}  // namespace dropcore

#endif  // DROPCORE_H
