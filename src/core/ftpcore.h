#ifndef FTPCORE_H
#define FTPCORE_H

#include "ftp/ftpcommandqueue.h"
#include "ftp/ftpentry.h"

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

/**
 * @brief A single parsed FTP response line.
 */
struct FtpResponseLine
{
    int code = 0;  ///< Three-digit FTP response code
    QString text;  ///< Response text following the code
};

/**
 * @brief Result of splitting a raw FTP control-channel buffer into response lines.
 */
struct FtpResponseParse
{
    QList<FtpResponseLine> lines;  ///< Complete, non-continuation response lines
    QString remainingBuffer;       ///< Bytes that did not form a complete line
};

// ---------------------------------------------------------------------------
// Command wire-formatting
// ---------------------------------------------------------------------------

/**
 * @brief Formats the wire string for an FTP command.
 *
 * Maps an FtpCommandQueue::Command to the corresponding FTP wire command
 * string.  For USER and PASS the supplied @p user / @p password arguments
 * are interpolated.  For commands that carry a remote-path argument the
 * @p arg string is appended.
 *
 * Returns an empty string for Command::None and any unrecognised command
 * value — callers should treat an empty result as a no-op.
 *
 * @param cmd      The command type to format.
 * @param arg      The primary argument (remote path, type string, etc.).
 * @param user     The FTP user name, used only for the USER command.
 * @param password The FTP password, used only for the PASS command.
 * @return The formatted FTP wire string (without the trailing CRLF).
 */
[[nodiscard]] QString formatCommand(FtpCommandQueue::Command cmd, const QString &arg,
                                    const QString &user, const QString &password);

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

/**
 * @brief Extracts the quoted path from a 257 PWD response text.
 *
 * Expects text of the form @c "\"/path/to/dir\" is current directory".
 * Extracts the first double-quoted substring.
 *
 * @param text The response text portion of a 257 reply (everything after the code).
 * @return The extracted path, or std::nullopt if no quoted path is found.
 */
[[nodiscard]] std::optional<QString> parsePwdResponse(const QString &text);

/**
 * @brief Splits a raw FTP control-channel buffer into complete response lines.
 *
 * Lines are delimited by @c \\r\\n.  Multi-line continuation lines (where the
 * fourth character is @c '-') are consumed but not returned.  Any trailing
 * bytes that do not yet form a complete line are returned in
 * FtpResponseParse::remainingBuffer.
 *
 * @param buffer Accumulated bytes from the control socket (may be partial).
 * @return Parsed lines and the unconsumed remainder of the buffer.
 */
[[nodiscard]] FtpResponseParse splitResponseLines(const QString &buffer);

// ---------------------------------------------------------------------------
// Transfer command-sequence builders
// ---------------------------------------------------------------------------

/**
 * @brief A command enum value paired with its argument string.
 */
struct CommandSpec
{
    FtpCommandQueue::Command cmd = FtpCommandQueue::Command::None;  ///< The FTP command to execute
    QString arg;  ///< Argument for the command (may be empty)
};

/**
 * @brief Builds the complete command sequence for an ASCII directory listing.
 *
 * Returns: TYPE A, PASV, LIST @p path.
 *
 * @param path Remote directory path to list.
 * @return Ordered list of commands the client should enqueue.
 */
[[nodiscard]] QList<CommandSpec> buildListSequence(const QString &path);

/**
 * @brief Builds the binary-mode transfer prelude commands for a download.
 *
 * Returns: TYPE I, PASV.
 * The caller must still enqueue the RETR command with its file handle.
 *
 * @return Ordered list of prelude commands to enqueue before RETR.
 */
[[nodiscard]] QList<CommandSpec> buildDownloadPrelude();

/**
 * @brief Builds the binary-mode transfer prelude commands for an upload.
 *
 * Returns: TYPE I, PASV.
 * The caller must still enqueue the STOR command with its file handle.
 *
 * @return Ordered list of prelude commands to enqueue before STOR.
 */
[[nodiscard]] QList<CommandSpec> buildUploadPrelude();

}  // namespace ftp

#endif  // FTPCORE_H
