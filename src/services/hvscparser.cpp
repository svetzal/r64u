/**
 * @file hvscparser.cpp
 * @brief Pure parsing functions for HVSC STIL and BUGlist databases.
 */

#include "hvscparser.h"

#include <QByteArray>
#include <QRegularExpression>
#include <QStringList>
#include <QTextStream>

namespace hvsc {

QHash<QString, QList<SubtuneEntry>> parseStilData(const QByteArray &data)
{
    QHash<QString, QList<SubtuneEntry>> result;

    QTextStream stream(data);
    stream.setEncoding(QStringConverter::Encoding::Latin1);

    static const QRegularExpression pathRegex(R"(^(/[^\s]+\.sid)$)",
                                              QRegularExpression::CaseInsensitiveOption);

    QString currentPath;
    QStringList currentBlock;

    while (!stream.atEnd()) {
        QString line = stream.readLine();

        if (line.startsWith("###")) {
            continue;
        }

        QRegularExpressionMatch pathMatch = pathRegex.match(line.trimmed());
        if (pathMatch.hasMatch()) {
            if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
                result.insert(currentPath, parseStilEntry(currentBlock));
            }
            currentPath = pathMatch.captured(1);
            currentBlock.clear();
            continue;
        }

        if (!currentPath.isEmpty() && !line.trimmed().isEmpty()) {
            currentBlock.append(line);
        }
    }

    if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
        result.insert(currentPath, parseStilEntry(currentBlock));
    }

    return result;
}

QHash<QString, QList<BugEntry>> parseBuglistData(const QByteArray &data)
{
    QHash<QString, QList<BugEntry>> result;

    QTextStream stream(data);
    stream.setEncoding(QStringConverter::Encoding::Latin1);

    static const QRegularExpression pathRegex(R"(^(/[^\s]+\.sid)$)",
                                              QRegularExpression::CaseInsensitiveOption);

    QString currentPath;
    QStringList currentBlock;

    while (!stream.atEnd()) {
        QString line = stream.readLine();

        if (line.startsWith("###")) {
            continue;
        }

        QRegularExpressionMatch pathMatch = pathRegex.match(line.trimmed());
        if (pathMatch.hasMatch()) {
            if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
                result.insert(currentPath, parseBugEntry(currentBlock));
            }
            currentPath = pathMatch.captured(1);
            currentBlock.clear();
            continue;
        }

        if (!currentPath.isEmpty() && !line.trimmed().isEmpty()) {
            currentBlock.append(line);
        }
    }

    if (!currentPath.isEmpty() && !currentBlock.isEmpty()) {
        result.insert(currentPath, parseBugEntry(currentBlock));
    }

    return result;
}

QList<SubtuneEntry> parseStilEntry(const QStringList &lines)
{
    QList<SubtuneEntry> entries;
    SubtuneEntry current;

    static const QRegularExpression nameRegex(R"(^   NAME: (.+)$)");
    static const QRegularExpression authorRegex(R"(^ AUTHOR: (.+)$)");
    static const QRegularExpression titleRegex(R"(^  TITLE: (.+)$)");
    static const QRegularExpression artistRegex(R"(^ ARTIST: (.+)$)");
    static const QRegularExpression commentRegex(R"(^COMMENT: (.+)$)");
    static const QRegularExpression subtuneRegex(R"(^\(#(\d+)\)$)");
    static const QRegularExpression continuationRegex(R"(^         (.+)$)");  // 9 spaces

    QString *currentMultiline = nullptr;
    CoverInfo *currentCover = nullptr;

    for (const QString &line : lines) {
        QRegularExpressionMatch subtuneMatch = subtuneRegex.match(line.trimmed());
        if (subtuneMatch.hasMatch()) {
            if (current.subtune > 0 || !current.comment.isEmpty() || !current.name.isEmpty() ||
                !current.covers.isEmpty()) {
                entries.append(current);
            }
            current = SubtuneEntry();
            current.subtune = subtuneMatch.captured(1).toInt();
            currentMultiline = nullptr;
            currentCover = nullptr;
            continue;
        }

        QRegularExpressionMatch nameMatch = nameRegex.match(line);
        if (nameMatch.hasMatch()) {
            current.name = nameMatch.captured(1).trimmed();
            currentMultiline = &current.name;
            currentCover = nullptr;
            continue;
        }

        QRegularExpressionMatch authorMatch = authorRegex.match(line);
        if (authorMatch.hasMatch()) {
            current.author = authorMatch.captured(1).trimmed();
            currentMultiline = &current.author;
            currentCover = nullptr;
            continue;
        }

        QRegularExpressionMatch titleMatch = titleRegex.match(line);
        if (titleMatch.hasMatch()) {
            CoverInfo cover;
            QString titleStr = titleMatch.captured(1).trimmed();

            static const QRegularExpression timestampRegex(R"(\((\d+:\d{2}(?:-\d+:\d{2})?)\)$)");
            QRegularExpressionMatch tsMatch = timestampRegex.match(titleStr);
            if (tsMatch.hasMatch()) {
                cover.timestamp = tsMatch.captured(1);
                titleStr = titleStr.left(tsMatch.capturedStart()).trimmed();
            }

            cover.title = titleStr;
            current.covers.append(cover);
            currentCover = &current.covers.last();
            currentMultiline = &currentCover->title;
            continue;
        }

        QRegularExpressionMatch artistMatch = artistRegex.match(line);
        if (artistMatch.hasMatch()) {
            if (currentCover != nullptr) {
                currentCover->artist = artistMatch.captured(1).trimmed();
                currentMultiline = &currentCover->artist;
            }
            continue;
        }

        QRegularExpressionMatch commentMatch = commentRegex.match(line);
        if (commentMatch.hasMatch()) {
            if (!current.comment.isEmpty()) {
                current.comment += "\n";
            }
            current.comment += commentMatch.captured(1).trimmed();
            currentMultiline = &current.comment;
            currentCover = nullptr;
            continue;
        }

        QRegularExpressionMatch contMatch = continuationRegex.match(line);
        if (contMatch.hasMatch() && currentMultiline != nullptr) {
            *currentMultiline += " " + contMatch.captured(1).trimmed();
            continue;
        }
    }

    if (current.subtune > 0 || !current.comment.isEmpty() || !current.name.isEmpty() ||
        !current.covers.isEmpty()) {
        entries.append(current);
    }

    return entries;
}

QList<BugEntry> parseBugEntry(const QStringList &lines)
{
    QList<BugEntry> entries;
    BugEntry current;

    static const QRegularExpression bugRegex(R"(^BUG: (.+)$)");
    static const QRegularExpression subtuneRegex(R"(^\(#(\d+)\)$)");
    static const QRegularExpression continuationRegex(
        R"(^     (.+)$)");  // 5 spaces for BUG continuation

    QString *currentMultiline = nullptr;

    for (const QString &line : lines) {
        QRegularExpressionMatch subtuneMatch = subtuneRegex.match(line.trimmed());
        if (subtuneMatch.hasMatch()) {
            if (!current.description.isEmpty()) {
                entries.append(current);
            }
            current = BugEntry();
            current.subtune = subtuneMatch.captured(1).toInt();
            currentMultiline = nullptr;
            continue;
        }

        QRegularExpressionMatch bugMatch = bugRegex.match(line);
        if (bugMatch.hasMatch()) {
            if (!current.description.isEmpty()) {
                current.description += "\n";
            }
            current.description += bugMatch.captured(1).trimmed();
            currentMultiline = &current.description;
            continue;
        }

        QRegularExpressionMatch contMatch = continuationRegex.match(line);
        if (contMatch.hasMatch() && currentMultiline != nullptr) {
            *currentMultiline += " " + contMatch.captured(1).trimmed();
            continue;
        }
    }

    if (!current.description.isEmpty()) {
        entries.append(current);
    }

    return entries;
}

StilInfo lookupStil(const QHash<QString, QList<SubtuneEntry>> &database, const QString &hvscPath)
{
    StilInfo result;
    QString path = hvscPath;
    path = path.replace('\\', '/');
    if (!path.startsWith('/')) {
        path = '/' + path;
    }

    if (database.contains(path)) {
        result.found = true;
        result.path = path;
        result.entries = database.value(path);
    }

    return result;
}

BugInfo lookupBuglist(const QHash<QString, QList<BugEntry>> &database, const QString &hvscPath)
{
    BugInfo result;
    QString path = hvscPath;
    path = path.replace('\\', '/');
    if (!path.startsWith('/')) {
        path = '/' + path;
    }

    if (database.contains(path)) {
        result.found = true;
        result.path = path;
        result.entries = database.value(path);
    }

    return result;
}

}  // namespace hvsc
