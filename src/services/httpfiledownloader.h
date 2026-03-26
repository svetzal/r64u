#pragma once

#include "ifiledownloader.h"

#include <QNetworkAccessManager>

class QNetworkReply;

/**
 * @brief Concrete HTTP file downloader wrapping QNetworkAccessManager.
 *
 * Thin imperative-shell wrapper. Contains no business logic.
 */
class HttpFileDownloader : public IFileDownloader
{
    Q_OBJECT

public:
    explicit HttpFileDownloader(QObject *parent = nullptr);

    void download(const QUrl &url) override;
    void cancel() override;
    [[nodiscard]] bool isDownloading() const override;

private slots:
    void onReplyProgress(qint64 bytesReceived, qint64 bytesTotal);
    void onReplyFinished();

private:
    QNetworkAccessManager *networkManager_ = nullptr;
    QNetworkReply *currentReply_ = nullptr;
};
