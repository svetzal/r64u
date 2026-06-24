#ifndef FILEBROWSERCORE_H
#define FILEBROWSERCORE_H

#include "core/filetypecore.h"

#include <QList>
#include <QString>
#include <QStringList>

namespace filebrowser {

/**
 * @brief Result of resolving a double-click action.
 */
enum class DoubleClickAction {
    Navigate,    ///< Navigate into directory
    Play,        ///< Play music file
    Run,         ///< Run program or cartridge
    Mount,       ///< Mount disk image
    LoadConfig,  ///< Load config file
    None         ///< No action
};

/**
 * @brief Resolves the action for a double-click on a file browser item.
 * @param type File type of the clicked item.
 * @param isDirectory True if the item is a directory.
 * @return The action to perform.
 */
[[nodiscard]] DoubleClickAction resolveDoubleClickAction(filetype::FileType type, bool isDirectory);

/**
 * @brief Entry for a filtered playlist candidate.
 */
struct PlaylistCandidate
{
    QString path;
};

/**
 * @brief Filters a list of (path, fileType) pairs to SID music only.
 * @param items Pairs of {path, fileType}.
 * @return List of SID music paths.
 */
[[nodiscard]] QList<PlaylistCandidate>
filterPlaylistCandidates(const QList<QPair<QString, filetype::FileType>> &items);

/**
 * @brief Builds a user-facing confirmation message for a delete operation.
 *
 * Handles single-vs-multi-item branching using the provided verb phrase.
 *
 * @param paths List of paths to delete.
 * @param singleIsDirectory True if the single selection is a directory.
 * @param verbPhrase Verb phrase describing the action (e.g. "delete").
 * @return The confirmation message string.
 */
[[nodiscard]] QString buildDeleteConfirmMessage(const QStringList &paths, bool singleIsDirectory,
                                               const QString &verbPhrase);

}  // namespace filebrowser

#endif  // FILEBROWSERCORE_H
