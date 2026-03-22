/**
 * @file songlengthsparser.cpp
 * @brief Pure parsing functions for the HVSC Songlengths.md5 database.
 */

#include "songlengthsparser.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QTextStream>

namespace songlengths {

ParsedDatabase parseDatabase(const QByteArray &data)
{
    ParsedDatabase result;

    QTextStream stream(data);
    stream.setEncoding(QStringConverter::Encoding::Latin1);

    static const QRegularExpression entryRegex(R"(^([0-9a-fA-F]{32})=(.+)$)");
    static const QRegularExpression pathCommentRegex(R"(^; (/[^\s]+\.sid)$)",
                                                     QRegularExpression::CaseInsensitiveOption);

    QString currentPath;

    while (!stream.atEnd()) {
        QString line = stream.readLine().trimmed();

        if (line.isEmpty() || line.startsWith('[')) {
            continue;
        }

        if (line.startsWith(';')) {
            QRegularExpressionMatch pathMatch = pathCommentRegex.match(line);
            if (pathMatch.hasMatch()) {
                currentPath = pathMatch.captured(1);
            }
            continue;
        }

        QRegularExpressionMatch match = entryRegex.match(line);
        if (match.hasMatch()) {
            QString md5 = match.captured(1).toLower();
            QString timesStr = match.captured(2);

            QList<int> durs = parseTimeList(timesStr);
            if (!durs.isEmpty()) {
                result.durations.insert(md5, durs);

                if (!currentPath.isEmpty()) {
                    result.md5ToPath.insert(md5, currentPath);
                }

                QList<QString> formatted;
                QStringList parts = timesStr.split(' ', Qt::SkipEmptyParts);
                for (const QString &part : parts) {
                    QString clean = part;
                    int dotPos = clean.indexOf('.');
                    if (dotPos >= 0) {
                        clean = clean.left(dotPos);
                    }
                    formatted.append(clean);
                }
                result.formattedTimes.insert(md5, formatted);
            }
        }
    }

    return result;
}

QList<int> parseTimeList(const QString &timeStr)
{
    QList<int> durations;
    QStringList parts = timeStr.split(' ', Qt::SkipEmptyParts);

    for (const QString &part : parts) {
        int seconds = parseTime(part);
        if (seconds > 0) {
            durations.append(seconds);
        }
    }

    return durations;
}

int parseTime(const QString &time)
{
    static const QRegularExpression timeRegex(R"(^(\d+):(\d{2})(?:\.(\d{1,3}))?$)");

    QRegularExpressionMatch match = timeRegex.match(time);
    if (!match.hasMatch()) {
        return 0;
    }

    int minutes = match.captured(1).toInt();
    int seconds = match.captured(2).toInt();

    return (minutes * 60) + seconds;
}

}  // namespace songlengths
