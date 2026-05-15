#ifndef PREVIEWCOORDINATOR_H
#define PREVIEWCOORDINATOR_H

#include "services/filetypecore.h"

#include <QModelIndex>
#include <QObject>
#include <QString>

class FilePreviewService;
class IDetailsDisplay;
class PlaylistService;
class RemoteFileModel;

class PreviewCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit PreviewCoordinator(FilePreviewService *previewService, IDetailsDisplay *detailsPanel,
                                PlaylistService *playlistService, QObject *parent = nullptr);

    /**
     * @brief Provides the model used to query file details for selection changes.
     * @param model The remote file model (may be null; not owned).
     */
    void setRemoteFileModel(RemoteFileModel *model);

    /**
     * @brief Updates the details panel for the currently selected model index.
     *
     * If the index is invalid or refers to a directory, the panel is cleared.
     * @param index The current selection index from the tree view.
     */
    void onSelectionChanged(const QModelIndex &index);

public slots:
    void onFileContentRequested(const QString &path);
    void onConfigLoadFinished(const QString &path);
    void onConfigLoadFailed(const QString &path, const QString &error);

signals:
    void statusMessage(const QString &message, int timeout = 0);

private slots:
    void onPreviewReady(const QString &remotePath, const QByteArray &data);
    void onPreviewFailed(const QString &remotePath, const QString &error);

private:
    FilePreviewService *previewService_ = nullptr;
    IDetailsDisplay *detailsPanel_ = nullptr;
    PlaylistService *playlistService_ = nullptr;
    RemoteFileModel *remoteFileModel_ = nullptr;
};

#endif  // PREVIEWCOORDINATOR_H
