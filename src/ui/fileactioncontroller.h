#ifndef FILEACTIONCONTROLLER_H
#define FILEACTIONCONTROLLER_H

#include "services/errorhandler.h"
#include "services/filetypecore.h"

#include <QList>
#include <QObject>
#include <QPair>
#include <QString>

class DeviceConnection;
class DeviceActionService;
class ConfigFileLoaderService;
class DiskBootSequenceService;
class PlaylistService;
class RemoteFileModel;
class StreamingService;
class QAction;
class QTreeView;

class FileActionController : public QObject
{
    Q_OBJECT

public:
    explicit FileActionController(DeviceActionService *deviceActionService,
                                  DeviceConnection *connection,
                                  ConfigFileLoaderService *configLoader, QObject *parent = nullptr);

    void setStreamingService(StreamingService *manager);
    void setPlaylistService(PlaylistService *manager);
    void setErrorHandler(ErrorHandler *handler);
    void setActions(QAction *play, QAction *run, QAction *mount);

    /**
     * @brief Provides the selection source so actions can query the current
     *        path and file type without the caller re-querying the model.
     * @param view   The tree view (may be null).
     * @param model  The remote file model (may be null).
     */
    void setSelectionSource(QTreeView *view, RemoteFileModel *model);

    void updateActionStates(filetype::FileType type, bool canOperate);

public slots:
    void play(const QString &path, filetype::FileType type);
    void run(const QString &path, filetype::FileType type);
    void mountToDrive(const QString &path, const QString &drive);
    void loadConfig(const QString &path, filetype::FileType type);
    void download(const QString &path);
    void addToPlaylist(const QList<QPair<QString, filetype::FileType>> &items);

    /**
     * @brief Selection-source variants — query treeView_/remoteFileModel_ internally.
     *
     * These slots emit statusMessage(tr("File browser not ready")) if the selection
     * source has not been configured or is null.  They allow connections in
     * ExplorePanel::setupConnections() to be written as single-line delegates.
     */
    void playSelection();
    void runSelection();
    void loadConfigSelection();
    void addToPlaylistSelection();

signals:
    void statusMessage(const QString &message, int timeout = 0);

private:
    [[nodiscard]] bool validateFileOperation(const QString &path);
    void runDiskImage(const QString &path);
    void ensureStreamingStarted();

    [[nodiscard]] bool hasSelectionSource() const;
    [[nodiscard]] QString selectionPath() const;
    [[nodiscard]] filetype::FileType selectionFileType() const;

    DeviceActionService *deviceActionService_ = nullptr;
    DeviceConnection *deviceConnection_ = nullptr;
    ConfigFileLoaderService *configFileLoader_ = nullptr;
    StreamingService *streamingService_ = nullptr;
    PlaylistService *playlistService_ = nullptr;
    DiskBootSequenceService *bootService_ = nullptr;
    ErrorHandler *errorHandler_ = nullptr;

    // Selection source (not owned)
    QTreeView *selectionView_ = nullptr;
    RemoteFileModel *selectionModel_ = nullptr;

    QAction *playAction_ = nullptr;
    QAction *runAction_ = nullptr;
    QAction *mountAction_ = nullptr;
};

#endif  // FILEACTIONCONTROLLER_H
