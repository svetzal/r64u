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
 * - Drops: remote paths download into the pane (or the folder row under the
 *   cursor); the pane's own files and Finder files are refused
 */

#include "core/dropcore.h"
#include "mocks/mockmessagepresenter.h"
#include "services/errorhandler.h"
#include "ui/localfilebrowserwidget.h"

#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QSignalSpy>
#include <QStandardPaths>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

#include <memory>

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

    bool callHandleDrop(const QMimeData *mime, const QPoint &pos, bool fromThisPane)
    {
        return handleDrop(mime, pos, fromThisPane);
    }

    bool callCanAcceptDrag(const QMimeData *mime, bool fromThisPane) const
    {
        return canAcceptDrag(mime, fromThisPane);
    }
};

/// A payload as RemoteFileModel writes it for the given device entries.
static QMimeData *remotePathsMime(const QList<dropcore::DropEntry> &entries)
{
    auto *mime = new QMimeData();
    mime->setData(QString::fromLatin1(dropcore::kRemotePathsMimeType),
                  dropcore::encodeRemoteEntries(entries));
    return mime;
}

/// A payload as the Finder (or the local pane) writes it for local files.
static QMimeData *localFilesMime(const QStringList &paths)
{
    auto *mime = new QMimeData();
    QList<QUrl> urls;
    for (const QString &path : paths) {
        urls.append(QUrl::fromLocalFile(path));
    }
    mime->setUrls(urls);
    return mime;
}

/// Delivers a drag of @p mime entering the tree view's viewport and dropping at @p pos, as Qt
/// would.
static void dropOnto(QWidget &widget, QMimeData *mime, const QPoint &pos = QPoint(5, 5))
{
    auto *tree = widget.findChild<QTreeView *>();
    QVERIFY(tree != nullptr);
    QDragEnterEvent enter(pos, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &enter);
    QDropEvent drop(pos, Qt::CopyAction, mime, Qt::LeftButton, Qt::NoModifier);
    QApplication::sendEvent(tree->viewport(), &drop);
}

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

    // =========================================================================
    // Drops: remote paths download into this pane
    // =========================================================================

    void testDrop_remotePaths_requestsDownloadsIntoTheCurrentDirectory()
    {
        QTemporaryDir dir;
        LocalFileBrowserWidget widget(makeErrorHandler());
        widget.setCurrentDirectory(dir.path());
        QSignalSpy spy(&widget, &LocalFileBrowserWidget::downloadRequested);
        QMimeData *mime =
            remotePathsMime({{"/SD/Games", true, 0}, {"/SD/big.reu", false, 16777216}});

        dropOnto(widget, mime);

        QCOMPARE(spy.count(), 2);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/SD/Games"));
        QCOMPARE(spy.at(0).at(1).toBool(), true);
        QCOMPARE(spy.at(0).at(3).toString(), dir.path());
        QCOMPARE(spy.at(1).at(0).toString(), QString("/SD/big.reu"));
        QCOMPARE(spy.at(1).at(1).toBool(), false);
        QCOMPARE(spy.at(1).at(2).toLongLong(), qint64(16777216));
        QCOMPARE(spy.at(1).at(3).toString(), dir.path());
        delete mime;
    }

    void testDrop_remotePathOntoAFolderRow_downloadsIntoThatFolder()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir("music"));
        LocalFileBrowserWidget widget(makeErrorHandler());
        widget.resize(400, 300);
        widget.show();
        widget.setCurrentDirectory(dir.path());
        auto *tree = widget.findChild<QTreeView *>();
        QVERIFY(tree != nullptr);
        QTRY_VERIFY_WITH_TIMEOUT((tree->model()->rowCount(tree->rootIndex()) > 0), 5000);
        const QModelIndex folderIndex = tree->model()->index(0, 0, tree->rootIndex());
        QVERIFY(folderIndex.isValid());
        const QPoint onFolder = tree->visualRect(folderIndex).center();
        QVERIFY(tree->indexAt(onFolder) == folderIndex);
        QSignalSpy spy(&widget, &LocalFileBrowserWidget::downloadRequested);
        QMimeData *mime = remotePathsMime({{"/SD/tune.sid", false, 4096}});

        dropOnto(widget, mime, onFolder);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/SD/tune.sid"));
        QCOMPARE(spy.at(0).at(3).toString(), dir.filePath("music"));
        delete mime;
    }

    void testDrop_ownFiles_isRefused()
    {
        QTemporaryDir dir;
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        widget.setCurrentDirectory(dir.path());
        QSignalSpy spy(&widget, &LocalFileBrowserWidget::downloadRequested);
        std::unique_ptr<QMimeData> mime(localFilesMime({dir.filePath("a.prg")}));

        QVERIFY(!widget.callCanAcceptDrag(mime.get(), true));
        QVERIFY(!widget.callHandleDrop(mime.get(), QPoint(5, 5), true));
        QCOMPARE(spy.count(), 0);
    }

    void testDrop_finderFiles_isRefused()
    {
        QTemporaryDir dir;
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        widget.setCurrentDirectory(dir.path());
        QSignalSpy spy(&widget, &LocalFileBrowserWidget::downloadRequested);
        QMimeData *mime = localFilesMime({"/Users/someone/Desktop/game.prg"});

        QVERIFY(!widget.callCanAcceptDrag(mime, false));
        dropOnto(widget, mime);

        QCOMPARE(spy.count(), 0);
        delete mime;
    }

    void testDrag_remotePathsAreAccepted_andMarkThePaneAsADropTarget()
    {
        ExposedLocalFileBrowserWidget widget(makeErrorHandler());
        auto *tree = widget.findChild<QTreeView *>();
        QVERIFY(tree != nullptr);
        std::unique_ptr<QMimeData> mime(remotePathsMime({{"/SD/tune.sid", false, 1}}));
        QVERIFY(widget.callCanAcceptDrag(mime.get(), false));

        QDragEnterEvent enter(QPoint(5, 5), Qt::CopyAction, mime.get(), Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &enter);
        QVERIFY(enter.isAccepted());
        QCOMPARE(tree->property("dropActive").toBool(), true);

        QDragLeaveEvent leave;
        QApplication::sendEvent(tree->viewport(), &leave);
        QCOMPARE(tree->property("dropActive").toBool(), false);
    }
};

QTEST_MAIN(TestLocalFileBrowserWidget)
#include "test_localfilebrowserwidget.moc"
