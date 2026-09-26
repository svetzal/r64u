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
 * - confirmDestructiveAction routes through the injected IMessagePresenter
 */

#include "mocks/mockmessagepresenter.h"
#include "services/errorhandler.h"
#include "ui/localfilebrowserwidget.h"

#include <QMenu>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

/**
 * @brief Thin test subclass that exposes the protected confirmDestructiveAction method.
 */
class ExposedLocalFileBrowserWidget : public LocalFileBrowserWidget
{
public:
    explicit ExposedLocalFileBrowserWidget(ErrorHandler *errorHandler, QWidget *parent = nullptr)
        : LocalFileBrowserWidget(errorHandler, parent)
    {
    }

    bool callConfirmDestructiveAction(const QString &title, const QString &message,
                                      const QString &acceptText,
                                      IMessagePresenter::MessageIcon icon)
    {
        return confirmDestructiveAction(title, message, acceptText, icon);
    }
};

class TestLocalFileBrowserWidget : public QObject
{
    Q_OBJECT

private:
    ErrorHandler *makeErrorHandler() { return new ErrorHandler(nullptr, this); }

private slots:

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        QVERIFY(true);
    }

    // =========================================================================
    // selectedPath() — no selection returns empty
    // =========================================================================

    void testSelectedPath_NoSelection_ReturnsEmpty()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        QVERIFY(widget.selectedPath().isEmpty());
    }

    // =========================================================================
    // isSelectedDirectory() — no selection returns false
    // =========================================================================

    void testIsSelectedDirectory_NoSelection_ReturnsFalse()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        QVERIFY(!widget.isSelectedDirectory());
    }

    // =========================================================================
    // selectedPaths() — no selection returns empty list
    // =========================================================================

    void testSelectedPaths_NoSelection_ReturnsEmpty()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        QVERIFY(widget.selectedPaths().isEmpty());
    }

    // =========================================================================
    // selectedEntries() — no selection returns empty list, consistent with selectedPaths()
    // =========================================================================

    void testSelectedEntries_NoSelection_ReturnsEmpty()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        auto entries = widget.selectedEntries();
        QVERIFY(entries.isEmpty());
        QVERIFY(widget.selectedPaths().isEmpty());
    }

    // =========================================================================
    // setCurrentDirectory() — updates accessor
    // =========================================================================

    void testSetCurrentDirectory_UpdatesCurrentDirectory()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        widget.setCurrentDirectory(homePath);
        QCOMPARE(widget.currentDirectory(), homePath);
    }

    // =========================================================================
    // setUploadEnabled() — does not crash
    // =========================================================================

    void testSetCurrentDirectory_DoesNotNarrateTheDestination()
    {
        ErrorHandler *handler = makeErrorHandler();
        LocalFileBrowserWidget widget(handler);
        QSignalSpy spy(handler, &ErrorHandler::statusMessage);

        widget.setCurrentDirectory(QStandardPaths::writableLocation(QStandardPaths::TempLocation));

        QCOMPARE(spy.count(), 0);  // the path badge already shows it
    }

    void testContextMenu_itemMenuOrder_setDestinationUploadThenFileActions()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        auto *menu = widget.findChild<QMenu *>(QStringLiteral("ItemMenu"));
        QVERIFY(menu != nullptr);

        QStringList texts;
        for (QAction *action : menu->actions()) {
            texts << (action->isSeparator() ? QStringLiteral("|") : action->text());
        }
        QCOMPARE(texts, (QStringList{"Set as Destination", "|", "Upload to C64U", "|", "New Folder",
                                     "Rename", "Delete"}));
    }

    void testContextMenu_emptySpaceMenu_offersNewFolderOnly()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        auto *menu = widget.findChild<QMenu *>(QStringLiteral("EmptySpaceMenu"));
        QVERIFY(menu != nullptr);

        QStringList texts;
        for (QAction *action : menu->actions()) {
            texts << action->text();
        }
        QCOMPARE(texts, QStringList{"New Folder"});
    }

    void testDoubleClick_onAFile_requestsItsUpload()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("tune.sid"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
        file.close();
        LocalFileBrowserWidget widget(makeErrorHandler());
        widget.setCurrentDirectory(dir.path());
        auto *tree = widget.findChild<QTreeView *>();
        QVERIFY(tree != nullptr);
        QModelIndex fileIndex;
        QTRY_VERIFY_WITH_TIMEOUT((tree->model()->rowCount(tree->rootIndex()) > 0), 5000);
        fileIndex = tree->model()->index(0, 0, tree->rootIndex());
        tree->setCurrentIndex(fileIndex);
        QSignalSpy spy(&widget, &LocalFileBrowserWidget::uploadRequested);

        QMetaObject::invokeMethod(&widget, "onDoubleClicked", Q_ARG(QModelIndex, fileIndex));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), dir.filePath("tune.sid"));
        QCOMPARE(spy.first().at(1).toBool(), false);
    }

    void testSetUploadEnabled_True_DoesNotCrash()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        widget.setUploadEnabled(true);
        QVERIFY(true);
    }

    void testSetUploadEnabled_False_DoesNotCrash()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        widget.setUploadEnabled(false);
        QVERIFY(true);
    }

    // =========================================================================
    // currentDirectory() — returns initial home directory
    // =========================================================================

    void testCurrentDirectory_Initial_ReturnsHomeLocation()
    {
        LocalFileBrowserWidget widget(makeErrorHandler());
        QString homePath = QStandardPaths::writableLocation(QStandardPaths::HomeLocation);
        QCOMPARE(widget.currentDirectory(), homePath);
    }

    // =========================================================================
    // confirmDestructiveAction — routes through injected IMessagePresenter
    // =========================================================================

    void testConfirmDestructiveAction_WhenAccepted_ReturnsTrue()
    {
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;  // index 0 = accept button
        widget.setMessagePresenter(&mock);

        const bool result = widget.callConfirmDestructiveAction(
            "Delete", "Are you sure?", "Delete", IMessagePresenter::MessageIcon::Warning);

        QVERIFY(result);
        QCOMPARE(mock.confirmCalls.size(), 1);
    }

    void testConfirmDestructiveAction_WhenCancelled_ReturnsFalse()
    {
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        MockMessagePresenter mock;
        mock.nextConfirmResult = 1;  // index 1 = Cancel button
        widget.setMessagePresenter(&mock);

        const bool result = widget.callConfirmDestructiveAction(
            "Delete", "Are you sure?", "Delete", IMessagePresenter::MessageIcon::Warning);

        QVERIFY(!result);
    }

    void testConfirmDestructiveAction_WhenDismissed_ReturnsFalse()
    {
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        MockMessagePresenter mock;
        mock.nextConfirmResult = -1;  // dialog dismissed without button click
        widget.setMessagePresenter(&mock);

        const bool result = widget.callConfirmDestructiveAction(
            "Delete", "Are you sure?", "Delete", IMessagePresenter::MessageIcon::Warning);

        QVERIFY(!result);
    }

    void testConfirmDestructiveAction_DefaultButtonIsCancel()
    {
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        MockMessagePresenter mock;
        widget.setMessagePresenter(&mock);

        widget.callConfirmDestructiveAction("Delete", "Are you sure?", "Delete",
                                            IMessagePresenter::MessageIcon::Warning);

        QCOMPARE(mock.confirmCalls.size(), 1);
        const ConfirmCall &call = mock.confirmCalls[0];
        QVERIFY(call.defaultIndex >= 0 && call.defaultIndex < call.buttons.size());
        QCOMPARE(call.buttons[call.defaultIndex].role, IMessagePresenter::ButtonRole::Reject);
        QVERIFY2(call.buttons[call.defaultIndex].role != IMessagePresenter::ButtonRole::Destructive,
                 "Enter must never trigger the destructive action");
    }

    void testConfirmDestructiveAction_PassesTitleAndMessage()
    {
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        MockMessagePresenter mock;
        mock.nextConfirmResult = 0;
        widget.setMessagePresenter(&mock);

        widget.callConfirmDestructiveAction("My Title", "My Message", "OK",
                                            IMessagePresenter::MessageIcon::Warning);

        QCOMPARE(mock.confirmCalls[0].title, QString("My Title"));
        QCOMPARE(mock.confirmCalls[0].message, QString("My Message"));
    }
};

QTEST_MAIN(TestLocalFileBrowserWidget)
#include "test_localfilebrowserwidget.moc"
