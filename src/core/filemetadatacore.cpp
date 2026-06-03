/**
 * @file filemetadatacore.cpp
 * @brief Implementation of pure file metadata formatting functions.
 */

#include "filemetadatacore.h"

#include <QCoreApplication>

namespace filemetadata {

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

namespace {

QString sectionSeparator()
{
    return QCoreApplication::translate("filemetadata", "─────────────────────────────────\n");
}

QString wideSectionSeparator()
{
    return QCoreApplication::translate("filemetadata",
                                       "════════════════════════════════════════\n");
}

QString formatSonglengthsSection(const SidDisplayContext &ctx)
{
    QString result;

    if (!ctx.songlengthsServicePresent) {
        // Service not wired up at all — diagnostic only
        return result;
    }

    if (!ctx.songlengthsDatabaseLoaded) {
        result += "\n";
        result += sectionSeparator();
        result += QCoreApplication::translate("filemetadata", "HVSC Database: Not loaded\n");
        return result;
    }

    result += "\n";
    result += sectionSeparator();

    if (ctx.songLengths.found) {
        result += QCoreApplication::translate("filemetadata", "HVSC Database: Found\n");
        result += QCoreApplication::translate("filemetadata", "Song Lengths:\n");
        for (int i = 0; i < ctx.songLengths.formattedTimes.size(); ++i) {
            result += QCoreApplication::translate("filemetadata", "  Song %1: %2\n")
                          .arg(i + 1)
                          .arg(ctx.songLengths.formattedTimes.at(i));
        }
    } else {
        result += QCoreApplication::translate("filemetadata", "HVSC Database: Not found\n");
        result += QCoreApplication::translate("filemetadata", "(Using default 3:00 duration)\n");
    }

    return result;
}

QString formatBugSection(const SidDisplayContext &ctx)
{
    if (!ctx.buglistLoaded) {
        return {};
    }
    if (!ctx.bugInfo.found || ctx.bugInfo.entries.isEmpty()) {
        return {};
    }

    QString result;
    result += "\n";
    result += sectionSeparator();
    result += QCoreApplication::translate("filemetadata", "⚠ KNOWN ISSUES:\n");

    for (const HVSCMetadataService::BugEntry &bug : ctx.bugInfo.entries) {
        if (bug.subtune > 0) {
            result += QCoreApplication::translate("filemetadata", "  Song #%1: %2\n")
                          .arg(bug.subtune)
                          .arg(bug.description);
        } else {
            result += QCoreApplication::translate("filemetadata", "  %1\n").arg(bug.description);
        }
    }

    return result;
}

QString formatStilSection(const SidDisplayContext &ctx)
{
    if (!ctx.stilLoaded) {
        return {};
    }
    if (!ctx.stilInfo.found || ctx.stilInfo.entries.isEmpty()) {
        return {};
    }

    QString result;
    result += "\n";
    result += sectionSeparator();
    result += QCoreApplication::translate("filemetadata", "STIL INFORMATION:\n");

    for (const HVSCMetadataService::SubtuneEntry &entry : ctx.stilInfo.entries) {
        if (entry.subtune > 0) {
            result +=
                QCoreApplication::translate("filemetadata", "\n  Song #%1:\n").arg(entry.subtune);
        }

        if (!entry.name.isEmpty()) {
            result += QCoreApplication::translate("filemetadata", "  Name: %1\n").arg(entry.name);
        }

        if (!entry.author.isEmpty()) {
            result +=
                QCoreApplication::translate("filemetadata", "  Author: %1\n").arg(entry.author);
        }

        for (const HVSCMetadataService::CoverInfo &cover : entry.covers) {
            QString coverLine =
                QCoreApplication::translate("filemetadata", "  Cover: %1").arg(cover.title);
            if (!cover.artist.isEmpty()) {
                coverLine +=
                    QCoreApplication::translate("filemetadata", " by %1").arg(cover.artist);
            }
            if (!cover.timestamp.isEmpty()) {
                coverLine +=
                    QCoreApplication::translate("filemetadata", " (%1)").arg(cover.timestamp);
            }
            result += coverLine + "\n";
        }

        if (!entry.comment.isEmpty()) {
            if (entry.subtune > 0 || !entry.name.isEmpty()) {
                result += QCoreApplication::translate("filemetadata", "  Comment: %1\n")
                              .arg(entry.comment);
            } else {
                result += QCoreApplication::translate("filemetadata", "  %1\n").arg(entry.comment);
            }
        }
    }

    return result;
}

QString formatGameBase64SidSection(const SidDisplayContext &ctx)
{
    if (!ctx.gameBase64ServicePresent) {
        return QCoreApplication::translate("filemetadata",
                                           "\n\n(GameBase64 service not available)\n");
    }
    if (!ctx.gameBase64DatabaseLoaded) {
        return QCoreApplication::translate(
            "filemetadata", "\n\n(Download GameBase64 in Preferences for game info)\n");
    }
    if (!ctx.gameInfo.found) {
        return {};
    }

    QString result;
    result += "\n";
    result += sectionSeparator();
    result += QCoreApplication::translate("filemetadata", "GAME INFO (GameBase64):\n");
    result += QCoreApplication::translate("filemetadata", "  Game: %1\n").arg(ctx.gameInfo.name);

    if (ctx.gameInfo.year > 0) {
        result +=
            QCoreApplication::translate("filemetadata", "  Year: %1\n").arg(ctx.gameInfo.year);
    }
    if (!ctx.gameInfo.publisher.isEmpty()) {
        result += QCoreApplication::translate("filemetadata", "  Publisher: %1\n")
                      .arg(ctx.gameInfo.publisher);
    }
    if (!ctx.gameInfo.genre.isEmpty()) {
        QString genre = ctx.gameInfo.genre;
        if (!ctx.gameInfo.parentGenre.isEmpty() && ctx.gameInfo.parentGenre != ctx.gameInfo.genre) {
            genre = ctx.gameInfo.parentGenre + " / " + ctx.gameInfo.genre;
        }
        result += QCoreApplication::translate("filemetadata", "  Genre: %1\n").arg(genre);
    }

    return result;
}

QString formatGameBase64DiskSection(const DiskDisplayContext &ctx)
{
    if (!ctx.gameBase64ServicePresent) {
        return QCoreApplication::translate("filemetadata",
                                           "\n\n(GameBase64 service not available)\n");
    }
    if (!ctx.gameBase64DatabaseLoaded) {
        return QCoreApplication::translate(
            "filemetadata", "\n\n(Download GameBase64 in Preferences for game info)\n");
    }
    if (!ctx.gameInfo.found) {
        return {};
    }

    QString result;
    result += "\n\n";
    result += wideSectionSeparator();
    result += QCoreApplication::translate("filemetadata", "GAMEBASE64 INFO:\n");
    result +=
        QCoreApplication::translate("filemetadata", "────────────────────────────────────────\n");
    result += QCoreApplication::translate("filemetadata", "  Game: %1\n").arg(ctx.gameInfo.name);

    if (ctx.gameInfo.year > 0) {
        result +=
            QCoreApplication::translate("filemetadata", "  Year: %1\n").arg(ctx.gameInfo.year);
    }
    if (!ctx.gameInfo.publisher.isEmpty()) {
        result += QCoreApplication::translate("filemetadata", "  Publisher: %1\n")
                      .arg(ctx.gameInfo.publisher);
    }
    if (!ctx.gameInfo.genre.isEmpty()) {
        QString genre = ctx.gameInfo.genre;
        if (!ctx.gameInfo.parentGenre.isEmpty() && ctx.gameInfo.parentGenre != ctx.gameInfo.genre) {
            genre = ctx.gameInfo.parentGenre + " / " + ctx.gameInfo.genre;
        }
        result += QCoreApplication::translate("filemetadata", "  Genre: %1\n").arg(genre);
    }
    if (!ctx.gameInfo.musician.isEmpty()) {
        QString musician = ctx.gameInfo.musician;
        if (!ctx.gameInfo.musicianGroup.isEmpty()) {
            musician += " (" + ctx.gameInfo.musicianGroup + ")";
        }
        result += QCoreApplication::translate("filemetadata", "  Musician: %1\n").arg(musician);
    }
    if (ctx.gameInfo.rating > 0) {
        result += QCoreApplication::translate("filemetadata", "  Rating: %1/10\n")
                      .arg(ctx.gameInfo.rating);
    }
    if (ctx.gameInfo.playersFrom > 0) {
        if (ctx.gameInfo.playersTo > ctx.gameInfo.playersFrom) {
            result += QCoreApplication::translate("filemetadata", "  Players: %1-%2\n")
                          .arg(ctx.gameInfo.playersFrom)
                          .arg(ctx.gameInfo.playersTo);
        } else {
            result += QCoreApplication::translate("filemetadata", "  Players: %1\n")
                          .arg(ctx.gameInfo.playersFrom);
        }
    }
    if (!ctx.gameInfo.memo.isEmpty()) {
        result += QCoreApplication::translate("filemetadata", "\n  %1\n").arg(ctx.gameInfo.memo);
    }

    return result;
}

}  // namespace

// ---------------------------------------------------------------------------
// Public API
// ---------------------------------------------------------------------------

QString formatSidDetails(const SidDisplayContext &ctx)
{
    QString details = SidFileParser::formatForDisplay(ctx.sidInfo);
    details += formatSonglengthsSection(ctx);
    details += formatBugSection(ctx);
    details += formatStilSection(ctx);
    details += formatGameBase64SidSection(ctx);
    return details;
}

QString formatDiskDetails(const DiskDisplayContext &ctx)
{
    QString details = ctx.directoryListing;
    details += formatGameBase64DiskSection(ctx);
    return details;
}

}  // namespace filemetadata
