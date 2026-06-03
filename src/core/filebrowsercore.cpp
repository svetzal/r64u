/**
 * @file filebrowsercore.cpp
 * @brief Implementation of pure file browser action routing functions.
 */

#include "filebrowsercore.h"

namespace filebrowser {

DoubleClickAction resolveDoubleClickAction(filetype::FileType type, bool isDirectory)
{
    if (isDirectory) {
        return DoubleClickAction::Navigate;
    }

    switch (filetype::defaultAction(type)) {
    case filetype::DefaultAction::Play:
        return DoubleClickAction::Play;
    case filetype::DefaultAction::Run:
        return DoubleClickAction::Run;
    case filetype::DefaultAction::Mount:
        return DoubleClickAction::Mount;
    case filetype::DefaultAction::LoadConfig:
        return DoubleClickAction::LoadConfig;
    case filetype::DefaultAction::None:
        return DoubleClickAction::None;
    }

    return DoubleClickAction::None;
}

QList<PlaylistCandidate>
filterPlaylistCandidates(const QList<QPair<QString, filetype::FileType>> &items)
{
    QList<PlaylistCandidate> candidates;
    for (const auto &[path, fileType] : items) {
        if (fileType == filetype::FileType::SidMusic) {
            candidates.append(PlaylistCandidate{path});
        }
    }
    return candidates;
}

}  // namespace filebrowser
