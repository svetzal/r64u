#include <QtTest>
#include <cmath>

#include "utils/rollingstats.h"

class TestRollingStats : public QObject
{
    Q_OBJECT

private slots:
    // ========== Constructor and basic state ==========

    void testConstructor()
    {
        RollingStats stats(100);
        QCOMPARE(stats.count(), static_cast<size_t>(0));
        QCOMPARE(stats.windowSize(), static_cast<size_t>(100));
        QVERIFY(!stats.isFull());
        QCOMPARE(stats.mean(), 0.0);
        QCOMPARE(stats.stddev(), 0.0);
    }

    void testDefaultWindowSize()
    {
        RollingStats stats;
        QCOMPARE(stats.windowSize(), static_cast<size_t>(100));
    }

    // ========== Single sample ==========

    void testSingleSample()
    {
        RollingStats stats(10);
        stats.addSample(42.0);

        QCOMPARE(stats.count(), static_cast<size_t>(1));
        QCOMPARE(stats.mean(), 42.0);
        QCOMPARE(stats.stddev(), 0.0);  // stddev needs at least 2 samples
        QCOMPARE(stats.min(), 42.0);
        QCOMPARE(stats.max(), 42.0);
    }

    // ========== Multiple samples ==========

    void testMultipleSamples()
    {
        RollingStats stats(10);
        stats.addSample(10.0);
        stats.addSample(20.0);
        stats.addSample(30.0);

        QCOMPARE(stats.count(), static_cast<size_t>(3));
        QCOMPARE(stats.mean(), 20.0);
        QCOMPARE(stats.min(), 10.0);
        QCOMPARE(stats.max(), 30.0);

        // Sample stddev for [10, 20, 30] = sqrt(((10-20)^2 + (20-20)^2 + (30-20)^2) / 2) = sqrt(200/2) = 10
        QVERIFY(std::abs(stats.stddev() - 10.0) < 0.0001);
    }

    void testMeanCalculation()
    {
        RollingStats stats(100);
        for (int i = 1; i <= 10; i++) {
            stats.addSample(static_cast<double>(i));
        }

        // Mean of 1..10 = 5.5
        QVERIFY(std::abs(stats.mean() - 5.5) < 0.0001);
    }

    // ========== Rolling window behavior ==========

    void testRollingWindow()
    {
        RollingStats stats(3);  // Small window for testing

        stats.addSample(10.0);
        stats.addSample(20.0);
        stats.addSample(30.0);

        QVERIFY(stats.isFull());
        QCOMPARE(stats.mean(), 20.0);  // (10 + 20 + 30) / 3

        // Add another sample - oldest (10) should be removed
        stats.addSample(40.0);

        QCOMPARE(stats.count(), static_cast<size_t>(3));
        QCOMPARE(stats.mean(), 30.0);  // (20 + 30 + 40) / 3
        QCOMPARE(stats.min(), 20.0);
        QCOMPARE(stats.max(), 40.0);
    }

    void testRollingWindowMinMax()
    {
        RollingStats stats(3);

        stats.addSample(100.0);  // This will be the max
        stats.addSample(50.0);
        stats.addSample(75.0);

        QCOMPARE(stats.max(), 100.0);

        // Add a new sample, removing the max (100)
        stats.addSample(60.0);

        QCOMPARE(stats.max(), 75.0);  // New max after 100 is removed
        QCOMPARE(stats.min(), 50.0);
    }

    // ========== Stddev calculation ==========

    void testStddevWithKnownValues()
    {
        RollingStats stats(10);

        // Add values: 2, 4, 4, 4, 5, 5, 7, 9
        // Mean = 40/8 = 5
        // Variance = ((2-5)^2 + (4-5)^2 + (4-5)^2 + (4-5)^2 + (5-5)^2 + (5-5)^2 + (7-5)^2 + (9-5)^2) / 7
        //          = (9 + 1 + 1 + 1 + 0 + 0 + 4 + 16) / 7 = 32/7
        // Sample stddev = sqrt(32/7) ≈ 2.138

        stats.addSample(2.0);
        stats.addSample(4.0);
        stats.addSample(4.0);
        stats.addSample(4.0);
        stats.addSample(5.0);
        stats.addSample(5.0);
        stats.addSample(7.0);
        stats.addSample(9.0);

        QCOMPARE(stats.mean(), 5.0);
        QVERIFY(std::abs(stats.stddev() - 2.138) < 0.01);
    }

    void testPopulationStddev()
    {
        RollingStats stats(10);

        stats.addSample(2.0);
        stats.addSample(4.0);
        stats.addSample(4.0);
        stats.addSample(4.0);
        stats.addSample(5.0);
        stats.addSample(5.0);
        stats.addSample(7.0);
        stats.addSample(9.0);

        // Population stddev = sqrt(32/8) = 2.0
        QCOMPARE(stats.stddevPopulation(), 2.0);
    }

    // ========== Clear ==========

    void testClear()
    {
        RollingStats stats(10);

        stats.addSample(10.0);
        stats.addSample(20.0);
        stats.addSample(30.0);

        stats.clear();

        QCOMPARE(stats.count(), static_cast<size_t>(0));
        QCOMPARE(stats.mean(), 0.0);
        QCOMPARE(stats.stddev(), 0.0);
        QVERIFY(!stats.isFull());
    }

    // ========== Edge cases ==========

    void testWindowSizeOne()
    {
        RollingStats stats(1);

        stats.addSample(10.0);
        QCOMPARE(stats.mean(), 10.0);
        QVERIFY(stats.isFull());

        stats.addSample(20.0);
        QCOMPARE(stats.mean(), 20.0);
        QCOMPARE(stats.count(), static_cast<size_t>(1));
    }

    void testNegativeValues()
    {
        RollingStats stats(10);

        stats.addSample(-10.0);
        stats.addSample(-5.0);
        stats.addSample(0.0);
        stats.addSample(5.0);
        stats.addSample(10.0);

        QCOMPARE(stats.mean(), 0.0);
        QCOMPARE(stats.min(), -10.0);
        QCOMPARE(stats.max(), 10.0);
    }

    void testSameValues()
    {
        RollingStats stats(10);

        for (int i = 0; i < 10; i++) {
            stats.addSample(42.0);
        }

        QCOMPARE(stats.mean(), 42.0);
        QCOMPARE(stats.stddev(), 0.0);
        QCOMPARE(stats.min(), 42.0);
        QCOMPARE(stats.max(), 42.0);
    }

    void testLargeWindow()
    {
        RollingStats stats(1000);

        for (int i = 0; i < 1000; i++) {
            stats.addSample(static_cast<double>(i));
        }

        QVERIFY(stats.isFull());
        QCOMPARE(stats.min(), 0.0);
        QCOMPARE(stats.max(), 999.0);
        // Mean of 0..999 = 499.5
        QVERIFY(std::abs(stats.mean() - 499.5) < 0.0001);
    }

    void testOverflowWindow()
    {
        RollingStats stats(5);

        // Add 10 samples to a window of 5
        for (int i = 1; i <= 10; i++) {
            stats.addSample(static_cast<double>(i));
        }

        QCOMPARE(stats.count(), static_cast<size_t>(5));
        // Window should contain 6, 7, 8, 9, 10
        QCOMPARE(stats.min(), 6.0);
        QCOMPARE(stats.max(), 10.0);
        QCOMPARE(stats.mean(), 8.0);  // (6+7+8+9+10)/5
    }
};

QTEST_MAIN(TestRollingStats)
#include "test_rollingstats.moc"
