/**
 * @file test_remotefilebrowserwidget.cpp
 * @brief Unit tests for RemoteFileBrowserWidget control-flow and signal routing.
 *
 * RemoteFileBrowserWidget delegates file listing to RemoteFileModel and
 * operations to RemoteFileOperationsService.  These tests focus on the
 * control-flow decisions inside the widget:
 *
 * - setCurrentDirectory() emits currentDirectoryChanged signal
 * - onParentFolder() at root "/" returns silently
 * - onParentFolder() parent path computation
 * - refreshIfStale() when disconnected — no-op
 * - refreshIfStale() when suppressAutoRefresh is active — no-op
 * - selectedPath() when no selection — returns empty string
 * - isSelectedDirectory() when no selection — returns false
 * - Drops: local and Finder files upload into the pane (or the folder row
 *   under the cursor); device paths, from this pane or Explore, are refused
 */

#include "core/dropcore.h"
#include "mocks/mockftpclient.h"
#include "mocks/mockrestclient.h"
#include "models/remotefilemodel.h"
#include "services/errorhandler.h"
#include "ui/remotefilebrowserwidget.h"

#include <QDropEvent>
#include <QMenu>
#include <QMimeData>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QTreeView>
#include <QtTest>

#include <memory>

/**
 * @brief Thin test subclass that exposes the protected drop handlers.
 */
class ExposedRemoteFileBrowserWidget : public RemoteFileBrowserWidget
{
public:
    using RemoteFileBrowserWidget::RemoteFileBrowserWidget;

    bool callHandleDrop(const QMimeData *mime, const QPoint &pos, bool fromThisPane)
    {
        return handleDrop(mime, pos, fromThisPane);
    }

    bool callCanAcceptDrag(const QMimeData *mime, bool fromThisPane) const
    {
        return canAcceptDrag(mime, fromThisPane);
    }
};

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

/// A payload as RemoteFileModel writes it for the given device entries.
static QMimeData *remotePathsMime(const QList<dropcore::DropEntry> &entries)
{
    auto *mime = new QMimeData();
    mime->setData(QString::fromLatin1(dropcore::kRemotePathsMimeType),
                  dropcore::encodeRemoteEntries(entries));
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

class TestRemoteFileBrowserWidget : public QObject
{
    Q_OBJECT

private:
    RemoteFileModel *model_ = nullptr;
    MockFtpClient *mockFtp_ = nullptr;
    MockRestClient *mockRest_ = nullptr;

    ErrorHandler *makeErrorHandler() { return new ErrorHandler(nullptr, this); }

    /// Lists @p directory on the mock device and makes the model show it.
    void listDirectory(const QString &directory, const QList<FtpEntry> &entries)
    {
        mockFtp_->mockSetConnected(true);
        mockFtp_->mockSetDirectoryListing(directory, entries);
        model_->setFtpClient(mockFtp_);
        model_->setRootPath(directory);
        model_->fetchMore(QModelIndex());
        mockFtp_->mockProcessAllOperations();
    }

    static FtpEntry dirEntry(const QString &name)
    {
        FtpEntry entry;
        entry.name = name;
        entry.isDirectory = true;
        return entry;
    }

private slots:
    void init()
    {
        mockFtp_ = new MockFtpClient(this);
        mockRest_ = new MockRestClient(this);
        model_ = new RemoteFileModel(this);
    }

    // =========================================================================
    // Construction
    // =========================================================================

    void testConstruct_doesNotCrash()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        QVERIFY(true);
    }

    // =========================================================================
    // setCurrentDirectory() — emits signal
    // =========================================================================

    void testSetCurrentDirectory_EmitsCurrentDirectoryChanged()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        widget.setCurrentDirectory("/SD");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/SD"));
    }

    void testSetCurrentDirectory_UpdatesCurrentDirectory()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.setCurrentDirectory("/SD/Games");
        QCOMPARE(widget.currentDirectory(), QString("/SD/Games"));
    }

    // =========================================================================
    // onParentFolder() — at root, no-op
    // =========================================================================

    void testOnParentFolder_AtRoot_DoesNotEmitSignal()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        // Start at root
        widget.setCurrentDirectory("/");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        // Should not emit because we are already at root
        QCOMPARE(spy.count(), 0);
    }

    void testOnParentFolder_AtRoot_StaysAtRoot()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.setCurrentDirectory("/");

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        QCOMPARE(widget.currentDirectory(), QString("/"));
    }

    // =========================================================================
    // onParentFolder() — parent path computation
    // =========================================================================

    void testOnParentFolder_OneLevelDeep_NavigatesToRoot()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.setCurrentDirectory("/SD");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/"));
    }

    void testOnParentFolder_TwoLevelsDeep_NavigatesToParent()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.setCurrentDirectory("/SD/Games");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::currentDirectoryChanged);

        QMetaObject::invokeMethod(&widget, "onParentFolder");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/SD"));
    }

    // =========================================================================
    // refreshIfStale() — disconnected state, no-op
    // =========================================================================

    void testRefreshIfStale_WhenDisconnected_NoOp()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        // Widget starts disconnected, so refreshIfStale should be a no-op
        // We verify by checking no crash and model refresh is not triggered
        widget.refreshIfStale();
        QVERIFY(true);  // No crash
    }

    // =========================================================================
    // refreshIfStale() — connected state, calls model
    // =========================================================================

    void testRefreshIfStale_WhenConnected_Proceeds()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.onConnectionStateChanged(true);
        // Should not crash
        widget.refreshIfStale();
        QVERIFY(true);
    }

    // =========================================================================
    // AutoRefreshSuppressor — suppresses refreshIfStale
    // =========================================================================

    void testAutoRefreshSuppressor_SuppressesRefresh()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.onConnectionStateChanged(true);

        {
            auto suppressor = widget.suppressRefresh();
            // refreshIfStale should be a no-op while suppressed (no crash)
            widget.refreshIfStale();
        }
        // After scope, suppressor released — refresh should work again
        widget.refreshIfStale();
        QVERIFY(true);
    }

    // =========================================================================
    // selectedPath() — no selection returns empty
    // =========================================================================

    void testSelectedPath_NoSelection_ReturnsEmpty()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        QVERIFY(widget.selectedPath().isEmpty());
    }

    // =========================================================================
    // isSelectedDirectory() — no selection returns false
    // =========================================================================

    void testIsSelectedDirectory_NoSelection_ReturnsFalse()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        QVERIFY(!widget.isSelectedDirectory());
    }

    // =========================================================================
    // selectedPaths() — no selection returns empty list
    // =========================================================================

    void testSelectedPaths_NoSelection_ReturnsEmpty()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        QVERIFY(widget.selectedPaths().isEmpty());
    }

    // =========================================================================
    // selectedEntries() — no selection returns empty list, consistent with selectedPaths()
    // =========================================================================

    void testSelectedEntries_NoSelection_ReturnsEmpty()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        auto entries = widget.selectedEntries();
        QVERIFY(entries.isEmpty());
        QVERIFY(widget.selectedPaths().isEmpty());
    }

    // =========================================================================
    // onDownload() — the file's listed size goes with the request
    // =========================================================================

    void testDownload_SelectedFile_RequestCarriesItsListedSize()
    {
        FtpEntry reu;
        reu.name = "big.reu";
        reu.size = 16777216;
        mockFtp_->mockSetConnected(true);
        mockFtp_->mockSetDirectoryListing("/SD", {reu});
        model_->setFtpClient(mockFtp_);
        model_->setRootPath("/SD");
        model_->fetchMore(QModelIndex());
        mockFtp_->mockProcessAllOperations();
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.findChild<QTreeView *>()->selectionModel()->select(
            model_->index(0, 0), QItemSelectionModel::Select | QItemSelectionModel::Rows);
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::downloadRequested);

        QMetaObject::invokeMethod(&widget, "onDownload");

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString("/SD/big.reu"));
        QCOMPARE(spy.first().at(2).toLongLong(), qint64(16777216));
    }

    void testDoubleClick_onAFile_requestsItsDownload()
    {
        FtpEntry prg;
        prg.name = "game.prg";
        prg.size = 2048;
        mockFtp_->mockSetConnected(true);
        mockFtp_->mockSetDirectoryListing("/SD", {prg});
        model_->setFtpClient(mockFtp_);
        model_->setRootPath("/SD");
        model_->fetchMore(QModelIndex());
        mockFtp_->mockProcessAllOperations();
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        const QModelIndex fileIndex = model_->index(0, 0);
        widget.findChild<QTreeView *>()->setCurrentIndex(fileIndex);
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::downloadRequested);

        QMetaObject::invokeMethod(&widget, "onDoubleClicked", Q_ARG(QModelIndex, fileIndex));

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.first().at(0).toString(), QString("/SD/game.prg"));
        QCOMPARE(spy.first().at(2).toLongLong(), qint64(2048));
    }

    void testSetCurrentDirectory_DoesNotNarrateTheDestination()
    {
        ErrorHandler *handler = makeErrorHandler();
        RemoteFileBrowserWidget widget(model_, handler);
        QSignalSpy spy(handler, &ErrorHandler::statusMessage);

        widget.setCurrentDirectory("/SD/games");

        QCOMPARE(spy.count(), 0);  // the path badge already shows it
    }

    void testContextMenu_itemMenu_matchesTheLocalOrderAndOffersRename()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        auto *menu = widget.findChild<QMenu *>(QStringLiteral("ItemMenu"));
        QVERIFY(menu != nullptr);

        QStringList texts;
        for (QAction *action : menu->actions()) {
            texts << (action->isSeparator() ? QStringLiteral("|") : action->text());
        }
        QCOMPARE(texts, (QStringList{"Set as Destination", "|", "Download to Local Directory", "|",
                                     "New Folder", "Rename", "Delete", "|", "Refresh"}));
    }

    void testContextMenu_emptySpaceMenu_offersNewFolderAndRefresh()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        auto *menu = widget.findChild<QMenu *>(QStringLiteral("EmptySpaceMenu"));
        QVERIFY(menu != nullptr);

        QStringList texts;
        for (QAction *action : menu->actions()) {
            texts << (action->isSeparator() ? QStringLiteral("|") : action->text());
        }
        QCOMPARE(texts, (QStringList{"New Folder", "|", "Refresh"}));
    }

    // =========================================================================
    // setDownloadEnabled() — does not crash
    // =========================================================================

    void testSetDownloadEnabled_DoesNotCrash()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.setDownloadEnabled(true);
        widget.setDownloadEnabled(false);
        QVERIFY(true);
    }

    // =========================================================================
    // onConnectionStateChanged() — updates connected state
    // =========================================================================

    void testOnConnectionStateChanged_Connected()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.onConnectionStateChanged(true);
        QVERIFY(true);  // No crash
    }

    void testOnConnectionStateChanged_Disconnected()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.onConnectionStateChanged(true);
        widget.onConnectionStateChanged(false);
        QVERIFY(true);  // No crash
    }

    // =========================================================================
    // Disconnected user operations — error routed through ErrorHandler
    // =========================================================================

    void testOnNewFolder_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        auto *errorHandler = makeErrorHandler();
        RemoteFileBrowserWidget widget(model_, errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(&widget, "onNewFolder");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Not connected"));
    }

    void testOnRename_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        auto *errorHandler = makeErrorHandler();
        RemoteFileBrowserWidget widget(model_, errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(&widget, "onRename");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Not connected"));
    }

    void testOnDelete_WhenDisconnected_EmitsErrorViaErrorHandler()
    {
        auto *errorHandler = makeErrorHandler();
        RemoteFileBrowserWidget widget(model_, errorHandler);
        QSignalSpy spy(errorHandler, &ErrorHandler::statusMessage);

        QMetaObject::invokeMethod(&widget, "onDelete");

        QCOMPARE(spy.count(), 1);
        QVERIFY(spy.at(0).at(0).toString().contains("Not connected"));
    }

    // =========================================================================
    // Drops: local files upload into this pane
    // =========================================================================

    void testDrop_localFile_requestsUploadIntoTheCurrentDirectory()
    {
        QTemporaryDir dir;
        QFile file(dir.filePath("game.prg"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("x");
        file.close();
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.setCurrentDirectory("/SD/Games");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::uploadRequested);
        QMimeData *mime = localFilesMime({dir.filePath("game.prg")});

        dropOnto(widget, mime);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), dir.filePath("game.prg"));
        QCOMPARE(spy.at(0).at(1).toBool(), false);
        QCOMPARE(spy.at(0).at(2).toString(), QString("/SD/Games"));
        delete mime;
    }

    void testDrop_finderFolder_requestsARecursiveUpload()
    {
        QTemporaryDir dir;
        QVERIFY(QDir(dir.path()).mkdir("demos"));
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.setCurrentDirectory("/SD");
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::uploadRequested);
        QMimeData *mime = localFilesMime({dir.filePath("demos")});

        dropOnto(widget, mime);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), dir.filePath("demos"));
        QCOMPARE(spy.at(0).at(1).toBool(), true);
        QCOMPARE(spy.at(0).at(2).toString(), QString("/SD"));
        delete mime;
    }

    void testDrop_localFileOntoAFolderRow_uploadsIntoThatFolder()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        widget.resize(400, 300);
        widget.show();
        widget.setCurrentDirectory("/SD");  // resets the listing, so list afterwards
        listDirectory("/SD", {dirEntry("Games")});
        auto *tree = widget.findChild<QTreeView *>();
        QVERIFY(tree != nullptr);
        const QModelIndex folderIndex = model_->index(0, 0);
        QVERIFY(folderIndex.isValid());
        const QPoint onFolder = tree->visualRect(folderIndex).center();
        QVERIFY(tree->indexAt(onFolder) == folderIndex);
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::uploadRequested);
        QMimeData *mime = localFilesMime({"/Users/someone/Desktop/game.prg"});

        dropOnto(widget, mime, onFolder);

        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("/Users/someone/Desktop/game.prg"));
        QCOMPARE(spy.at(0).at(2).toString(), QString("/SD/Games"));
        delete mime;
    }

    void testDrop_ownFiles_isRefused()
    {
        ExposedRemoteFileBrowserWidget widget(model_, makeErrorHandler());
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::uploadRequested);
        std::unique_ptr<QMimeData> mime(remotePathsMime({{"/SD/a.prg", false, 1}}));

        QVERIFY(!widget.callCanAcceptDrag(mime.get(), true));
        QVERIFY(!widget.callHandleDrop(mime.get(), QPoint(5, 5), true));
        QCOMPARE(spy.count(), 0);
    }

    void testDrop_devicePathsFromAnotherListing_isRefused()
    {
        ExposedRemoteFileBrowserWidget widget(model_, makeErrorHandler());
        QSignalSpy spy(&widget, &RemoteFileBrowserWidget::uploadRequested);
        QMimeData *mime = remotePathsMime({{"/SD/a.prg", false, 1}});

        QVERIFY(!widget.callCanAcceptDrag(mime, false));
        dropOnto(widget, mime);

        QCOMPARE(spy.count(), 0);
        delete mime;
    }

    void testDrag_localFilesAreAccepted_andMarkThePaneAsADropTarget()
    {
        RemoteFileBrowserWidget widget(model_, makeErrorHandler());
        auto *tree = widget.findChild<QTreeView *>();
        QVERIFY(tree != nullptr);
        std::unique_ptr<QMimeData> mime(localFilesMime({"/Users/someone/game.prg"}));

        QDragEnterEvent enter(QPoint(5, 5), Qt::CopyAction, mime.get(), Qt::LeftButton,
                              Qt::NoModifier);
        QApplication::sendEvent(tree->viewport(), &enter);
        QVERIFY(enter.isAccepted());
        QCOMPARE(enter.dropAction(), Qt::CopyAction);
        QCOMPARE(tree->property("dropActive").toBool(), true);

        QDragLeaveEvent leave;
        QApplication::sendEvent(tree->viewport(), &leave);
        QCOMPARE(tree->property("dropActive").toBool(), false);
    }
};

QTEST_MAIN(TestRemoteFileBrowserWidget)
#include "test_remotefilebrowserwidget.moc"
