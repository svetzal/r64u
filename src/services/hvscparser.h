/**
 * @file hvscparser.h
 * @brief Pure parsing functions for HVSC STIL and BUGlist databases.
 *
 * These functions are free of I/O dependencies, taking raw data as input
 * and returning parsed results. They can be tested directly without any
 * network or filesystem setup.
 */

#ifndef HVSCPARSER_H
#define HVSCPARSER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>
#include <QStringList>

namespace hvsc {

/**
 * @brief Information about a cover/sample in a tune.
 */
struct CoverInfo
{
    QString title;      ///< Original song title
    QString artist;     ///< Original artist
    QString timestamp;  ///< Optional timestamp (e.g., "1:05" or "1:05-2:30")
};

/**
 * @brief STIL entry for a single subtune.
 */
struct SubtuneEntry
{
    int subtune = 0;          ///< Subtune number (0 = whole file)
    QString name;             ///< Tune name
    QString author;           ///< Tune author (if different from SID header)
    QString comment;          ///< Commentary/history
    QList<CoverInfo> covers;  ///< Cover/sample information
};

/**
 * @brief Complete STIL information for a SID file.
 */
struct StilInfo
{
    bool found = false;           ///< True if entry exists in STIL
    QString path;                 ///< HVSC path
    QList<SubtuneEntry> entries;  ///< Entries for file and/or subtunes
};

/**
 * @brief Bug report for a SID file.
 */
struct BugEntry
{
    int subtune = 0;      ///< Subtune number (0 = whole file)
    QString description;  ///< Bug description
};

/**
 * @brief Complete bug information for a SID file.
 */
struct BugInfo
{
    bool found = false;       ///< True if entry exists in BUGlist
    QString path;             ///< HVSC path
    QList<BugEntry> entries;  ///< Bug entries for file and/or subtunes
};

/**
 * @brief Parses STIL.txt data into a database.
 * @param data Raw STIL.txt bytes (Latin1 encoded).
 * @return Hash mapping HVSC paths to their STIL entries.
 */
[[nodiscard]] QHash<QString, QList<SubtuneEntry>> parseStilData(const QByteArray &data);

/**
 * @brief Parses BUGlist.txt data into a database.
 * @param data Raw BUGlist.txt bytes (Latin1 encoded).
 * @return Hash mapping HVSC paths to their bug entries.
 */
[[nodiscard]] QHash<QString, QList<BugEntry>> parseBuglistData(const QByteArray &data);

/**
 * @brief Parses a single STIL entry block into subtune entries.
 * @param lines Lines from a single file's entry block.
 * @return List of subtune entries.
 */
[[nodiscard]] QList<SubtuneEntry> parseStilEntry(const QStringList &lines);

/**
 * @brief Parses a single BUGlist entry block into bug entries.
 * @param lines Lines from a single file's entry block.
 * @return List of bug entries.
 */
[[nodiscard]] QList<BugEntry> parseBugEntry(const QStringList &lines);

/**
 * @brief Looks up STIL info for a given HVSC path.
 * @param database The parsed STIL database.
 * @param hvscPath HVSC path (normalizes slashes, adds leading / if missing).
 * @return STIL information for the path.
 */
[[nodiscard]] StilInfo lookupStil(const QHash<QString, QList<SubtuneEntry>> &database,
                                  const QString &hvscPath);

/**
 * @brief Looks up bug info for a given HVSC path.
 * @param database The parsed BUGlist database.
 * @param hvscPath HVSC path (normalizes slashes, adds leading / if missing).
 * @return Bug information for the path.
 */
[[nodiscard]] BugInfo lookupBuglist(const QHash<QString, QList<BugEntry>> &database,
                                    const QString &hvscPath);

}  // namespace hvsc

#endif  // HVSCPARSER_H
