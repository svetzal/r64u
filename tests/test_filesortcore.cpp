/**
 * @file test_filesortcore.cpp
 * @brief Unit tests for filesort, the listing order shared by both file browsers.
 */

#include "core/filesortcore.h"

#include <QtTest>

namespace {

filesort::Entry file(const QString &name, qint64 size, const QString &type = QString())
{
    return {name, false, size, type};
}

filesort::Entry directory(const QString &name)
{
    return {name, true, 0, QStringLiteral("Directory")};
}

}  // namespace

class TestFileSortCore : public QObject
{
    Q_OBJECT

private slots:
    void testKeyForColumn_mapsTheThreeListingColumns()
    {
        QCOMPARE(filesort::keyForColumn(0), filesort::Key::Name);
        QCOMPARE(filesort::keyForColumn(1), filesort::Key::Size);
        QCOMPARE(filesort::keyForColumn(2), filesort::Key::Type);
        QCOMPARE(filesort::keyForColumn(7), filesort::Key::Name);
    }

    void testLessThan_directoriesPrecedeFiles_inEitherDirection()
    {
        const auto dir = directory("zzz");
        const auto aFile = file("aaa", 0);

        QVERIFY(filesort::lessThan(dir, aFile, {filesort::Key::Name, Qt::AscendingOrder}));
        QVERIFY(!filesort::lessThan(aFile, dir, {filesort::Key::Name, Qt::AscendingOrder}));
        QVERIFY(filesort::lessThan(dir, aFile, {filesort::Key::Size, Qt::DescendingOrder}));
        QVERIFY(!filesort::lessThan(aFile, dir, {filesort::Key::Size, Qt::DescendingOrder}));
    }

    void testLessThan_byName_isCaseInsensitive()
    {
        const filesort::Spec byName;
        QVERIFY(filesort::lessThan(file("apple", 0), file("Mango", 0), byName));
        QVERIFY(filesort::lessThan(file("Mango", 0), file("zebra", 0), byName));
        QVERIFY(!filesort::lessThan(file("zebra", 0), file("Mango", 0), byName));
    }

    void testLessThan_bySize_ordersRawBytes_notTheDisplayedText()
    {
        // As text, "170.8 KB" < "26.9 KB" < "916 bytes"; as bytes the order is the reverse
        const filesort::Spec bySize{filesort::Key::Size, Qt::AscendingOrder};
        QVERIFY(filesort::lessThan(file("c", 916), file("b", 27536), bySize));
        QVERIFY(filesort::lessThan(file("b", 27536), file("a", 174848), bySize));
        QVERIFY(!filesort::lessThan(file("a", 174848), file("c", 916), bySize));
    }

    void testLessThan_descending_reversesTheKeyOnly()
    {
        const filesort::Spec bySizeDesc{filesort::Key::Size, Qt::DescendingOrder};
        QVERIFY(filesort::lessThan(file("a", 174848), file("c", 916), bySizeDesc));
        QVERIFY(!filesort::lessThan(file("c", 916), file("a", 174848), bySizeDesc));
    }

    void testLessThan_equalKeys_fallBackToAscendingName()
    {
        const filesort::Spec bySizeDesc{filesort::Key::Size, Qt::DescendingOrder};
        QVERIFY(filesort::lessThan(file("apple", 100), file("Mango", 100), bySizeDesc));
        QVERIFY(!filesort::lessThan(file("Mango", 100), file("apple", 100), bySizeDesc));
    }

    void testLessThan_byType_comparesTheTypeColumnText()
    {
        const filesort::Spec byType{filesort::Key::Type, Qt::AscendingOrder};
        QVERIFY(filesort::lessThan(file("z", 0, "Disk Image"), file("a", 0, "SID Music"), byType));
        QVERIFY(!filesort::lessThan(file("a", 0, "SID Music"), file("z", 0, "Disk Image"), byType));
    }
};

QTEST_APPLESS_MAIN(TestFileSortCore)
#include "test_filesortcore.moc"
