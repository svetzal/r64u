#ifndef DATABASEDOWNLOADCONTROLLER_H
#define DATABASEDOWNLOADCONTROLLER_H

#include "databasedownloadview.h"

#include <QObject>

#include <memory>

class SonglengthsDatabase;
class HVSCMetadataService;
class GameBase64Service;
class IMessagePresenter;
class QWidget;

/**
 * @brief Coordinates metadata-database download operations.
 *
 * Receives service events and updates the DatabaseDownloadView accordingly.
 * Does not create or own any UI widgets directly.
 */
class DatabaseDownloadController : public QObject
{
    Q_OBJECT

public:
    explicit DatabaseDownloadController(QWidget *parentWidget, QObject *parent = nullptr);
    ~DatabaseDownloadController() override;

    /// Creates and returns the databases widget for embedding in PreferencesDialog.
    QWidget *createWidgets();

    void setSonglengthsDatabase(SonglengthsDatabase *database);
    void setHVSCMetadataService(HVSCMetadataService *service);
    void setGameBase64Service(GameBase64Service *service);

    /**
     * @brief Injects a message presenter used to show info/warning dialogs.
     *
     * Transfers ownership to this controller. The previous default presenter is
     * replaced and destroyed.
     *
     * @param presenter New presenter; ownership is transferred.
     */
    void setMessagePresenter(std::unique_ptr<IMessagePresenter> presenter);

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
    void startDownload(DownloadWidgetGroup &group);
    void handleDownloadProgress(DownloadWidgetGroup &group, qint64 bytesReceived,
                                qint64 bytesTotal);
    void handleDownloadFinished(DownloadWidgetGroup &group, int count);
    void handleDownloadFailed(DownloadWidgetGroup &group, const QString &error);

    QWidget *parentWidget_ = nullptr;
    DatabaseDownloadView *view_ = nullptr;

    /// Owned message presenter; defaults to QMessageBoxPresenter, replaceable in tests.
    std::unique_ptr<IMessagePresenter> messagePresenter_;

    SonglengthsDatabase *songlengthsDatabase_ = nullptr;
    HVSCMetadataService *hvscMetadataService_ = nullptr;
    GameBase64Service *gameBase64Service_ = nullptr;
};

#endif  // DATABASEDOWNLOADCONTROLLER_H
