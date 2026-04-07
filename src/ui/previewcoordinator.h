#ifndef PREVIEWCOORDINATOR_H
#define PREVIEWCOORDINATOR_H

#include <QObject>
#include <QString>

class FilePreviewService;
class FileDetailsPanel;
class PlaylistManager;

class PreviewCoordinator : public QObject
{
    Q_OBJECT

public:
    explicit PreviewCoordinator(FilePreviewService *previewService, FileDetailsPanel *detailsPanel,
                                PlaylistManager *playlistManager, QObject *parent = nullptr);

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
    FileDetailsPanel *detailsPanel_ = nullptr;
    PlaylistManager *playlistManager_ = nullptr;
};

#endif  // PREVIEWCOORDINATOR_H
