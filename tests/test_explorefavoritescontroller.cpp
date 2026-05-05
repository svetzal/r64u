/**
 * @file test_explorefavoritescontroller.cpp
 * @brief Unit tests for ExploreFavoritesController.
 *
 * Uses a real FavoritesService with an isolated QSettings namespace so no
 * user settings are modified.  QSignalSpy captures signal emissions from the
 * controller.  QAction and QMenu are created with Qt parent ownership so the
 * test harness does not leak memory.
 */

#include "services/favoritesservice.h"
#include "ui/explorefavoritescontroller.h"

#include <QAction>
#include <QMenu>
#include <QSettings>
#include <QSignalSpy>
#include <QtTest>

class TestExploreFavoritesController : public QObject
{
    Q_OBJECT

private:
    FavoritesService *favService_ = nullptr;
    ExploreFavoritesController *controller_ = nullptr;

private slots:
    void init()
    {
        QCoreApplication::setOrganizationName("r64utest");
        QCoreApplication::setApplicationName("test_explorefavoritescontroller");

        QSettings settings;
        settings.remove("bookmarks/paths");

        favService_ = new FavoritesService(this);
        controller_ = new ExploreFavoritesController(favService_, this);
    }

    void cleanup()
    {
        delete controller_;
        controller_ = nullptr;

        delete favService_;
        favService_ = nullptr;

        QSettings settings;
        settings.remove("bookmarks/paths");
    }

    // -----------------------------------------------------------------------
    // onFavoriteSelected — null action
    // -----------------------------------------------------------------------

    void testOnFavoriteSelected_NullAction_NoSignalsEmitted()
    {
        QSignalSpy navSpy(controller_, &ExploreFavoritesController::navigateToPath);
        QSignalSpy msgSpy(controller_, &ExploreFavoritesController::statusMessage);

        controller_->onFavoriteSelected(nullptr);

        QCOMPARE(navSpy.count(), 0);
        QCOMPARE(msgSpy.count(), 0);
    }

    // -----------------------------------------------------------------------
    // onFavoriteSelected — action with empty data
    // -----------------------------------------------------------------------

    void testOnFavoriteSelected_ActionWithEmptyData_NoSignalsEmitted()
    {
        QAction action;
        action.setData(QString(""));

        QSignalSpy navSpy(controller_, &ExploreFavoritesController::navigateToPath);
        QSignalSpy msgSpy(controller_, &ExploreFavoritesController::statusMessage);

        controller_->onFavoriteSelected(&action);

        QCOMPARE(navSpy.count(), 0);
        QCOMPARE(msgSpy.count(), 0);
    }

    // -----------------------------------------------------------------------
    // onFavoriteSelected — action with directory path (no extension)
    // -----------------------------------------------------------------------

    void testOnFavoriteSelected_ActionWithDirectoryPath_EmitsNavigateOnly()
    {
        QAction action;
        action.setData(QString("/SD/Music"));

        QSignalSpy navSpy(controller_, &ExploreFavoritesController::navigateToPath);
        QSignalSpy msgSpy(controller_, &ExploreFavoritesController::statusMessage);

        controller_->onFavoriteSelected(&action);

        QCOMPARE(navSpy.count(), 1);
        QCOMPARE(navSpy.at(0).at(0).toString(), QString("/SD/Music"));
        QCOMPARE(msgSpy.count(), 0);
    }

    // -----------------------------------------------------------------------
    // onFavoriteSelected — action with file path (has extension)
    // -----------------------------------------------------------------------

    void testOnFavoriteSelected_ActionWithFilePath_EmitsNavigateToParentAndStatus()
    {
        QAction action;
        action.setData(QString("/SD/Music/song.sid"));

        QSignalSpy navSpy(controller_, &ExploreFavoritesController::navigateToPath);
        QSignalSpy msgSpy(controller_, &ExploreFavoritesController::statusMessage);

        controller_->onFavoriteSelected(&action);

        QCOMPARE(navSpy.count(), 1);
        QCOMPARE(navSpy.at(0).at(0).toString(), QString("/SD/Music"));
        QCOMPARE(msgSpy.count(), 1);
        QVERIFY(msgSpy.at(0).at(0).toString().contains("song.sid"));
    }

    void testOnFavoriteSelected_FilePathAtRoot_NavigatesToRoot()
    {
        // Path like "/song.sid" — parent dir is empty, should become "/"
        QAction action;
        action.setData(QString("/song.sid"));

        QSignalSpy navSpy(controller_, &ExploreFavoritesController::navigateToPath);

        controller_->onFavoriteSelected(&action);

        QCOMPARE(navSpy.count(), 1);
        QCOMPARE(navSpy.at(0).at(0).toString(), QString("/"));
    }

    // -----------------------------------------------------------------------
    // onToggleFavorite — empty path
    // -----------------------------------------------------------------------

    void testOnToggleFavorite_EmptyPath_NoSignalsEmitted()
    {
        QSignalSpy spy(controller_, &ExploreFavoritesController::statusMessage);

        controller_->onToggleFavorite("");

        QCOMPARE(spy.count(), 0);
    }

    // -----------------------------------------------------------------------
    // onToggleFavorite — valid path, adding
    // -----------------------------------------------------------------------

    void testOnToggleFavorite_ValidPath_NotFavorite_EmitsAddedMessage()
    {
        QSignalSpy spy(controller_, &ExploreFavoritesController::statusMessage);

        controller_->onToggleFavorite("/SD/Music/song.sid");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Added"));
    }

    void testOnToggleFavorite_ValidPath_AddsThenRemoves_EmitsRemovedMessage()
    {
        // Add first
        controller_->onToggleFavorite("/SD/Music/song.sid");

        QSignalSpy spy(controller_, &ExploreFavoritesController::statusMessage);

        // Toggle again to remove
        controller_->onToggleFavorite("/SD/Music/song.sid");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Removed"));
    }

    void testOnToggleFavorite_ValidPath_UpdatesIsFavoriteState()
    {
        QVERIFY(!controller_->isFavorite("/SD/Music/song.sid"));

        controller_->onToggleFavorite("/SD/Music/song.sid");

        QVERIFY(controller_->isFavorite("/SD/Music/song.sid"));
    }

    // -----------------------------------------------------------------------
    // onToggleFavorite — toggleFavoriteAction text and checked state
    // -----------------------------------------------------------------------

    void testOnToggleFavorite_WithToggleAction_UpdatesActionState()
    {
        QAction toggleAction;
        toggleAction.setCheckable(true);
        controller_->setToggleAction(&toggleAction);

        // Add: should be checked and show star
        controller_->onToggleFavorite("/SD/Music/song.sid");
        QVERIFY(toggleAction.isChecked());

        // Remove: should not be checked and show outline star
        controller_->onToggleFavorite("/SD/Music/song.sid");
        QVERIFY(!toggleAction.isChecked());
    }

    // -----------------------------------------------------------------------
    // updateForPath — no toggle action or service
    // -----------------------------------------------------------------------

    void testUpdateForPath_NoToggleAction_DoesNotCrash()
    {
        // controller_ has no toggleAction set; should silently do nothing
        controller_->updateForPath("/SD/Music");
        QVERIFY(true);  // Reached without crash
    }

    // -----------------------------------------------------------------------
    // updateForPath — path is not a favorite
    // -----------------------------------------------------------------------

    void testUpdateForPath_PathNotFavorite_ActionUncheckedShowsOutlineStar()
    {
        QAction toggleAction;
        controller_->setToggleAction(&toggleAction);

        controller_->updateForPath("/SD/Music/unknown.sid");

        QVERIFY(!toggleAction.isChecked());
        QCOMPARE(toggleAction.text(), QString::fromUtf8("☆"));
    }

    // -----------------------------------------------------------------------
    // updateForPath — path is a favorite
    // -----------------------------------------------------------------------

    void testUpdateForPath_PathIsFavorite_ActionCheckedShowsFilledStar()
    {
        favService_->addFavorite("/SD/Music/song.sid");

        QAction toggleAction;
        toggleAction.setCheckable(true);
        controller_->setToggleAction(&toggleAction);

        controller_->updateForPath("/SD/Music/song.sid");

        QVERIFY(toggleAction.isChecked());
        QCOMPARE(toggleAction.text(), QString::fromUtf8("⭐"));
    }

    // -----------------------------------------------------------------------
    // onFavoritesChanged — menu is rebuilt
    // -----------------------------------------------------------------------

    void testOnFavoritesChanged_WithMenu_ClearsAndRepopulatesMenu()
    {
        QMenu menu;
        controller_->setFavoritesMenu(&menu);

        // Menu starts with a placeholder (no favorites)
        int initialCount = menu.actions().count();
        QVERIFY(initialCount >= 1);

        // Add a favorite — favoritesChanged signal triggers onFavoritesChanged
        favService_->addFavorite("/SD/Music/song.sid");

        // Menu should now contain an entry for the favorite
        bool found = false;
        for (QAction *action : menu.actions()) {
            if (action->data().toString() == "/SD/Music/song.sid") {
                found = true;
                break;
            }
        }
        QVERIFY(found);
    }

    void testOnFavoritesChanged_NoFavorites_ContainsDisabledPlaceholder()
    {
        QMenu menu;
        controller_->setFavoritesMenu(&menu);

        // No favorites: menu should have at least one disabled action (placeholder)
        QVERIFY(!menu.actions().isEmpty());
        bool hasDisabled = false;
        for (QAction *action : menu.actions()) {
            if (!action->isEnabled()) {
                hasDisabled = true;
                break;
            }
        }
        QVERIFY(hasDisabled);
    }

    void testOnFavoritesChanged_MultipleFavorites_AllPresent()
    {
        QMenu menu;
        controller_->setFavoritesMenu(&menu);

        favService_->addFavorite("/SD/Music/a.sid");
        favService_->addFavorite("/SD/Music/b.sid");

        QStringList paths;
        for (QAction *action : menu.actions()) {
            if (!action->data().toString().isEmpty()) {
                paths.append(action->data().toString());
            }
        }

        QVERIFY(paths.contains("/SD/Music/a.sid"));
        QVERIFY(paths.contains("/SD/Music/b.sid"));
    }

    // -----------------------------------------------------------------------
    // isFavorite — delegates to service
    // -----------------------------------------------------------------------

    void testIsFavorite_PathNotAdded_ReturnsFalse()
    {
        QVERIFY(!controller_->isFavorite("/SD/nothere.sid"));
    }

    void testIsFavorite_PathAdded_ReturnsTrue()
    {
        favService_->addFavorite("/SD/Music/song.sid");

        QVERIFY(controller_->isFavorite("/SD/Music/song.sid"));
    }

    // -----------------------------------------------------------------------
    // Null FavoritesService — guard clause
    // -----------------------------------------------------------------------

    void testConstructedWithNullService_ToggleFavorite_NoSignal()
    {
        ExploreFavoritesController ctrl(nullptr, this);
        QSignalSpy spy(&ctrl, &ExploreFavoritesController::statusMessage);

        ctrl.onToggleFavorite("/SD/Music/song.sid");

        QCOMPARE(spy.count(), 0);
    }
};

QTEST_MAIN(TestExploreFavoritesController)
#include "test_explorefavoritescontroller.moc"
