#ifndef FILEBROWSERWIDGET_H
#define FILEBROWSERWIDGET_H

#include "ui/imessagepresenter.h"
#include "ui/qmessageboxpresenter.h"

#include <QList>
#include <QString>
#include <QWidget>

class QTreeView;
class QToolBar;
class QMenu;
class QAction;
class PathNavigationWidget;
class QAbstractItemModel;
class QModelIndex;
class ErrorHandler;

/**
 * @brief Abstract base class for file browser widgets.
 *
 * This class provides the common structure for both LocalFileBrowserWidget
 * and RemoteFileBrowserWidget, including:
 * - Toolbar with common actions
 * - Tree view for file listing
 * - Path navigation widget
 * - Context menu
 * - Common signals and slots
 *
 * Subclasses must implement model-specific operations and customize
 * the toolbar with their specific actions (upload/download).
 */
class FileBrowserWidget : public QWidget
{
    Q_OBJECT

public:
    /**
     * @brief Constructs a file browser widget.
     * @param errorHandler Error handler for routing status messages (required, not owned).
     * @param parent Optional parent widget.
     */
    explicit FileBrowserWidget(ErrorHandler *errorHandler, QWidget *parent = nullptr);

    /**
     * @brief Returns the current directory path.
     * @return The current directory path.
     */
    [[nodiscard]] QString currentDirectory() const { return currentDirectory_; }

    /**
     * @brief Replaces the message presenter used for destructive-action confirmations.
     *
     * The default presenter shows real QMessageBox dialogs. Inject a test
     * double to verify confirmation behaviour without blocking the UI.
     *
     * @param presenter Non-owning pointer; must outlive this widget. Pass nullptr to
     *                  restore the default QMessageBoxPresenter.
     */
    void setMessagePresenter(IMessagePresenter *presenter);

    /**
     * @brief Returns the path of the selected item.
     * @return The selected path, or empty string if no selection.
     */
    [[nodiscard]] virtual QString selectedPath() const = 0;

    /**
     * @brief A selected file system entry with its path and directory flag.
     */
    struct SelectedEntry
    {
        QString path;
        bool isDirectory;
    };

    /**
     * @brief Returns all selected items with their path and directory flag.
     *
     * Filters to column 0 only and deduplicates, using the virtual filePath()
     * and isDirectory() methods to populate each entry.
     *
     * @return List of selected entries.
     */
    [[nodiscard]] QList<SelectedEntry> selectedEntries() const;

    /**
     * @brief Returns the paths of all selected items.
     *
     * Delegates to selectedEntries() for a consistent single selection-walk.
     *
     * @return List of selected paths.
     */
    [[nodiscard]] QStringList selectedPaths() const;

    /**
     * @brief Returns whether the selected item is a directory.
     * @return true if selected item is a directory, false otherwise.
     */
    [[nodiscard]] virtual bool isSelectedDirectory() const = 0;

signals:
    /**
     * @brief Emitted when the current directory changes.
     * @param path The new directory path.
     */
    void currentDirectoryChanged(const QString &path);

    /**
     * @brief Emitted when the selection changes.
     */
    void selectionChanged();

public slots:
    /**
     * @brief Sets the current directory.
     * @param path The directory path to navigate to.
     */
    virtual void setCurrentDirectory(const QString &path);

protected slots:
    /**
     * @brief Handles double-click on an item.
     * @param index The model index that was double-clicked.
     */
    void onDoubleClicked(const QModelIndex &index);

    /**
     * @brief Handles context menu request.
     * @param pos The position where context menu was requested.
     */
    virtual void onContextMenu(const QPoint &pos);

    /**
     * @brief Navigates to the parent folder.
     */
    virtual void onParentFolder();

    /**
     * @brief Creates a new folder in the current directory.
     */
    virtual void onNewFolder();

    /**
     * @brief Renames the selected item.
     */
    virtual void onRename();

    /**
     * @brief Deletes the selected item.
     */
    virtual void onDelete();

protected:
    /**
     * @brief Sets up the UI components.
     *
     * Creates the label, navigation widget, toolbar, and tree view.
     * Subclasses should override this to customize the UI.
     */
    virtual void setupUi();

    /**
     * @brief Sets up the context menu.
     *
     * Creates the context menu with "Set as Destination" action.
     * Subclasses should override this to add custom menu items.
     */
    virtual void setupContextMenu();

    /**
     * @brief Sets up signal/slot connections.
     *
     * Connects tree view signals and other UI elements.
     * Subclasses should override this to add custom connections.
     */
    virtual void setupConnections();

    /**
     * @brief Updates the enabled state of actions based on selection.
     *
     * Subclasses must implement this to enable/disable their actions.
     */
    virtual void updateActions() = 0;

    /**
     * @brief Updates common action states shared by all subclasses.
     *
     * Sets the enabled state of newFolderAction_, renameAction_, and
     * deleteAction_ based on the current selection and an optional extra
     * condition (e.g., connection state for remote panels).
     *
     * @param extraCondition Additional condition that must be true to enable actions.
     *                       Defaults to true (always allow for local panels).
     */
    void updateCommonActions(bool extraCondition = true);

    /**
     * @brief Shows a destructive-confirmation dialog and returns whether the user accepted.
     * @param title Window title (e.g. "Delete", "Move to Trash").
     * @param message Body text asking the user to confirm.
     * @param acceptText Label on the accept button (e.g. "Delete", "Move to Trash").
     * @param icon Icon severity shown in the dialog.
     * @return true if the user clicked the accept button, false otherwise.
     */
    bool confirmDestructiveAction(const QString &title, const QString &message,
                                  const QString &acceptText, IMessagePresenter::MessageIcon icon);

    /**
     * @brief Shows an input dialog for a new name and validates it.
     *
     * Returns empty string if the user cancels, leaves the name unchanged,
     * or enters a name containing '/' or '\\'.
     *
     * @param title Dialog window title (e.g. "Rename folder").
     * @param oldName Pre-filled current name.
     * @return The validated new name, or an empty string.
     */
    [[nodiscard]] QString promptForNewName(const QString &title, const QString &oldName) const;

    /**
     * @brief Returns the label text for this browser.
     * @return The label text (e.g., "Local Files" or "C64U Files").
     */
    [[nodiscard]] virtual QString labelText() const = 0;

    /**
     * @brief Returns the navigation widget label text.
     * @return The nav widget label (e.g., "Download to:" or "Upload to:").
     */
    [[nodiscard]] virtual QString navLabelText() const = 0;

    /**
     * @brief Returns the model for the tree view.
     * @return Pointer to the model.
     */
    [[nodiscard]] virtual QAbstractItemModel *model() const = 0;

    /**
     * @brief Returns the file path for a model index.
     * @param index The model index.
     * @return The file path.
     */
    [[nodiscard]] virtual QString filePath(const QModelIndex &index) const = 0;

    /**
     * @brief Returns whether a model index represents a directory.
     * @param index The model index.
     * @return true if directory, false otherwise.
     */
    [[nodiscard]] virtual bool isDirectory(const QModelIndex &index) const = 0;

    /**
     * @brief Navigates to a directory.
     * @param path The directory path to navigate to.
     */
    virtual void navigateToDirectory(const QString &path) = 0;

    /**
     * @brief Called by onNewFolder() after dialog interaction to perform the actual creation.
     * @param folderName The validated folder name entered by the user.
     */
    virtual void performNewFolder(const QString &folderName) = 0;

    /**
     * @brief Called by onRename() after dialog interaction to perform the actual rename.
     * @param path The current file path.
     * @param newName The validated new name entered by the user.
     */
    virtual void performRename(const QString &path, const QString &newName) = 0;

    /**
     * @brief Called by onDelete() after confirmation to perform the actual deletion.
     * @param entries The selected entries to delete.
     */
    virtual void performDelete(const QList<SelectedEntry> &entries) = 0;

    /**
     * @brief Returns false and emits an error when a precondition is not met.
     *
     * The default implementation always returns true.  RemoteFileBrowserWidget
     * overrides this to check the connection state.
     *
     * @param actionLabel Short label for the action (e.g. "create folder").
     * @return true if the action may proceed.
     */
    virtual bool canModify(const QString &actionLabel);

    /**
     * @brief Returns the verb phrase used in the delete confirmation message.
     * Default: "delete". LocalFileBrowserWidget overrides to "move to the trash".
     */
    [[nodiscard]] virtual QString deleteVerbPhrase() const;

    /**
     * @brief Returns the label for the delete action (used as dialog title and button text).
     * Default: "Delete". LocalFileBrowserWidget overrides to "Move to Trash".
     */
    [[nodiscard]] virtual QString deleteActionLabel() const;

    /**
     * @brief Returns the icon to show in the delete confirmation dialog.
     * Default: MessageIcon::Warning. LocalFileBrowserWidget overrides to MessageIcon::Question.
     */
    [[nodiscard]] virtual IMessagePresenter::MessageIcon deleteIcon() const;

    // State
    QString currentDirectory_;

    // Error handler (not owned)
    ErrorHandler *errorHandler_;

    // Message presenter — owned default, swappable for tests (non-owning pointer).
    QMessageBoxPresenter defaultPresenter_;
    IMessagePresenter *presenter_ = &defaultPresenter_;

    // UI widgets (protected so subclasses can access)
    QTreeView *treeView_ = nullptr;
    QToolBar *toolBar_ = nullptr;
    PathNavigationWidget *navWidget_ = nullptr;
    QMenu *contextMenu_ = nullptr;

    // Common actions
    QAction *newFolderAction_ = nullptr;
    QAction *renameAction_ = nullptr;
    QAction *deleteAction_ = nullptr;
    QAction *setDestAction_ = nullptr;
};

#endif  // FILEBROWSERWIDGET_H
