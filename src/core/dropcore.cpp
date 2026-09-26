#include "dropcore.h"

#include <QStringList>

namespace dropcore {

bool canAccept(Source source, Pane pane)
{
    switch (pane) {
    case Pane::Remote:
        return source == Source::LocalPane || source == Source::External;
    case Pane::Local:
        return source == Source::RemotePane;
    }
    return false;
}

std::optional<Source> classifySource(bool fromThisView, Pane thisPane, bool hasRemotePaths,
                                     bool hasUrls)
{
    if (fromThisView) {
        return thisPane == Pane::Local ? Source::LocalPane : Source::RemotePane;
    }
    if (hasRemotePaths) {
        return Source::RemotePane;
    }
    if (hasUrls) {
        // A URL drag from inside the app can only come from the local pane; from
        // outside it is the Finder or another app. Both are uploads, so the
        // difference only matters for the same-pane rule handled above.
        return Source::External;
    }
    return std::nullopt;
}

std::optional<DropPlan> planDrop(Source source, const QList<DropEntry> &entries,
                                 const DropTarget &target)
{
    if (entries.isEmpty() || !canAccept(source, target.pane)) {
        return std::nullopt;
    }

    DropPlan plan;
    plan.kind = target.pane == Pane::Remote ? DropPlan::Kind::Upload : DropPlan::Kind::Download;
    plan.destinationDirectory = target.onDirectoryRow ? target.rowPath : target.currentDirectory;
    plan.entries = entries;
    return plan;
}

QByteArray encodeRemoteEntries(const QList<DropEntry> &entries)
{
    QStringList lines;
    lines.reserve(entries.size());
    for (const DropEntry &entry : entries) {
        lines.append(QStringLiteral("%1\t%2\t%3")
                         .arg(entry.isDirectory ? QLatin1Char('d') : QLatin1Char('f'))
                         .arg(entry.size)
                         .arg(entry.path));
    }
    return lines.join(QLatin1Char('\n')).toUtf8();
}

QList<DropEntry> decodeRemoteEntries(const QByteArray &payload)
{
    QList<DropEntry> entries;
    const QStringList lines = QString::fromUtf8(payload).split(QLatin1Char('\n'));
    for (const QString &line : lines) {
        const QStringList fields = line.split(QLatin1Char('\t'));
        if (fields.size() != 3 || fields.at(2).isEmpty()) {
            continue;
        }
        const QString &kind = fields.at(0);
        if (kind != QLatin1String("d") && kind != QLatin1String("f")) {
            continue;
        }
        bool sizeOk = false;
        const qint64 size = fields.at(1).toLongLong(&sizeOk);
        if (!sizeOk) {
            continue;
        }
        entries.append({fields.at(2), kind == QLatin1String("d"), size});
    }
    return entries;
}

int reorderDestination(int fromRow, int insertionRow)
{
    return insertionRow > fromRow ? insertionRow - 1 : insertionRow;
}

}  // namespace dropcore
