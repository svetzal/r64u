#ifndef DATABASEDOWNLOADCONTROLLER_H
#define DATABASEDOWNLOADCONTROLLER_H

#include <QLabel>
#include <QObject>
#include <QProgressBar>
#include <QPushButton>

class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;
class QWidget;

/// Manages the download UI and logic for all metadata databases in Preferences.
class DatabaseDownloadController : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseDownloadController(QWidget *parentWidget, QObject *parent = nullptr);

    /// Creates and returns the databases widget for embedding in PreferencesDialog.
    QWidget *createWidgets();

    void setSonglengthsDatabase(SonglengthsDatabase *database);
    void setHVSCMetadataService(HVSCMetadataService *service);
    void setGameBase64Service(GameBase64Service *service);

private slots:
    void onDownloadDatabase();
    void onDatabaseDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onDatabaseDownloadFinished(int entryCount);
    void onDatabaseDownloadFailed(const QString &error);

    void onDownloadStil();
    void onStilDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onStilDownloadFinished(int entryCount);
    void onStilDownloadFailed(const QString &error);

    void onDownloadBuglist();
    void onBuglistDownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onBuglistDownloadFinished(int entryCount);
    void onBuglistDownloadFailed(const QString &error);

    void onDownloadGameBase64();
    void onGameBase64DownloadProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onGameBase64DownloadFinished(int gameCount);
    void onGameBase64DownloadFailed(const QString &error);

private:
    struct DownloadWidgetGroup
    {
        QPushButton *button = nullptr;
        QProgressBar *progressBar = nullptr;
        QLabel *statusLabel = nullptr;
        QString itemName;  ///< e.g., "HVSC Songlengths database"
        QString unitName;  ///< e.g., "entries"
    };

    void startDownload(DownloadWidgetGroup &group);
    void handleDownloadProgress(DownloadWidgetGroup &group, qint64 bytesReceived,
                                qint64 bytesTotal);
    void handleDownloadFinished(DownloadWidgetGroup &group, int count);
    void handleDownloadFailed(DownloadWidgetGroup &group, const QString &error);

    QWidget *parentWidget_ = nullptr;

    SonglengthsDatabase *songlengthsDatabase_ = nullptr;
    DownloadWidgetGroup dbWidgets_;

    HVSCMetadataService *hvscMetadataService_ = nullptr;
    DownloadWidgetGroup stilWidgets_;
    DownloadWidgetGroup buglistWidgets_;

    GameBase64Service *gameBase64Service_ = nullptr;
    DownloadWidgetGroup gameBase64Widgets_;
};

#endif  // DATABASEDOWNLOADCONTROLLER_H
