#include "previewcoordinator.h"

#include "services/fileactioncore.h"
#include "services/filepreviewservice.h"
#include "services/playlistmanager.h"
#include "ui/filedetailspanel.h"

#include <QFileInfo>
#include <QMessageBox>

PreviewCoordinator::PreviewCoordinator(FilePreviewService *previewService,
                                       FileDetailsPanel *detailsPanel,
                                       PlaylistManager *playlistManager, QObject *parent)
    : QObject(parent), previewService_(previewService), detailsPanel_(detailsPanel),
      playlistManager_(playlistManager)
{
    if (previewService_) {
        connect(previewService_, &FilePreviewService::previewReady, this,
                &PreviewCoordinator::onPreviewReady);
        connect(previewService_, &FilePreviewService::previewFailed, this,
                &PreviewCoordinator::onPreviewFailed);
    }
}

void PreviewCoordinator::onFileContentRequested(const QString &path)
{
    if (previewService_) {
        previewService_->requestPreview(path);
    }
}

void PreviewCoordinator::onPreviewReady(const QString &remotePath, const QByteArray &data)
{
    if (!detailsPanel_) {
        return;
    }

    fileaction::PreviewAction action =
        fileaction::routePreviewData(remotePath, data, playlistManager_ != nullptr);

    std::visit(
        [this](auto &&act) {
            using T = std::decay_t<decltype(act)>;

            if constexpr (std::is_same_v<T, fileaction::ShowDiskDirectory>) {
                detailsPanel_->showDiskDirectory(act.data, act.path);
            } else if constexpr (std::is_same_v<T, fileaction::ShowSidDetails>) {
                detailsPanel_->showSidDetails(act.data, act.path);
                if (act.updatePlaylist && playlistManager_ != nullptr) {
                    playlistManager_->updateDurationFromData(act.path, act.data);
                }
            } else if constexpr (std::is_same_v<T, fileaction::ShowTextContent>) {
                detailsPanel_->showTextContent(act.content);
            }
        },
        action);
}

void PreviewCoordinator::onPreviewFailed(const QString &remotePath, const QString &error)
{
    Q_UNUSED(remotePath)
    if (detailsPanel_) {
        detailsPanel_->showError(error);
    }
}

void PreviewCoordinator::onConfigLoadFinished(const QString &path)
{
    emit statusMessage(tr("Configuration loaded: %1").arg(QFileInfo(path).fileName()), 5000);
}

void PreviewCoordinator::onConfigLoadFailed(const QString &path, const QString &error)
{
    emit statusMessage(tr("Failed to load %1: %2").arg(QFileInfo(path).fileName()).arg(error),
                       5000);
    // Show dialog - need a parent widget. Since we don't have one here, use nullptr
    // (detailsPanel_ parent could be used via its window() accessor)
    QWidget *parentWidget = detailsPanel_ ? detailsPanel_->window() : nullptr;
    QMessageBox::warning(
        parentWidget, tr("Configuration Error"),
        tr("Failed to load configuration file:\n%1\n\nError: %2").arg(path).arg(error));
}
