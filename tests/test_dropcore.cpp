/**
 * @file test_dropcore.cpp
 * @brief Unit tests for the drag-and-drop decisions in namespace dropcore.
 */

#include "core/dropcore.h"

#include <QtTest>

using dropcore::DropEntry;
using dropcore::DropPlan;
using dropcore::DropTarget;
using dropcore::Pane;
using dropcore::Source;

class TestDropCore : public QObject
{
    Q_OBJECT

private:
    static QList<DropEntry> oneFile(const QString &path = "/tmp/game.prg")
    {
        return {DropEntry{path, false, 1024}};
    }

    static DropTarget paneAt(Pane pane, const QString &directory)
    {
        return DropTarget{pane, directory, false, {}};
    }

    static DropTarget directoryRowIn(Pane pane, const QString &directory, const QString &rowPath)
    {
        return DropTarget{pane, directory, true, rowPath};
    }

private slots:

    // -----------------------------------------------------------------------
    // planDrop: what goes where
    // -----------------------------------------------------------------------

    void testLocalOntoRemote_uploadsIntoTheCurrentDirectory()
    {
        const auto plan =
            dropcore::planDrop(Source::LocalPane, oneFile(), paneAt(Pane::Remote, "/SD"));

        QVERIFY(plan.has_value());
        QCOMPARE(plan->kind, DropPlan::Kind::Upload);
        QCOMPARE(plan->destinationDirectory, QString("/SD"));
        QCOMPARE(plan->entries.size(), 1);
        QCOMPARE(plan->entries.first().path, QString("/tmp/game.prg"));
    }

    void testLocalOntoRemoteDirectoryRow_uploadsIntoThatDirectory()
    {
        const auto plan = dropcore::planDrop(Source::LocalPane, oneFile(),
                                             directoryRowIn(Pane::Remote, "/SD", "/SD/Games"));

        QVERIFY(plan.has_value());
        QCOMPARE(plan->destinationDirectory, QString("/SD/Games"));
    }

    void testExternalOntoRemote_uploads()
    {
        const auto plan =
            dropcore::planDrop(Source::External, oneFile(), paneAt(Pane::Remote, "/"));

        QVERIFY(plan.has_value());
        QCOMPARE(plan->kind, DropPlan::Kind::Upload);
        QCOMPARE(plan->destinationDirectory, QString("/"));
    }

    void testRemoteOntoLocal_downloadsIntoTheCurrentDirectory()
    {
        const auto plan = dropcore::planDrop(Source::RemotePane, oneFile("/SD/tune.sid"),
                                             paneAt(Pane::Local, "/Users/me/c64"));

        QVERIFY(plan.has_value());
        QCOMPARE(plan->kind, DropPlan::Kind::Download);
        QCOMPARE(plan->destinationDirectory, QString("/Users/me/c64"));
        QCOMPARE(plan->entries.first().path, QString("/SD/tune.sid"));
    }

    void testRemoteOntoLocalDirectoryRow_downloadsIntoThatDirectory()
    {
        const auto plan =
            dropcore::planDrop(Source::RemotePane, oneFile("/SD/tune.sid"),
                               directoryRowIn(Pane::Local, "/Users/me", "/Users/me/music"));

        QVERIFY(plan.has_value());
        QCOMPARE(plan->destinationDirectory, QString("/Users/me/music"));
    }

    void testPlan_keepsDirectoryFlagAndSizeOfEachEntry()
    {
        const QList<DropEntry> entries = {DropEntry{"/SD/Games", true, 0},
                                          DropEntry{"/SD/big.reu", false, 16777216}};

        const auto plan =
            dropcore::planDrop(Source::RemotePane, entries, paneAt(Pane::Local, "/x"));

        QVERIFY(plan.has_value());
        QCOMPARE(plan->entries.size(), 2);
        QCOMPARE(plan->entries.at(0).isDirectory, true);
        QCOMPARE(plan->entries.at(1).size, qint64(16777216));
    }

    // -----------------------------------------------------------------------
    // planDrop: drops that mean nothing
    // -----------------------------------------------------------------------

    void testLocalOntoLocal_isRefused()
    {
        QVERIFY(!dropcore::planDrop(Source::LocalPane, oneFile(), paneAt(Pane::Local, "/x")));
        QVERIFY(!dropcore::planDrop(Source::LocalPane, oneFile(),
                                    directoryRowIn(Pane::Local, "/x", "/x/y")));
    }

    void testRemoteOntoRemote_isRefused()
    {
        QVERIFY(
            !dropcore::planDrop(Source::RemotePane, oneFile("/SD/a"), paneAt(Pane::Remote, "/")));
        QVERIFY(!dropcore::planDrop(Source::RemotePane, oneFile("/SD/a"),
                                    directoryRowIn(Pane::Remote, "/", "/SD")));
    }

    void testExternalOntoLocal_isRefused()
    {
        QVERIFY(!dropcore::planDrop(Source::External, oneFile(), paneAt(Pane::Local, "/x")));
    }

    void testNoEntries_isRefused()
    {
        QVERIFY(!dropcore::planDrop(Source::LocalPane, {}, paneAt(Pane::Remote, "/")));
        QVERIFY(!dropcore::planDrop(Source::RemotePane, {}, paneAt(Pane::Local, "/x")));
    }

    // -----------------------------------------------------------------------
    // canAccept mirrors planDrop so a drag can be refused on entry
    // -----------------------------------------------------------------------

    void testCanAccept_matchesThePlanningRules()
    {
        QVERIFY(dropcore::canAccept(Source::LocalPane, Pane::Remote));
        QVERIFY(dropcore::canAccept(Source::External, Pane::Remote));
        QVERIFY(dropcore::canAccept(Source::RemotePane, Pane::Local));
        QVERIFY(!dropcore::canAccept(Source::RemotePane, Pane::Remote));
        QVERIFY(!dropcore::canAccept(Source::LocalPane, Pane::Local));
        QVERIFY(!dropcore::canAccept(Source::External, Pane::Local));
    }

    // -----------------------------------------------------------------------
    // classifySource
    // -----------------------------------------------------------------------

    void testClassifySource_ownViewIsItsOwnPane()
    {
        QCOMPARE(dropcore::classifySource(true, Pane::Local, false, true), Source::LocalPane);
        QCOMPARE(dropcore::classifySource(true, Pane::Remote, true, false), Source::RemotePane);
    }

    void testClassifySource_remotePathsFromElsewhereAreTheRemotePane()
    {
        QCOMPARE(dropcore::classifySource(false, Pane::Local, true, false), Source::RemotePane);
        QCOMPARE(dropcore::classifySource(false, Pane::Remote, true, false), Source::RemotePane);
    }

    void testClassifySource_urlsFromElsewhereAreExternal()
    {
        QCOMPARE(dropcore::classifySource(false, Pane::Remote, false, true), Source::External);
    }

    void testClassifySource_unknownPayloadIsNothing()
    {
        QVERIFY(!dropcore::classifySource(false, Pane::Remote, false, false));
    }

    // -----------------------------------------------------------------------
    // Remote-paths payload
    // -----------------------------------------------------------------------

    void testEncodeDecode_roundTripsPathsFlagsAndSizes()
    {
        const QList<DropEntry> entries = {DropEntry{"/SD/Games", true, 0},
                                          DropEntry{"/SD/big.reu", false, 16777216},
                                          DropEntry{"/SD/Music/tune.sid", false, 4096}};

        const QList<DropEntry> decoded =
            dropcore::decodeRemoteEntries(dropcore::encodeRemoteEntries(entries));

        QCOMPARE(decoded.size(), 3);
        QCOMPARE(decoded.at(0).path, QString("/SD/Games"));
        QCOMPARE(decoded.at(0).isDirectory, true);
        QCOMPARE(decoded.at(1).path, QString("/SD/big.reu"));
        QCOMPARE(decoded.at(1).isDirectory, false);
        QCOMPARE(decoded.at(1).size, qint64(16777216));
        QCOMPARE(decoded.at(2).path, QString("/SD/Music/tune.sid"));
    }

    void testEncode_isOneEntryPerLine()
    {
        const QByteArray payload = dropcore::encodeRemoteEntries(
            {DropEntry{"/SD/a.prg", false, 10}, DropEntry{"/SD/Dir", true, 0}});

        QCOMPARE(payload, QByteArray("f\t10\t/SD/a.prg\nd\t0\t/SD/Dir"));
    }

    void testDecode_skipsMalformedLines()
    {
        const QList<DropEntry> decoded = dropcore::decodeRemoteEntries(
            "garbage\nf\tnotanumber\t/SD/x\nx\t0\t/SD/y\nf\t5\t\n\nf\t7\t/SD/ok.prg");

        QCOMPARE(decoded.size(), 1);
        QCOMPARE(decoded.first().path, QString("/SD/ok.prg"));
        QCOMPARE(decoded.first().size, qint64(7));
    }

    void testDecode_emptyPayloadIsNoEntries()
    {
        QVERIFY(dropcore::decodeRemoteEntries(QByteArray()).isEmpty());
    }

    // -----------------------------------------------------------------------
    // reorderDestination
    // -----------------------------------------------------------------------

    void testReorderDestination_droppingBelowALaterRowLandsBeforeIt()
    {
        // Rows [a b c d]: drag a (0) into the gap after c (insertion 3) -> [b c a d], a at 2
        QCOMPARE(dropcore::reorderDestination(0, 3), 2);
    }

    void testReorderDestination_droppingAboveAnEarlierRowLandsThere()
    {
        // Rows [a b c d]: drag d (3) into the gap before b (insertion 1) -> [a d b c], d at 1
        QCOMPARE(dropcore::reorderDestination(3, 1), 1);
    }

    void testReorderDestination_droppingAroundItselfIsAStay()
    {
        QCOMPARE(dropcore::reorderDestination(2, 2), 2);
        QCOMPARE(dropcore::reorderDestination(2, 3), 2);
    }

    void testReorderDestination_droppingAfterTheLastRowIsTheLastRow()
    {
        QCOMPARE(dropcore::reorderDestination(0, 4), 3);
    }
};

QTEST_MAIN(TestDropCore)
#include "test_dropcore.moc"
