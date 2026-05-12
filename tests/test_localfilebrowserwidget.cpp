/**
 * @file test_localfilebrowserwidget.cpp
 * @brief Unit tests for LocalFileBrowserWidget control-flow and public API.
 *
 * LocalFileBrowserWidget wraps QFileSystemModel and provides upload, new-folder,
 * rename, and delete actions for local files.  These tests focus on:
 *
 * - selectedPath() no selection returns empty
 * - isSelectedDirectory() no selection guard
 * - setCurrentDirectory() updates currentDirectory()
 * - setUploadEnabled() does not crash
 * - Construction does not crash
 */

#include "ui/localfilebrowserwidget.h"

#include <QStandardPaths>
#include <QtTest>

class TestLocalFileBrowserWidget : public QObject
{
    Q_OBJECT

private slots:

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        LocalFileBrowserWidget widget;
        QVERIFY(true);
    }

    // =========================================================================
    // selectedPath() — no selection returns empty
    // =========================================================================

    void testSelectedPath_NoSelection_ReturnsEmpty()
    {
        LocalFileBrowserWidget widget;
        QVERIFY(widget.selectedPath().isEmpty());
    }

    // =========================================================================
    // isSelectedDirectory() — no selection returns false
    // =========================================================================

    void testIsSelectedDirectory_NoSelection_ReturnsFalse()
    {
        LocalFileBrowserWidget widget;
        QVERIFY(!widget.isSelectedDirectory());
    }

    // =========================================================================
    // selectedPaths() — no selection returns empty list
    // =========================================================================

    void testSelectedPaths_NoSelection_ReturnsEmpty()
    {
        LocalFileBrowserWidget widget;
        QVERIFY(widget.selectedPaths().isEmpty());
    }

    // =========================================================================
    // setCurrentDirectory() — updates accessor
    // =========================================================================

    void testSetCurrentDirectory_UpdatesCurrentDirectory()
    {
        LocalFileBrowserWidget widget;
        QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        widget.setCurrentDirectory(homePath);
        QCOMPARE(widget.currentDirectory(), homePath);
    }

    // =========================================================================
    // setUploadEnabled() — does not crash
    // =========================================================================

    void testSetUploadEnabled_True_DoesNotCrash()
    {
        LocalFileBrowserWidget widget;
        widget.setUploadEnabled(true);
        QVERIFY(true);
    }

    void testSetUploadEnabled_False_DoesNotCrash()
    {
        LocalFileBrowserWidget widget;
        widget.setUploadEnabled(false);
        QVERIFY(true);
    }

    // =========================================================================
    // currentDirectory() — returns initial home directory
    // =========================================================================

    void testCurrentDirectory_Initial_ReturnsHomeLocation()
    {
        LocalFileBrowserWidget widget;
        QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        QCOMPARE(widget.currentDirectory(), homePath);
    }
};

QTEST_MAIN(TestLocalFileBrowserWidget)
#include "test_localfilebrowserwidget.moc"
