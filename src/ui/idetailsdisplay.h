#pragma once
#include <QByteArray>
#include <QString>

/**
 * @brief Interface for displaying file preview details in the details panel.
 *
 * Abstracts the concrete FileDetailsPanel from PreviewCoordinator,
 * enabling the coordinator to be tested in isolation using a mock.
 */
class IDetailsDisplay
{
public:
    virtual ~IDetailsDisplay() = default;

    /**
     * @brief Displays a C64 disk directory listing from raw disk image data.
     * @param data Raw disk image bytes.
     * @param path Remote path of the disk image file.
     */
    virtual void showDiskDirectory(const QByteArray &data, const QString &path) = 0;

    /**
     * @brief Displays SID file metadata and details.
     * @param data Raw SID file bytes.
     * @param path Remote path of the SID file.
     */
    virtual void showSidDetails(const QByteArray &data, const QString &path) = 0;

    /**
     * @brief Displays plain text or HTML file content.
     * @param content UTF-8 decoded text content.
     */
    virtual void showTextContent(const QString &content) = 0;

    /**
     * @brief Displays an error message in the details panel.
     * @param message Human-readable error description.
     */
    virtual void showError(const QString &message) = 0;
};
