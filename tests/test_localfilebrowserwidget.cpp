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

#include <QStandardPaths>
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
