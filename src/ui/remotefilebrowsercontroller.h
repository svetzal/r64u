#ifndef REMOTEFILEBROWSERCONTROLLER_H
#define REMOTEFILEBROWSERCONTROLLER_H

#include <QObject>
#include <QStringList>

/**
 * @brief Encapsulates the business logic for remote file browser operations.
 *
 * This controller handles path construction, name validation, and confirmation
 * message building for RemoteFileBrowserWidget operations. It has no dependency
 * on Qt dialog classes — the widget is responsible for showing actual dialogs
 * and passing user input to the controller.
 */
class RemoteFileBrowserController : public QObject
{
    Q_OBJECT

public:
    explicit RemoteFileBrowserController(QObject *parent = nullptr);

    /**
     * @brief Builds the full remote path for a new folder.
     * @param currentDirectory The current remote directory.
     * @param folderName The folder name entered by the user.
     * @return The full remote path, or an empty string if inputs are invalid.
     */
    [[nodiscard]] QString buildNewFolderPath(const QString &currentDirectory,
                                             const QString &folderName) const;

    /**
     * @brief Builds the full remote path after a rename.
     * @param oldPath The current full remote path.
     * @param newName The new name entered by the user.
     * @return The new full remote path, or an empty string if the name is invalid.
     */
    [[nodiscard]] QString buildRenamePath(const QString &oldPath, const QString &newName) const;

    /**
     * @brief Validates that a name does not contain path separator characters.
     * @param name The name to validate.
     * @return True if the name is valid (no '/' or '\\').
     */
    [[nodiscard]] static bool isValidName(const QString &name);

    /**
     * @brief Builds a confirmation message for a delete operation.
     * @param paths The list of remote paths to delete.
     * @param isDirectory True if the single selection is a directory.
     * @return The confirmation message string.
     */
    [[nodiscard]] QString buildDeleteConfirmMessage(const QStringList &paths,
                                                    bool isDirectory) const;
};

#endif  // REMOTEFILEBROWSERCONTROLLER_H
