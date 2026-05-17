#ifndef FILEBROWSERWIDGET_H
#define FILEBROWSERWIDGET_H

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
     * @param parent Optional parent widget.
     */
    explicit FileBrowserWidget(QWidget *parent = nullptr);

    /**
     * @brief Returns the current directory path.
     * @return The current directory path.
     */
    [[nodiscard]] QString currentDirectory() const { return currentDirectory_; }

    /**
     * @brief Sets the error handler for routing status messages.
     * @param handler The error handler instance (not owned).
     */
    void setErrorHandler(ErrorHandler *handler);

    /**
     * @brief Returns the path of the selected item.
     * @return The selected path, or empty string if no selection.
     */
    [[nodiscard]] virtual QString selectedPath() const = 0;

    /**
     * @brief Returns the paths of all selected items.
     *
     * Filters to column 0 only and deduplicates, using the virtual filePath()
     * method to map model indexes to paths.
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
    virtual void onNewFolder() = 0;

    /**
     * @brief Renames the selected item.
     */
    virtual void onRename() = 0;

    /**
     * @brief Deletes the selected item.
     */
    virtual void onDelete() = 0;

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

    // State
    QString currentDirectory_;

    // Error handler (not owned)
    ErrorHandler *errorHandler_ = nullptr;

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
