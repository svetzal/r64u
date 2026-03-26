#include "httpfiledownloader.h"

#include <QNetworkReply>
#include <QNetworkRequest>

HttpFileDownloader::HttpFileDownloader(QObject *parent)
    : IFileDownloader(parent), networkManager_(new QNetworkAccessManager(this))
{
}

void HttpFileDownloader::download(const QUrl &url)
{
    if (currentReply_ != nullptr) {
        return;
    }

    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("r64u/1.0"));
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute,
                         QNetworkRequest::NoLessSafeRedirectPolicy);

    currentReply_ = networkManager_->get(request);

    connect(currentReply_, &QNetworkReply::downloadProgress, this,
            &HttpFileDownloader::onReplyProgress);
    connect(currentReply_, &QNetworkReply::finished, this, &HttpFileDownloader::onReplyFinished);
}

void HttpFileDownloader::cancel()
{
    if (currentReply_ == nullptr) {
        return;
    }
    currentReply_->abort();
    currentReply_->deleteLater();
    currentReply_ = nullptr;
}

bool HttpFileDownloader::isDownloading() const
{
    return currentReply_ != nullptr;
}

void HttpFileDownloader::onReplyProgress(qint64 bytesReceived, qint64 bytesTotal)
{
    emit downloadProgress(bytesReceived, bytesTotal);
}

void HttpFileDownloader::onReplyFinished()
{
    if (currentReply_ == nullptr) {
        return;
    }

    QNetworkReply *reply = currentReply_;
    currentReply_ = nullptr;

    if (reply->error() != QNetworkReply::NoError) {
        const QString error = reply->errorString();
        reply->deleteLater();
        emit downloadFailed(error);
        return;
    }

    const QByteArray data = reply->readAll();
    reply->deleteLater();

    if (data.isEmpty()) {
        emit downloadFailed(tr("Downloaded file is empty"));
        return;
    }

    emit downloadFinished(data);
}
