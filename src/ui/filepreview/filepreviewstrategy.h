/**
 * @file filepreviewstrategy.h
 * @brief Abstract strategy interface for file preview rendering.
 *
 * Defines the interface for file preview strategies that handle
 * format detection and content rendering for different file types.
 */

#ifndef FILEPREVIEWSTRATEGY_H
#define FILEPREVIEWSTRATEGY_H

#include <QString>
#include <QByteArray>
#include <QWidget>

/**
 * @brief Abstract interface for file preview strategies.
 *
 * Each strategy handles a specific file type (text, HTML, disk images, SID files)
 * and provides methods to:
 * - Detect if it can handle a file based on path/extension
 * - Create the appropriate widget for displaying the content
 * - Render the content into the widget
 * - Clear the widget state
 */
class FilePreviewStrategy
{
public:
    virtual ~FilePreviewStrategy() = default;

    /**
     * @brief Returns whether this strategy can handle the given file.
     * @param path The file path (used for extension checking).
     * @return true if this strategy can preview the file, false otherwise.
     */
    [[nodiscard]] virtual bool canHandle(const QString &path) const = 0;

    /**
     * @brief Creates the widget used for displaying preview content.
     * @param parent Optional parent widget.
     * @return Pointer to the created widget.
     */
    [[nodiscard]] virtual QWidget* createPreviewWidget(QWidget *parent) = 0;

    /**
     * @brief Shows the file preview content.
     * @param path The file path (for display purposes).
     * @param data The file content data.
     */
    virtual void showPreview(const QString &path, const QByteArray &data) = 0;

    /**
     * @brief Shows a loading indicator.
     * @param path The file path being loaded.
     */
    virtual void showLoading(const QString &path) = 0;

    /**
     * @brief Shows an error message.
     * @param error The error description.
     */
    virtual void showError(const QString &error) = 0;

    /**
     * @brief Clears the preview content.
     */
    virtual void clear() = 0;
};

#endif // FILEPREVIEWSTRATEGY_H
