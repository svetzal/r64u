#ifndef SCREENSHOTSERVICE_H
#define SCREENSHOTSERVICE_H

#include <QObject>
#include <QString>

class QImage;

/**
 * @brief Saves a video frame as a timestamped PNG file.
 *
 * Handles directory creation, filename generation, and file I/O.
 * Returns the base filename on success or an empty string on failure.
 */
class ScreenshotService : public QObject
{
    Q_OBJECT

public:
    explicit ScreenshotService(QObject *parent = nullptr);

    /**
     * @brief Captures a single frame as a PNG file.
     *
     * Creates @p outputDir if it does not exist.
     *
     * @param frame The image to save.
     * @param outputDir Target directory (absolute path).
     * @return The base filename (not the full path) on success, or an empty string on failure.
     */
    [[nodiscard]] QString capture(const QImage &frame, const QString &outputDir);
};

#endif  // SCREENSHOTSERVICE_H
