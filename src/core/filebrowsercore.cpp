/**
 * @file filebrowsercore.cpp
 * @brief Implementation of pure file browser action routing functions.
 */

#include "filebrowsercore.h"

#include <QFileInfo>

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

QString buildDeleteConfirmMessage(const QStringList &paths, bool singleIsDirectory,
                                  const QString &verbPhrase)
{
    if (paths.size() == 1) {
        QString name = QFileInfo(paths.first()).fileName();
        QString type = singleIsDirectory ? QStringLiteral("folder") : QStringLiteral("file");
        return QStringLiteral("Are you sure you want to %1 the %2 '%3'?")
            .arg(verbPhrase, type, name);
    }
    return QStringLiteral("Are you sure you want to %1 %2 items?")
        .arg(verbPhrase)
        .arg(paths.size());
}

}  // namespace filebrowser
