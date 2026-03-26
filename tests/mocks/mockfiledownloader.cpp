#include "mockfiledownloader.h"

MockFileDownloader::MockFileDownloader(QObject *parent) : IFileDownloader(parent) {}

void MockFileDownloader::download(const QUrl &url)
{
    lastUrl_ = url;
    ++downloadCalls_;
    downloading_ = true;
}

void MockFileDownloader::cancel()
{
    ++cancelCalls_;
    downloading_ = false;
}

bool MockFileDownloader::isDownloading() const
{
    return downloading_;
}

void MockFileDownloader::mockEmitProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void MockFileDownloader::mockEmitFinished(const QByteArray &data)
{
    downloading_ = false;
    emit downloadFinished(data);
}

void MockFileDownloader::mockEmitFailed(const QString &error)
{
    downloading_ = false;
    emit downloadFailed(error);
}

void MockFileDownloader::mockReset()
{
    downloading_ = false;
    downloadCalls_ = 0;
    cancelCalls_ = 0;
    lastUrl_ = QUrl();
}
