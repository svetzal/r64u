#include "mockfiledownloaderservice.h"

MockFileDownloaderService::MockFileDownloaderService(QObject *parent)
    : IFileDownloaderService(parent)
{
}

void MockFileDownloaderService::download(const QUrl &url)
{
    lastUrl_ = url;
    ++downloadCalls_;
    downloading_ = true;
}

void MockFileDownloaderService::cancel()
{
    ++cancelCalls_;
    downloading_ = false;
}

bool MockFileDownloaderService::isDownloading() const
{
    return downloading_;
}

void MockFileDownloaderService::mockEmitProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void MockFileDownloaderService::mockEmitFinished(const QByteArray &data)
{
    downloading_ = false;
    emit downloadFinished(data);
}

void MockFileDownloaderService::mockEmitFailed(const QString &error)
{
    downloading_ = false;
    emit downloadFailed(error);
}

void MockFileDownloaderService::mockReset()
{
    downloading_ = false;
    downloadCalls_ = 0;
    cancelCalls_ = 0;
    lastUrl_ = QUrl();
}
