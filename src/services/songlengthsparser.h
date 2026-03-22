/**
 * @file songlengthsparser.h
 * @brief Pure parsing functions for the HVSC Songlengths.md5 database.
 *
 * These functions are free of I/O dependencies, taking raw data as input
 * and returning parsed results. They can be tested directly without any
 * network or filesystem setup.
 */

#ifndef SONGLENGTHSPARSER_H
#define SONGLENGTHSPARSER_H

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QString>

namespace songlengths {

/**
 * @brief The fully-parsed Songlengths database.
 */
struct ParsedDatabase
{
    QHash<QString, QList<int>> durations;           ///< MD5 -> durations in seconds
    QHash<QString, QList<QString>> formattedTimes;  ///< MD5 -> formatted time strings
    QHash<QString, QString> md5ToPath;              ///< MD5 -> HVSC path
};

/**
 * @brief Parses Songlengths.md5 data.
 * @param data Raw database bytes.
 * @return Parsed database.
 */
[[nodiscard]] ParsedDatabase parseDatabase(const QByteArray &data);

/**
 * @brief Parses a space-separated list of time strings.
 * @param timeStr Space-separated list like "3:57 2:23 4:01".
 * @return List of durations in seconds.
 */
[[nodiscard]] QList<int> parseTimeList(const QString &timeStr);

/**
 * @brief Parses a single time string in mm:ss or mm:ss.SSS format.
 * @param time Time string.
 * @return Duration in seconds, or 0 if invalid.
 */
[[nodiscard]] int parseTime(const QString &time);

}  // namespace songlengths

#endif  // SONGLENGTHSPARSER_H
