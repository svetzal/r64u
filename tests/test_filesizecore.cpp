/**
 * @file test_filesizecore.cpp
 * @brief Unit tests for filesize::humanSize, the one size format shown to the user.
 */

#include "core/filesizecore.h"

#include <QtTest>

class TestFileSizeCore : public QObject
{
    Q_OBJECT

private slots:
    void initTestCase() { QLocale::setDefault(QLocale::c()); }

    void testHumanSize_belowOneKilobyte_showsBytes()
    {
        QCOMPARE(filesize::humanSize(0), QStringLiteral("0 bytes"));
        QCOMPARE(filesize::humanSize(916), QStringLiteral("916 bytes"));
    }

    void testHumanSize_kilobytes_showOneDecimal()
    {
        QCOMPARE(filesize::humanSize(8192), QStringLiteral("8.0 kB"));
        QCOMPARE(filesize::humanSize(27536), QStringLiteral("26.9 kB"));
        QCOMPARE(filesize::humanSize(174848), QStringLiteral("170.8 kB"));
    }

    void testHumanSize_megabytes_useTraditionalUnits()
    {
        QCOMPARE(filesize::humanSize(qint64{1024} * 1024 * 3 / 2), QStringLiteral("1.5 MB"));
    }
};

QTEST_APPLESS_MAIN(TestFileSizeCore)
#include "test_filesizecore.moc"
