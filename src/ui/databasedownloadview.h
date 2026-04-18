#ifndef DATABASEDOWNLOADVIEW_H
#define DATABASEDOWNLOADVIEW_H

#include <QLabel>
#include <QObject>
#include <QProgressBar>
#include <QPushButton>
#include <QWidget>

/// Holds the UI widgets for a single downloadable database entry.
struct DownloadWidgetGroup
{
    QPushButton *button = nullptr;
    QProgressBar *progressBar = nullptr;
    QLabel *statusLabel = nullptr;
    QString itemName;  ///< e.g., "HVSC Songlengths database"
    QString unitName;  ///< e.g., "entries"
};

/**
 * @brief Builds and owns the database download UI panel.
 *
 * Responsible solely for creating and exposing the widgets used by
 * DatabaseDownloadController. Carries no download logic.
 */
class DatabaseDownloadView : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseDownloadView(QObject *parent = nullptr);

    /// Creates and returns the root container widget. Caller does not take ownership
    /// (Qt parent chain manages lifetime once the widget is parented).
    QWidget *createWidget();

    /// @name Widget group accessors
    /// @{
    [[nodiscard]] DownloadWidgetGroup &songlengths() { return dbWidgets_; }
    [[nodiscard]] DownloadWidgetGroup &stil() { return stilWidgets_; }
    [[nodiscard]] DownloadWidgetGroup &buglist() { return buglistWidgets_; }
    [[nodiscard]] DownloadWidgetGroup &gameBase64() { return gameBase64Widgets_; }
    /// @}

private:
    DownloadWidgetGroup dbWidgets_;
    DownloadWidgetGroup stilWidgets_;
    DownloadWidgetGroup buglistWidgets_;
    DownloadWidgetGroup gameBase64Widgets_;
};

#endif  // DATABASEDOWNLOADVIEW_H
