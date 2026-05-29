#pragma once

#include "services/ifiledownloaderservice.h"

/**
 * @brief Test mock for IFileDownloaderService.
 *
 * Follows the established mock pattern: does not auto-emit signals.
 * Tests drive timing via mockEmit*() methods.
 */
class MockFileDownloaderService : public IFileDownloaderService
{
    Q_OBJECT

public:
    explicit MockFileDownloaderService(QObject *parent = nullptr);

    void download(const QUrl &url) override;
    void cancel() override;
    [[nodiscard]] bool isDownloading() const override;

    // --- Mock control ---
    void mockEmitProgress(qint64 bytesReceived, qint64 bytesTotal);
    void mockEmitFinished(const QByteArray &data);
    void mockEmitFailed(const QString &error);
    void mockReset();

    // --- Inspection ---
    [[nodiscard]] int mockDownloadCallCount() const { return downloadCalls_; }
    [[nodiscard]] int mockCancelCallCount() const { return cancelCalls_; }
    [[nodiscard]] QUrl mockLastUrl() const { return lastUrl_; }
    [[nodiscard]] bool mockIsDownloading() const { return downloading_; }

private:
    bool downloading_ = false;
    int downloadCalls_ = 0;
    int cancelCalls_ = 0;
    QUrl lastUrl_;
};
