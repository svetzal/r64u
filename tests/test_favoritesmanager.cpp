/**
 * @file test_favoritesmanager.cpp
 * @brief Unit tests for FavoritesManager — the imperative shell around favorites:: core functions.
 *
 * Tests cover signal emissions, QSettings round-trip persistence, and all
 * public slot operations.  Each test uses an isolated QSettings key to avoid
 * polluting user settings.
 */

#include "services/favoritesmanager.h"

#include <QSettings>
#include <QSignalSpy>
#include <QtTest>

class TestFavoritesManager : public QObject
{
    Q_OBJECT

private slots:
    void init()
    {
        // Use a test-specific application name so we don't clobber real settings
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_favoritesmanager");

        // Clear any leftover data from a previous run
        QSettings settings;
        settings.remove("bookmarks/paths");

        manager_ = new FavoritesManager(this);
    }

    void cleanup()
    {
        delete manager_;
        manager_ = nullptr;

        QSettings settings;
        settings.remove("bookmarks/paths");
    }

    // =========================================================
    // Initial state
    // =========================================================

    void testInitialState_Empty()
    {
        QVERIFY(manager_->favorites().isEmpty());
        QCOMPARE(manager_->count(), 0);
    }

    void testIsFavorite_EmptyList_ReturnsFalse()
    {
        QVERIFY(!manager_->isFavorite("/SD/games/test.sid"));
    }

    // =========================================================
    // addFavorite
    // =========================================================

    void testAddFavorite_AddsPath()
    {
        manager_->addFavorite("/SD/games/test.sid");

        QCOMPARE(manager_->count(), 1);
        QVERIFY(manager_->isFavorite("/SD/games/test.sid"));
    }

    void testAddFavorite_EmitsFavoriteAdded()
    {
        QSignalSpy spy(manager_, &FavoritesManager::favoriteAdded);

        manager_->addFavorite("/SD/games/test.sid");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QString("/SD/games/test.sid"));
    }

    void testAddFavorite_EmitsFavoritesChanged()
    {
        QSignalSpy spy(manager_, &FavoritesManager::favoritesChanged);

        manager_->addFavorite("/SD/games/test.sid");

        QCOMPARE(spy.count(), 1);
    }

    void testAddFavorite_Duplicate_DoesNotAdd()
    {
        manager_->addFavorite("/SD/games/test.sid");
        manager_->addFavorite("/SD/games/test.sid");

        QCOMPARE(manager_->count(), 1);
    }

    void testAddFavorite_Duplicate_DoesNotEmitSignals()
    {
        manager_->addFavorite("/SD/games/test.sid");

        QSignalSpy addedSpy(manager_, &FavoritesManager::favoriteAdded);
        QSignalSpy changedSpy(manager_, &FavoritesManager::favoritesChanged);

        manager_->addFavorite("/SD/games/test.sid");

        QCOMPARE(addedSpy.count(), 0);
        QCOMPARE(changedSpy.count(), 0);
    }

    void testAddFavorite_MultiplePaths()
    {
        manager_->addFavorite("/SD/games/a.sid");
        manager_->addFavorite("/SD/games/b.sid");
        manager_->addFavorite("/SD/games/c.sid");

        QCOMPARE(manager_->count(), 3);
    }

    // =========================================================
    // removeFavorite
    // =========================================================

    void testRemoveFavorite_RemovesExistingPath()
    {
        manager_->addFavorite("/SD/games/test.sid");
        manager_->removeFavorite("/SD/games/test.sid");

        QCOMPARE(manager_->count(), 0);
        QVERIFY(!manager_->isFavorite("/SD/games/test.sid"));
    }

    void testRemoveFavorite_EmitsFavoriteRemoved()
    {
        manager_->addFavorite("/SD/games/test.sid");
        QSignalSpy spy(manager_, &FavoritesManager::favoriteRemoved);

        manager_->removeFavorite("/SD/games/test.sid");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().first().toString(), QString("/SD/games/test.sid"));
    }

    void testRemoveFavorite_EmitsFavoritesChanged()
    {
        manager_->addFavorite("/SD/games/test.sid");
        QSignalSpy spy(manager_, &FavoritesManager::favoritesChanged);

        manager_->removeFavorite("/SD/games/test.sid");

        QCOMPARE(spy.count(), 1);
    }

    void testRemoveFavorite_NonExistent_DoesNotEmitSignals()
    {
        QSignalSpy removedSpy(manager_, &FavoritesManager::favoriteRemoved);
        QSignalSpy changedSpy(manager_, &FavoritesManager::favoritesChanged);

        manager_->removeFavorite("/SD/games/nonexistent.sid");

        QCOMPARE(removedSpy.count(), 0);
        QCOMPARE(changedSpy.count(), 0);
    }

    // =========================================================
    // toggleFavorite
    // =========================================================

    void testToggleFavorite_AddWhenAbsent_ReturnsTrue()
    {
        bool result = manager_->toggleFavorite("/SD/games/test.sid");

        QVERIFY(result);
        QVERIFY(manager_->isFavorite("/SD/games/test.sid"));
    }

    void testToggleFavorite_RemoveWhenPresent_ReturnsFalse()
    {
        manager_->addFavorite("/SD/games/test.sid");

        bool result = manager_->toggleFavorite("/SD/games/test.sid");

        QVERIFY(!result);
        QVERIFY(!manager_->isFavorite("/SD/games/test.sid"));
    }

    void testToggleFavorite_EmitsFavoriteAddedWhenAdding()
    {
        QSignalSpy spy(manager_, &FavoritesManager::favoriteAdded);

        manager_->toggleFavorite("/SD/games/test.sid");

        QCOMPARE(spy.count(), 1);
    }

    void testToggleFavorite_EmitsFavoriteRemovedWhenRemoving()
    {
        manager_->addFavorite("/SD/games/test.sid");
        QSignalSpy spy(manager_, &FavoritesManager::favoriteRemoved);

        manager_->toggleFavorite("/SD/games/test.sid");

        QCOMPARE(spy.count(), 1);
    }

    // =========================================================
    // moveFavorite
    // =========================================================

    void testMoveFavorite_ChangesOrder()
    {
        manager_->addFavorite("/SD/games/a.sid");
        manager_->addFavorite("/SD/games/b.sid");
        manager_->addFavorite("/SD/games/c.sid");

        // favorites() returns sorted, but the internal list is in insertion order.
        // moveFavorite operates on insertion order (favorites_), not the sorted view.
        // After move(0,2): [b, c, a] in internal order.
        manager_->moveFavorite(0, 2);

        QCOMPARE(manager_->count(), 3);
        // No removal happened
        QVERIFY(manager_->isFavorite("/SD/games/a.sid"));
        QVERIFY(manager_->isFavorite("/SD/games/b.sid"));
        QVERIFY(manager_->isFavorite("/SD/games/c.sid"));
    }

    void testMoveFavorite_EmitsFavoritesChanged()
    {
        manager_->addFavorite("/SD/games/a.sid");
        manager_->addFavorite("/SD/games/b.sid");
        QSignalSpy spy(manager_, &FavoritesManager::favoritesChanged);

        manager_->moveFavorite(0, 1);

        QCOMPARE(spy.count(), 1);
    }

    void testMoveFavorite_SameIndex_NoSignal()
    {
        manager_->addFavorite("/SD/games/a.sid");
        manager_->addFavorite("/SD/games/b.sid");
        QSignalSpy spy(manager_, &FavoritesManager::favoritesChanged);

        manager_->moveFavorite(0, 0);

        QCOMPARE(spy.count(), 0);
    }

    // =========================================================
    // clearAll
    // =========================================================

    void testClearAll_RemovesAllFavorites()
    {
        manager_->addFavorite("/SD/games/a.sid");
        manager_->addFavorite("/SD/games/b.sid");

        manager_->clearAll();

        QCOMPARE(manager_->count(), 0);
        QVERIFY(manager_->favorites().isEmpty());
    }

    void testClearAll_EmitsFavoritesChanged()
    {
        manager_->addFavorite("/SD/games/a.sid");
        QSignalSpy spy(manager_, &FavoritesManager::favoritesChanged);

        manager_->clearAll();

        QCOMPARE(spy.count(), 1);
    }

    void testClearAll_WhenEmpty_DoesNotEmitSignal()
    {
        QSignalSpy spy(manager_, &FavoritesManager::favoritesChanged);

        manager_->clearAll();

        QCOMPARE(spy.count(), 0);
    }

    // =========================================================
    // favorites() returns sorted list
    // =========================================================

    void testFavorites_ReturnsSortedList()
    {
        manager_->addFavorite("/SD/z_last.sid");
        manager_->addFavorite("/SD/a_first.sid");
        manager_->addFavorite("/SD/m_middle.sid");

        QStringList sorted = manager_->favorites();

        QCOMPARE(sorted[0], QString("/SD/a_first.sid"));
        QCOMPARE(sorted[1], QString("/SD/m_middle.sid"));
        QCOMPARE(sorted[2], QString("/SD/z_last.sid"));
    }

    // =========================================================
    // saveSettings / loadSettings round-trip
    // =========================================================

    void testSaveAndLoadSettings_RoundTrip()
    {
        manager_->addFavorite("/SD/games/a.sid");
        manager_->addFavorite("/SD/games/b.sid");
        manager_->saveSettings();

        // Create a second manager that loads the saved settings
        auto *manager2 = new FavoritesManager(this);

        QCOMPARE(manager2->count(), 2);
        QVERIFY(manager2->isFavorite("/SD/games/a.sid"));
        QVERIFY(manager2->isFavorite("/SD/games/b.sid"));

        delete manager2;
    }

    void testLoadSettings_Empty_StartsEmpty()
    {
        // No settings have been saved; manager_ loads empty list in init()
        QCOMPARE(manager_->count(), 0);
    }

private:
    FavoritesManager *manager_ = nullptr;
};

QTEST_MAIN(TestFavoritesManager)
#include "test_favoritesmanager.moc"
