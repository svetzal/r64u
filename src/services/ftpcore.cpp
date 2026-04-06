/**
 * @file ftpcore.cpp
 * @brief Implementation of pure FTP protocol parsing functions.
 */

#include "ftpcore.h"

#include <QRegularExpression>

namespace ftp {

std::optional<PassiveResult> parsePassiveResponse(const QString &text)
{
    // Parse response like: 227 Entering Passive Mode (h1,h2,h3,h4,p1,p2)
    QRegularExpression rx("\\((\\d+),(\\d+),(\\d+),(\\d+),(\\d+),(\\d+)\\)");
    auto match = rx.match(text);

    if (!match.hasMatch()) {
        return std::nullopt;
    }

    PassiveResult result;
    result.host = QString("%1.%2.%3.%4")
                      .arg(match.captured(1))
                      .arg(match.captured(2))
                      .arg(match.captured(3))
                      .arg(match.captured(4));

    int p1 = match.captured(5).toInt();
    int p2 = match.captured(6).toInt();
    result.port = static_cast<quint16>((p1 * PassivePortMultiplier) + p2);

    return result;
}

QList<FtpEntry> parseDirectoryListing(const QByteArray &data)
{
    QList<FtpEntry> entries;
    QString listing = QString::fromUtf8(data);
    QStringList lines = listing.split("\r\n", Qt::SkipEmptyParts);

    // Unix-style listing: drwxr-xr-x 2 user group 4096 Jan 1 12:00 dirname
    // Or simple listing: filename
    QRegularExpression unixRx("^([d\\-])([rwx\\-]{9})\\s+\\d+\\s+\\S+\\s+\\S+\\s+(\\d+)\\s+"
                              "(\\w+\\s+\\d+\\s+[\\d:]+)\\s+(.+)$");

    for (const QString &line : lines) {
        if (line.isEmpty()) {
            continue;
        }

        FtpEntry entry;
        auto match = unixRx.match(line);

        if (match.hasMatch()) {
            entry.isDirectory = (match.captured(1) == "d");
            entry.permissions = match.captured(2);
            entry.size = match.captured(3).toLongLong();
            entry.name = match.captured(5);
        } else {
            // Simple listing - just filename
            entry.name = line.trimmed();
            entry.isDirectory = false;  // Can't tell from simple listing
        }

        if (!entry.name.isEmpty() && entry.name != "." && entry.name != "..") {
            entries.append(entry);
        }
    }

    return entries;
}

std::optional<QString> parsePwdResponse(const QString &text)
{
    QRegularExpression rx("\"(.*)\"");
    auto match = rx.match(text);
    if (!match.hasMatch()) {
        return std::nullopt;
    }
    return match.captured(1);
}

FtpResponseParse splitResponseLines(const QString &buffer)
{
    static constexpr int FtpReplyCodeLength = 3;
    static constexpr int FtpReplyTextOffset = 4;
    static constexpr int CrLfLength = 2;

    FtpResponseParse result;
    QString remaining = buffer;

    while (remaining.contains("\r\n")) {
        int idx = remaining.indexOf("\r\n");
        QString line = remaining.left(idx);
        remaining = remaining.mid(idx + CrLfLength);

        if (line.length() < FtpReplyCodeLength) {
            continue;
        }

        bool ok = false;
        int code = line.left(FtpReplyCodeLength).toInt(&ok);
        if (!ok) {
            continue;
        }

        // Multi-line continuation: 4th char is '-', skip this line
        if (line.length() > FtpReplyCodeLength && line.at(FtpReplyCodeLength) == '-') {
            continue;
        }

        FtpResponseLine responseLine;
        responseLine.code = code;
        responseLine.text =
            line.length() >= FtpReplyTextOffset ? line.mid(FtpReplyTextOffset) : QString();
        result.lines.append(responseLine);
    }

    result.remainingBuffer = remaining;
    return result;
}

}  // namespace ftp
