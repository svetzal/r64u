/**
 * @file ftpcore.h
 * @brief Pure core functions for FTP protocol parsing.
 *
 * All functions in this namespace are pure: they take immutable input and return
 * new output with no side effects. This enables comprehensive unit testing and
 * clean separation from I/O concerns (QTcpSocket, connection state).
 *
 * This follows the same pattern as playlistcore.h / favoritescore.h.
 */

#ifndef FTPCORE_H
#define FTPCORE_H

#include "ftpentry.h"

#include <QByteArray>
#include <QList>
#include <QString>

#include <optional>

namespace ftp {

/// Multiplier used when decoding the passive mode port from two bytes.
constexpr int PassivePortMultiplier = 256;

/**
 * @brief Parsed host and port from a PASV response.
 */
struct PassiveResult
{
    QString host;  ///< Dotted-decimal host extracted from the PASV response
    quint16 port;  ///< Port number decoded from the two port bytes
};

// ---------------------------------------------------------------------------
// Protocol parsing
// ---------------------------------------------------------------------------

/**
 * @brief Parses the host and port from a PASV response line.
 *
 * Expected format: ... (h1,h2,h3,h4,p1,p2) ...
 * The port is calculated as p1 * 256 + p2.
 *
 * @param text The PASV response text (e.g. "Entering Passive Mode (192,168,1,64,4,0)").
 * @return PassiveResult on success, or std::nullopt if the text does not match.
 */
[[nodiscard]] std::optional<PassiveResult> parsePassiveResponse(const QString &text);

/**
 * @brief Parses an FTP directory listing into a list of FtpEntry values.
 *
 * Supports two listing formats:
 * - Unix-style: @c drwxr-xr-x 2 user group 4096 Jan 1 12:00 dirname
 * - Simple: plain filename, one per line
 *
 * Entries named @c "." and @c ".." are filtered out automatically.
 *
 * @param data Raw bytes received from the FTP data connection.
 * @return List of parsed entries (may be empty).
 */
[[nodiscard]] QList<FtpEntry> parseDirectoryListing(const QByteArray &data);

}  // namespace ftp

#endif  // FTPCORE_H
